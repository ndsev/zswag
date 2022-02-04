import copy
import os
import random

import openapi_spec_validator.exceptions
import yaml
import sys
import inspect
import tempfile
import importlib
from typing import Optional, Dict, Tuple, List, IO
from argparse import ArgumentParser, FileType, RawTextHelpFormatter
from subprocess import CalledProcessError
import dataclasses as dc
from copy import deepcopy
from enum import Enum
from zserio.typeinfo import MemberAttribute
from openapi_spec_validator import validate_spec
import uuid
import zserio
from pyzswagcl import \
    ZSERIO_OBJECT_CONTENT_TYPE, \
    ZSERIO_REQUEST_PART_WHOLE, \
    ZSERIO_REQUEST_PART, \
    parse_openapi_config

from .reflect import \
    service_method_request_type, \
    rgetattr, \
    check_uninstantiable, \
    cached_type_info, \
    members as type_members, \
    find_field, \
    is_scalar
from .doc import get_doc_str, IdentType, md_filter_definition


class HttpParamLocation(Enum):
    QUERY = "query"
    BODY = "body"
    PATH = "path"
    HEADER = "header"


class HttpParamFormat(Enum):
    STRING = "string"
    BINARY = "binary"
    BYTE = "byte"
    BASE64 = "base64"
    BASE64URL = "base64url"
    HEX = "hex"


HTTP_METHOD_TAGS = ("get", "put", "post", "delete")
FLATTEN_TAG = "flat"
BLOB_TAG = "blob"
SECURITY_ASSIGNMENT_TAG = "security="
PATH_ASSIGNMENT_TAG = "path="
WILDCARD_CONFIG = "*"


def argdoc(s: str):
    return "\n"+inspect.cleandoc(s)+"\n\n"


def less_indent_formatter(prog):
    return RawTextHelpFormatter(prog, max_help_position=8, width=80)


@dc.dataclass
class ParamSpecifier:
    """"Can be used to annotate a method config with a desired
    location, format and parameter name for a request-part field."""
    request_part: str            # e.g. "request_member.subfield"
    name: Optional[str]          # e.g. "subfieldParam"
    location: HttpParamLocation  # e.g. QUERY
    format: HttpParamFormat        # e.g. BASE64
    style: Optional[str] = None
    explode: Optional[str] = None


@dc.dataclass
class MethodConfig:
    """User-specified set of parameters to control how a zserio service method
    is converted into an OpenAPI-file entry."""
    name: str
    http_method: str = "post"
    param_loc: Optional[HttpParamLocation] = None  # None -> Whole request blob in body
    flatten: bool = False
    param_specifiers: Optional[List[ParamSpecifier]] = dc.field(default_factory=list)
    security: Optional[str] = None
    path: Optional[str] = None
    openapi_docstring: str = ""
    openapi_return_type: str = ""
    openapi_arg_type: str = "Unknown"
    openapi_result_doc: str = ""
    openapi_arg_doc: str = ""
    openapi_parameters: dict = dc.field(default_factory=dict)


class OpenApiGenError(RuntimeError):
    """Raised by OpenApiSchemaGenerator when something goes wrong."""
    def __init__(self, what: str):
        super(OpenApiGenError, self).__init__(what)


