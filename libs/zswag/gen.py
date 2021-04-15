import os

import yaml
import sys
import inspect
import tempfile
import importlib
from typing import Optional, Dict, Tuple, List, IO
from argparse import ArgumentParser, FileType, RawTextHelpFormatter
from subprocess import CalledProcessError

import zserio
from pyzswagcl import \
    OAParamLocation, \
    ZSERIO_OBJECT_CONTENT_TYPE, \
    ZSERIO_REQUEST_PART_WHOLE, \
    ZSERIO_REQUEST_PART

from .reflect import \
    make_instance_and_typeinfo, \
    service_method_request_type, \
    rgetattr, \
    UnsupportedParameterError
from .doc import get_doc_str, IdentType, md_filter_definition

HTTP_METHOD_TAGS = ("get", "put", "post", "patch", "delete")
PARAM_LOCATION_QUERY_TAG = "query"
PARAM_LOCATION_BODY_TAG = "body"
PARAM_LOCATION_PATH_TAG = "path"
PARAM_LOCATION_TAGS = (PARAM_LOCATION_QUERY_TAG, PARAM_LOCATION_PATH_TAG, PARAM_LOCATION_BODY_TAG)
FLATTEN_TAG = "flat"
BLOB_TAG = "blob"


def argdoc(s: str):
    return "\n"+inspect.cleandoc(s)+"\n\n"


def less_indent_formatter(prog):
    return RawTextHelpFormatter(prog, max_help_position=8, width=80)


class MethodSchemaInfo:
    """
    (Private) Return value of Server._method_info()
    """
    def __init__(self, *, name):
        self.name = name
        self.docstring = ""
        self.returntype = ""
        self.argtype = "Unknown"
        self.returndoc = ""
        self.argdoc = ""
        self.path = f"/{name}"
        self.parameters = dict()
        self.http_method = "post"


class OpenApiSchemaGenerator:

    def __init__(self, *,
                 service: str,
                 path: str,
                 package: Optional[str] = None,
                 config: Optional[List[str]] = None,
                 output: IO):
        self.service_name = service
        self.zs_pkg_path = None
        service_name_parts = service.split(".") + ["Service"]
        python_module = None
        if os.path.isdir(path):
            sys.path.append(path)
            python_module = importlib.import_module(f"{service_name_parts[0]}.api")
        elif os.path.isfile(path):
            self.zs_pkg_path = os.path.abspath(os.path.dirname(path))
            try:
                python_module = zserio.generate(
                    zs_dir=self.zs_pkg_path,
                    main_zs_file=os.path.basename(path),
                    top_level_package=package,
                    gen_dir=tempfile.mkdtemp("zswag.gen"))
            except CalledProcessError as e:
                print(f"Failed to parse zserio sources:\n{e.stderr}")
                exit(1)
        if not python_module:
            print(f"ERROR: Could not import {service_name_parts[0]}.api!")
            exit(1)
        self.service_type = rgetattr(python_module, ".".join(service_name_parts[1:]))
        self.service_instance = self.service_type()
        self.config: Dict[str, Tuple[str, Optional[OAParamLocation], bool]] = dict()
        self.config["*"] = default_entry = ("post", None, False)
        self.output = output
        if config:
            for entry in config:
                entry_http_method, entry_param_loc, entry_flatten = default_entry
                method_name = "*"
                parts = entry.split(":")
                if len(parts) > 1:
                    method_name = parts[0]
                tag_list = [tag.strip() for tag in parts[-1].lower().split(",")]
                for tag in tag_list:
                    if tag in HTTP_METHOD_TAGS:
                        entry_http_method = tag
                    elif tag == PARAM_LOCATION_PATH_TAG:
                        entry_param_loc = OAParamLocation.PATH
                    elif tag == PARAM_LOCATION_QUERY_TAG:
                        entry_param_loc = OAParamLocation.QUERY
                    elif tag == PARAM_LOCATION_BODY_TAG:
                        entry_param_loc = None
                        if entry_http_method == "get":
                            print(f"WARNING: Changing HTTP method from get to post due to `body` tag.")
                            entry_http_method = "post"
                    elif tag == FLATTEN_TAG:
                        entry_flatten = True
                    elif tag == BLOB_TAG:
                        entry_flatten = False
                entry = (entry_http_method, entry_param_loc, entry_flatten)
                if method_name == "*":
                    default_entry = entry
                self.config[method_name] = entry

    def config_for_method(self, method_name: str) -> Tuple[str, OAParamLocation, bool]:
        if method_name in self.config:
            return self.config[method_name]
        else:
            return self.config["*"]

    def generate(self):
        service_name_parts = self.service_instance.service_full_name.split(".")
        schema = {
            "openapi": "3.0.0",
            "info": {
                "title": ".".join(service_name_parts[1:]),
                "description": md_filter_definition(get_doc_str(
                    ident_type=IdentType.SERVICE,
                    pkg_path=self.zs_pkg_path,
                    ident=self.service_instance.service_full_name,
                    fallback=[f"REST API for {self.service_instance.service_full_name}"]
                )[0]),
                "contact": {
                    "email": "TODO@TODO.TODO"
                },
                "license": {
                    "name": "TODO"
                },
                "version": "TODO",
            },
            "servers": [],
            "paths": {
                method_info.path: {
                    method_info.http_method: {
                        "summary": method_info.docstring,
                        "description": method_info.docstring,
                        "operationId": method_info.name,
                        **method_info.parameters,
                        "responses": {
                            "200": {
                                "description": method_info.returndoc,
                                "content": {
                                    "application/octet-stream": {
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
                    self.generate_method_info(method_name)
                    for method_name in self.service_instance.method_names)
            }
        }
        yaml.dump(schema, self.output, default_flow_style=False)

    def generate_method_info(self, method_name: str) -> MethodSchemaInfo:
        result = MethodSchemaInfo(name=method_name)

        # Generate doc-strings
        if self.zs_pkg_path:
            doc_strings = get_doc_str(
                ident_type=IdentType.RPC,
                pkg_path=self.zs_pkg_path,
                ident=f"{self.service_instance.service_full_name}.{method_name}")
            if doc_strings:
                result.docstring = doc_strings[0]
                result.returntype = doc_strings[1]
                result.argtype = doc_strings[2]
                result.returndoc = md_filter_definition(get_doc_str(
                    ident_type=IdentType.STRUCT,
                    pkg_path=self.zs_pkg_path,
                    ident=result.returntype,
                    fallback=[f"### struct {result.returntype}"])[0])
                result.argdoc = md_filter_definition(get_doc_str(
                    ident_type=IdentType.STRUCT,
                    pkg_path=self.zs_pkg_path,
                    ident=result.argtype,
                    fallback=[f"### struct {result.argtype}"])[0])

        # Generate parameter passing scheme
        result.http_method, param_loc, flatten = self.config_for_method(method_name)
        if param_loc is None and result.http_method.lower() != "get":
            result.parameters = {
                "requestBody": {
                    "description": result.argdoc,
                    "content": {
                        ZSERIO_OBJECT_CONTENT_TYPE: {
                            "schema": {
                                "type": "string"
                            }
                        }
                    }
                }
            }
            return result

        if flatten:
            if self.generate_method_info_parameters_flat(result, param_loc):
                return result

        param_loc_str = "query"
        if param_loc is OAParamLocation.PATH:
            param_loc_str = "path"
            result.path += "/{requestData}"
        result.parameters = {
            "parameters": [{
                "in": param_loc_str,
                "name": "requestData",
                "description": result.argdoc,
                "required": True,
                ZSERIO_REQUEST_PART: ZSERIO_REQUEST_PART_WHOLE,
                "schema": {
                    "type": "string",
                    "format": "byte"
                }
            }]
        }
        return result

    def generate_method_info_parameters_flat(self,
                                             result: MethodSchemaInfo,
                                             param_loc: Optional[OAParamLocation]) -> bool:
        try:
            _, field_type_info = make_instance_and_typeinfo(
                service_method_request_type(
                    self.service_instance, result.name))
        except UnsupportedParameterError as e:
            print(UnsupportedParameterError(e.member_name, e.member_type, result.name))
            return False

        param_loc_str = "query"
        if param_loc is OAParamLocation.PATH:
            param_loc_str = "path"
            for field_name in field_type_info:
                result.path += f"/{{{field_name.replace('.', '__')}}}"
        result.parameters = {
            "parameters": [
                {
                    "in": param_loc_str,
                    "name": field_name.replace('.', '-'),
                    "description": f"Member of {result.argtype}.",
                    "required": True,
                    **(
                        {"allowEmptyValue": True} if param_loc is OAParamLocation.QUERY else {}
                    ),
                    ZSERIO_REQUEST_PART: field_name,
                    "schema": {
                        "format": "string",
                        **(
                            {
                                "type": "array",
                                "format": "string",
                                "items": {
                                    "type": "string"
                                }
                            }
                            if type(field_type) is list else
                            {
                                "type": "string",
                                "format": "string",
                            }
                        )
                    }
                }
                for field_name, field_type in field_type_info.items()
            ]
        }
        return True


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
                        (A) Path to a zserio .zs file.
                        (B) Path to parent dir of a zserio Python package.
                        
                        Examples:
                            -i path/to/schema/main.zs         (A)
                            -i path/to/python/package/parent  (B) 
                        """))
    parser.add_argument("-p", "--package", nargs=1, metavar="top-level-package",
                        required=False, help=argdoc("""
                        When -i specifies a zs file (Option A), indicate
                        that a top-level zserio package name should be used.
                        
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
                        which indicate OpenApi method generator preferences:
                        
                        get|put|post|patch|delete : HTTP method tags
                                  query|path|body : Parameter location tags
                                        flat|blob : Flatten request object,
                                                    or pass it as whole blob.
                                                    
                        Note:
                            * The http method defaults to `post`.
                            * The parameter location defaults to `query` for
                              `get`, `body` otherwise.
                            * The `flat` tag is only meaningful in conjunction
                              with `query` or `path`.
                            * An unspecific tag list (no service-method-name)
                              affects the defaults only for following, not
                              preceding specialized tag assignments.
                              
                        Example:
                            -c post getLayerByTileId:get,flat,path
                        """))
    parser.add_argument("-o", "--output", nargs=1, type=FileType("w"), default=[sys.stdout],
                        metavar="output", help=argdoc("""
                        Output file path. If not specified, the output will be
                        written to stdout.
                        """))

    args = parser.parse_args(sys.argv[1:])
    OpenApiSchemaGenerator(
        service=args.service[0],
        path=args.input[0] if args.input else None,
        package=args.package[0] if args.package else None,
        config=[arg for args in args.config for arg in args] if args.config else [],
        output=args.output[0]).generate()