class OpenApiSchemaGenerator:

    def __init__(self, *,
                 service: str,
                 path: str,
                 package: Optional[str] = None,
                 config: Optional[List[str]] = None,
                 output: IO,
                 base_config: Optional[IO],
                 zserio_src_root: Optional[str]):

        # Process service name and package path
        self.service_name = service
        self.zs_pkg_path = zserio_src_root
        service_name_parts = service.split(".") + ["Service"]

        if os.path.isdir(path):
            # Generate OpenAPI from existing Python code
            sys.path.append(path)
            python_module = importlib.import_module(f"{service_name_parts[0]}.api")
            self.zs_pkg_path = None
        else:
            if self.zs_pkg_path is None:
                self.zs_pkg_path = os.path.abspath(os.path.dirname(path))
                path = os.path.basename(path)
            full_path = os.path.join(self.zs_pkg_path, path)
            if not os.path.isfile(full_path):
                raise OpenApiGenError(f"The path '{full_path}' is neither a zserio file nor a pythonpath.")
            # Generate OpenAPI from zserio code. Must generate
            # intermediate Python source to inspect service.
            if package is None:
                package = f"zswag_gen_{uuid.uuid1().hex}"
            if service_name_parts[0] != package:
                service_name_parts = [package] + service_name_parts
            try:
                gen_dir = tempfile.mkdtemp("zswag.gen")
                python_module = zserio.generate(
                    zs_dir=self.zs_pkg_path,
                    main_zs_file=path,
                    top_level_package=package,
                    gen_dir=gen_dir,
                    extra_args=["-withTypeInfoCode"])
            except CalledProcessError as e:
                raise OpenApiGenError(f"Failed to parse zserio sources:\n{e.stderr}")
        if not python_module:
            raise OpenApiGenError(f"Could not import {service_name_parts[0]}.api!")
        self.service_type = rgetattr(python_module, ".".join(service_name_parts[1:]))
        self.service_instance = self.service_type()

        # Process method config tags ...
        self.config: Dict[str, MethodConfig] = dict()
        self.config[WILDCARD_CONFIG] = default_entry = MethodConfig(WILDCARD_CONFIG)
        self.output = output
        self.base_config = dict()
        # ... first load base-config.
        if base_config:
            self.base_config = yaml.load(base_config, yaml.Loader)
        if methods_base_config := self.base_config.get("methods", None):
            # If there are wildcard base-config tags, process them first.
            if default_config_tags := methods_base_config.get(WILDCARD_CONFIG, None):
                default_entry = self.add_method_config_from_tags(WILDCARD_CONFIG, default_config_tags, default_entry)
            # Then process tags for other methods from the base-config.
            for method_name, tags in ((m, t) for m, t in methods_base_config.items() if m != WILDCARD_CONFIG):
                self.add_method_config_from_tags(method_name, tags, default_entry)
        # ... then process additional tags from the command line.
        if config:
            for entry in config:
                method_name = WILDCARD_CONFIG
                parts = entry.split(":")
                if len(parts) > 1:
                    method_name = parts[0]
                tag_list = [tag.strip() for tag in parts[-1].split(",")]
                default_entry = self.add_method_config_from_tags(method_name, tag_list, default_entry)

    def add_method_config_from_tags(
            self,
            method_name: str,
            tag_list: List[str],
            default_entry: MethodConfig) -> MethodConfig:
        new_config = deepcopy(default_entry)
        new_config.name = method_name
        # Make sure that HTTP method tags are always processed first!
        tag_list.sort(key=lambda x: x in HTTP_METHOD_TAGS, reverse=True)
        for tag in tag_list:
            if tag in HTTP_METHOD_TAGS:
                new_config.http_method = tag
            elif any(tag == loc.value for loc in HttpParamLocation):
                new_config.param_loc = HttpParamLocation(tag)
                if new_config.param_loc == HttpParamLocation.BODY and new_config.http_method == "get":
                    raise OpenApiGenError(f"Cannot use `body` tag with HTTP GET.")
            elif tag == FLATTEN_TAG:
                new_config.flatten = True
            elif tag == BLOB_TAG:
                new_config.flatten = False
            elif tag.startswith(SECURITY_ASSIGNMENT_TAG):
                new_config.security = tag[len(SECURITY_ASSIGNMENT_TAG):]
            elif tag.startswith(PATH_ASSIGNMENT_TAG):
                if method_name == WILDCARD_CONFIG:
                    raise OpenApiGenError(f"Refusing to apply '{tag}' to ALL methods. Likely not on purpose.")
                else:
                    new_config.path = tag[len(PATH_ASSIGNMENT_TAG):]
            elif "?" in tag:
                try:
                    request_part, specifiers_part = tag.split("?")
                    spec = dict(specifier.split("=") for specifier in specifiers_part.split("&"))
                    par_loc = HttpParamLocation(spec.get("in"))
                    if par_loc == HttpParamLocation.BODY:
                        if new_config.http_method == "get":
                            raise OpenApiGenError("Cannot use `location=body` with HTTP GET.")
                        par_format = HttpParamFormat.BINARY
                        par_name = "body"
                    else:
                        par_name, par_format = spec.get("name"), HttpParamFormat(spec.get("format", "string"))
                    if any(par.name == par_name for par in new_config.param_specifiers):
                        raise OpenApiGenError(f"Encountered duplicate use of parameter name '{par_name}' in the same method!")
                    new_config.param_specifiers.append(ParamSpecifier(request_part, par_name, par_loc, par_format))
                    # Process style and explode entries
                    if style := spec.get("style", None):
                        new_config.param_specifiers[-1].style = style.lower()
                    if explode := spec.get("explode", None):
                        new_config.param_specifiers[-1].explode = explode.lower()
                except (ValueError, IndexError) as e:
                    raise OpenApiGenError(f"Encountered malformed parameter specifier tag '{tag}' ({e})!")
                except KeyError as missing_key:
                    raise OpenApiGenError(f"Parameter specifier tag {tag} has no value for '{missing_key}' key.")
            else:
                raise OpenApiGenError(f"Did not understand tag '{tag}'.")
        if method_name in self.config and method_name != WILDCARD_CONFIG:
            print(f"[WARNING] Overwriting config for method {method_name}!")
        self.config[method_name] = new_config
        if method_name == WILDCARD_CONFIG:
            default_entry = new_config
        return default_entry

    def config_for_method(self, method_name: str) -> MethodConfig:
        if method_name in self.config:
            return self.config[method_name]
        else:
            result = copy.deepcopy(self.config[WILDCARD_CONFIG])
            result.name = method_name
            return result

    def generate(self):
        service_name_parts = self.service_instance.service_full_name.split(".")
        schema = {
            "openapi": "3.0.0",
            "info": self.base_config.get("info", {
                "title": ".".join(service_name_parts[1:]),
                "description": md_filter_definition(get_doc_str(
                    ident_type=IdentType.SERVICE,
                    pkg_path=self.zs_pkg_path,
                    ident=self.service_instance.service_full_name,
                    fallback=[f"REST API for {'.'.join(service_name_parts[1:])} service."]
                )[0]),
                "contact": {
                    "email": "TODO@TODO.TODO"
                },
                "license": {
                    "name": "TODO"
                },
                "version": "TODO",
            }),
            "servers": self.base_config.get("servers", []),
            "paths": {
                method_info.path: {
                    method_info.http_method: {
                        "summary": method_info.openapi_docstring,
                        "description": method_info.openapi_docstring,
                        "operationId": method_info.name,
                        **method_info.openapi_parameters,
                        "responses": {
                            "200": {
                                "description": method_info.openapi_result_doc,
                                "content": {
                                    ZSERIO_OBJECT_CONTENT_TYPE: {
                                        "schema": {
                                            "type": "string",
                                            "format": "binary"
                                        }
                                    }
                                }
                            }
                        }
                    },
                } for method_info in (
                    self.process_method_config(method_name)
                    for method_name in self.service_instance.method_names)
            }
        }
        if security_schemes := self.base_config.get("securitySchemes", None):
            schema["components"] = {
                "securitySchemes": security_schemes
            }
        if security := self.base_config.get("security", None):
            schema["security"] = security
        print(f"[INFO] Validating with openapi-spec-validator ... ", end="")
        try:
            validate_spec(schema)
        except openapi_spec_validator.exceptions.OpenAPIValidationError as e:
            print()
            raise OpenApiGenError(f"Spec is invalid: {e}")
        print("OK")
        print(f"[INFO] Writing to '{self.output.name}' ... ", end="")
        yaml.dump(schema, self.output, default_flow_style=False)
        self.output.close()
        print("OK")
        if os.path.isfile(self.output.name):
            print(f"[INFO] Validating with zswag parser ... ", end="")
            try:
                parse_openapi_config(self.output.name)
            except RuntimeError as e:
                print()
                raise OpenApiGenError(f"Spec is invalid: {e}")
            print("OK")
        else:
            print(f"[INFO] Skipping zswag parser validation.")
        print(f"[INFO] Done.")

    def process_method_config(self, method_name: str) -> MethodConfig:
        result = self.config_for_method(method_name)
        req_t = service_method_request_type(self.service_instance, result.name)
        req_t_info = cached_type_info(req_t)
        assert req_t_info
        method_ident = f"{self.service_instance.service_full_name}.{method_name}"
        if not result.path:
            print(f"[INFO] Auto-generating path for `{method_ident}`.")
            result.path = f"/{method_name}"
        # Generate doc-strings
        if self.zs_pkg_path:
            doc_strings = get_doc_str(
                ident_type=IdentType.RPC,
                pkg_path=self.zs_pkg_path,
                ident=method_ident)
            if doc_strings:
                result.openapi_docstring = doc_strings[0]
                result.openapi_return_type = doc_strings[1]
                result.openapi_arg_type = doc_strings[2]
                result.openapi_result_doc = md_filter_definition(get_doc_str(
                    ident_type=IdentType.STRUCT,
                    pkg_path=self.zs_pkg_path,
                    ident=result.openapi_return_type,
                    fallback=[f"### struct {result.openapi_return_type}"])[0])
                result.openapi_arg_doc = md_filter_definition(get_doc_str(
                    ident_type=IdentType.STRUCT,
                    pkg_path=self.zs_pkg_path,
                    ident=result.openapi_arg_type,
                    fallback=[f"### struct {result.openapi_arg_type}"])[0])
        # Convert simple legacy instructions into parameter specifiers
        if not result.param_specifiers:
            if result.param_loc == HttpParamLocation.BODY or (not result.param_loc and not result.flatten):
                result.param_specifiers.append(
                    ParamSpecifier(ZSERIO_REQUEST_PART_WHOLE, "", HttpParamLocation.BODY, HttpParamFormat.BINARY))
            elif result.flatten:
                if not_instantiable_reason := check_uninstantiable(req_t_info):
                    raise OpenApiGenError(str(not_instantiable_reason))
                for field_name, member_info in type_members(req_t_info, True):
                    if is_scalar(member_info.type_info):
                        result.param_specifiers.append(
                            ParamSpecifier(
                                field_name, field_name.replace(".", "__"),
                                result.param_loc or HttpParamLocation.QUERY,
                                HttpParamFormat.STRING))
            elif result.param_loc:
                result.param_specifiers.append(
                    ParamSpecifier(
                        ZSERIO_REQUEST_PART_WHOLE, "requestBody", result.param_loc, HttpParamFormat.BINARY))
        # Convert MethodConfig.param_specifiers to MethodConfig.openapi_parameters
        self.process_method_parameters(result)
        return result

    def process_method_parameters(self, config: MethodConfig):
        req_t = service_method_request_type(self.service_instance, config.name)
        req_t_info = cached_type_info(req_t)
        # Convert parameter specifiers to parameters-JSON
        openapi_param_list = []
        for param_specifier in config.param_specifiers:
            # Easy - parameter only indicates binary transfer of the
            # request object in the body
            if param_specifier.location == HttpParamLocation.BODY:
                config.openapi_parameters["requestBody"] = {
                    "description": config.openapi_arg_doc,
                    "content": {
                        ZSERIO_OBJECT_CONTENT_TYPE: {
                            "schema": {
                                "type": "string"
                            }
                        }
                    }
                }
                continue
            # Fallthrough - the parameter has a name, a format and a request part
            # Process type-info first and determine if the field even exists.
            openapi_schema_info = {
                "type": "string",
                "format": param_specifier.format.name.lower(),
            }
            if param_specifier.request_part != ZSERIO_REQUEST_PART_WHOLE:
                _, member_info = find_field(req_t_info, param_specifier.request_part)
                if not member_info:
                    raise OpenApiGenError(f"Could not find field '{param_specifier.request_part}' in {req_t_info.schema_name}!")
                # If the member is an array, we must indicate this in OpenAPI
                if MemberAttribute.ARRAY_LENGTH in member_info.attributes:
                    openapi_schema_info.update({
                        "type": "array",
                        "items": {
                            "type": "string"
                        }
                    })
            # If the parameter is to be placed in the path, make sure there's a placeholder
            if param_specifier.location == HttpParamLocation.PATH:
                placeholder = f"{{{param_specifier.name}}}"
                if placeholder not in config.path:
                    print(f"[INFO] Appending /{placeholder} to path for {config.name}")
                    config.path += f"/{placeholder}"
            # Append the OpenAPI information for the new parameter
            openapi_param_list.append({
                "in": param_specifier.location.value.lower(),
                "name": param_specifier.name,
                "description": config.openapi_arg_doc,
                "required": True,
                ZSERIO_REQUEST_PART: param_specifier.request_part,
                **({"allowEmptyValue": True} if param_specifier.location == HttpParamLocation.QUERY else {}),
                "schema": openapi_schema_info
            })
            # Add style and explode hints
            if param_specifier.style:
                openapi_param_list[-1]["style"] = param_specifier.style
            if param_specifier.explode:
                openapi_param_list[-1]["explode"] = param_specifier.explode == "true"
        # Process security scheme
        if config.security is not None:
            if config.security == "":
                print(f"[WARNING] {config.name} has explicit empty security setting.")
                config.openapi_parameters["security"] = []
            else:
                config.openapi_parameters["security"] = [{config.security: []}]
        # Set the finalized parameter list
        config.openapi_parameters["parameters"] = openapi_param_list


if __name__ == "__main__":

    parser = ArgumentParser(
        "Zserio OpenApi YAML Generator",
        formatter_class=less_indent_formatter)
    parser.add_argument("-s", "--service", nargs=1, required=True,
                        metavar="service-identifier", help=argdoc("""
                        Fully qualified zserio service identifier.
                        
                        Example:
                            -s my.package.ServiceClass
                        """))
    parser.add_argument("-i", "--input", nargs=1, metavar="zserio-or-python-path",
                        required=True, help=argdoc("""
                        Can be either ...
                        (A) Path to a zserio .zs file. Must be either a top-
                            level entrypoint (e.g. all.zs), or a subpackage
                            (e.g. services/myservice.zs) in conjunction with
                            a "--zserio-source-root|-r <dir>" argument.
                        (B) Path to parent dir of a zserio Python package.
                        
                        Examples:
                            -i path/to/schema/main.zs         (A)
                            -i path/to/python/package/parent  (B) 
                        """))
    parser.add_argument("-r", "--zserio-source-root", nargs=1, metavar="zserio-src-root-dir",
                        required=False, help=argdoc("""
                        When -i specifies a zs file (Option A), indicate the
                        directory for the zserio -src directory argument. If
                        not specified, the parent directory of the zs file
                        will be used.
                        """))
    parser.add_argument("-p", "--package", nargs=1, metavar="top-level-package",
                        required=False, help=argdoc("""
                        When -i specifies a zs file (Option A), indicate
                        that a specific top-level zserio package name
                        should be used.
                        
                        Examples:
                            -p zserio_pkg_name
                        """))
    parser.add_argument("-c", "--config", nargs="+", metavar="tags",
                        action="append", help=argdoc("""
                        Configuration tags for a specific or all methods.
                        The argument syntax follows this pattern:
                            
                           [(service-method-name):](comma-separated-tags)
                            
                        Note: The -c argument may be applied multiple times.
                        The `comma-separated-tags` must be a list of tags
                        which indicate OpenApi method generator preferences.
                        The following tags are supported:
                        
                        get|put|post|delete : HTTP method tags
                                query|path| : Parameter location tags
                                header|body 
                                  flat|blob : Flatten request object,
                                              or pass it as whole blob.
                          (param-specifier) : Specify parameter name, format
                                              and location for a specific
                                              request-part. See below.
                            security=(name) : Set a particular security
                                              scheme to be used. The scheme
                                              details must be provided through
                                              the --base-config-yaml.
                         path=(method-path) : Set a particular method path.
                                              May contain placeholders for
                                              path params.
                                                   
                        A (param-specifier) tag has the following schema:
                        
                            (field?name=...
                                  &in=[path|body|query|header]
                                  &format=[binary|base64|hex]
                                  [&style=...]
                                  [&explode=...])
                        
                        Examples:
                        
                          Expose all methods as POST, but `getLayerByTileId`
                          as GET with flat path-parameters:
                          
                            `-c post getLayerByTileId:get,flat,path`
                            
                          For myMethod, put the whole request blob into the a
                          query "data" parameter as base64:
                          
                            `-c myMethod:*?name=data&in=query&format=base64`
                            
                          For myMethod, set the "AwesomeAuth" auth scheme:
                          
                            `-c myMethod:security=AwesomeAuth`
                            
                          For myMethod, provide the path and place myField
                          explicitely in a path placeholder:
                          
                            `-c 'myMethod:path=/my-method/{param},...
                                 myField?name=param&in=path&format=string'`
                            
                        Note:
                            * The HTTP-method defaults to `post`.
                            * The parameter 'in' defaults to `query` for
                              `get`, `body` otherwise.
                            * If a method uses a parameter specifier, the
                              `flat`, `body`, `query`, `path`, `header` and
                              `body`-tags are ignored.
                            * The `flat` tag is only meaningful in conjunction
                              with `query` or `path`.
                            * An unspecific tag list (no service-method-name)
                              affects the defaults only for following, not
                              preceding specialized tag assignments.
                        """))
    parser.add_argument("-o", "--output", nargs=1, type=FileType("w"), default=[sys.stdout],
                        metavar="output", help=argdoc("""
                        Output file path. If not specified, the output will be
                        written to stdout.
                        """))
    parser.add_argument("-b", "--base-config-yaml", nargs=1, type=FileType("r"), required=False, default=[None],
                        help=argdoc("""
                        Base configuration file. Can be used to fully or partially
                        substitute --config arguments, and to provide additional
                        OpenAPI information. The YAML file must look like this:
                        
                          method: # Optional method tags dictionary
                            <method-name|*>: <list of config tags>
                          securitySchemes: ... # Optional OpenAPI securitySchemes
                          info: ...            # Optional OpenAPI info section
                          servers: ...         # Optional OpenAPI servers section
                          security: ...        # Optional OpenAPI global security
                        """))

    args = parser.parse_args(sys.argv[1:])
    try:
        OpenApiSchemaGenerator(
            service=args.service[0],
            path=args.input[0] if args.input else None,
            package=args.package[0] if args.package else None,
            config=[arg for args in args.config for arg in args] if args.config else [],
            output=args.output[0],
            base_config=args.base_config_yaml[0] if args.base_config_yaml else None,
            zserio_src_root=args.zserio_source_root[0] if args.zserio_source_root else None).generate()
    except OpenApiGenError as e:
        print(f"[ERROR] {e}")
        exit(1)
