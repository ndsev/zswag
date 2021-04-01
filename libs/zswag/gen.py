import yaml
import sys
import inspect
from typing import Optional, Dict, Tuple, List, IO
from argparse import ArgumentParser, FileType, RawTextHelpFormatter

from pyzswagcl import \
    OAParamLocation, \
    ZSERIO_OBJECT_CONTENT_TYPE, \
    ZSERIO_REQUEST_PART_WHOLE, \
    ZSERIO_REQUEST_PART

from .reflect import make_instance_and_typeinfo, service_method_request_type, rgetattr
from .doc import get_doc_str, IdentType, md_filter_definition

HTTP_METHOD_TAGS = ("get","put","post","patch","delete")
PARAM_LOCATION_QUERY_TAG = "query"
PARAM_LOCATION_BODY_TAG = "body"
PARAM_LOCATION_PATH_TAG = "path"
PARAM_LOCATION_TAGS = (PARAM_LOCATION_QUERY_TAG, PARAM_LOCATION_PATH_TAG, PARAM_LOCATION_BODY_TAG)
FLATTEN_TAG = "flat"


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
                 path: Optional[str] = None,
                 config: Optional[List[str]] = None,
                 zs: Optional[str] = None,
                 output: IO):
        self.service_name = service
        if path:
            sys.path.append(path)
        service_name_parts = service.split(".")
        self.service_type = rgetattr(__import__(
            f"{service_name_parts[0]}.api"
        ), ".".join(service_name_parts[1:]))
        self.service_instance = self.service_type()
        self.config: Dict[str, Tuple[str, Optional[OAParamLocation], bool]] = dict()
        self.config["*"] = ("post", None, False)
        self.output = output
        self.zs_pkg_path = zs
        if config:
            for entry in config:
                entry_http_method = "post"
                entry_param_loc: Optional[OAParamLocation] = None
                entry_flatten = False
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
                        assert entry_http_method != "get"
                    elif tag == FLATTEN_TAG:
                        entry_flatten = True
                self.config[method_name] = (entry_http_method, entry_param_loc, entry_flatten)

    def config_for_method(self, method_name: str) -> Tuple[str, OAParamLocation, bool]:
        if method_name in self.config:
            return self.config[method_name]
        else:
            return self.config["*"]

    def generate(self):
        service_name_parts = self.service_instance.SERVICE_FULL_NAME.split(".")
        schema = {
            "openapi": "3.0.0",
            "info": {
                "title": ".".join(service_name_parts[1:]),
                "description": md_filter_definition(get_doc_str(
                    ident_type=IdentType.SERVICE,
                    pkg_path=self.zs_pkg_path,
                    ident=self.service_instance.SERVICE_FULL_NAME,
                    fallback=[f"REST API for {self.service_instance.SERVICE_FULL_NAME}"]
                )[0]),
                "contact": {
                    "email": "TODO"
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
                    for method_name in self.service_instance.METHOD_NAMES)
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
                ident=f"{self.service_instance.SERVICE_FULL_NAME}.{method_name}")
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
        http_method, param_loc, flatten = self.config_for_method(method_name)
        if param_loc is None and http_method.lower() != "get":
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
        elif flatten:
            _, field_type_info = make_instance_and_typeinfo(service_method_request_type(self.service_instance, method_name))
            param_loc_str = "query"
            if param_loc is OAParamLocation.PATH:
                param_loc_str = "path"
                for field_name in field_type_info:
                    result.path += f"/{{{field_name.replace('.', '-')}}}"
            result.parameters = {
                "parameters": [
                    {
                        "in": param_loc_str,
                        "name": field_name.replace('.', '-'),
                        "description": f"Member of {result.argtype}.",
                        "required": True,
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
        else:
            param_loc_str = "query"
            if param_loc is OAParamLocation.PATH:
                param_loc_str = "path"
                result.argdoc += "/{requestData}"
            result.parameters = [{
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

        return result


if __name__ == "__main__":

    parser = ArgumentParser(
        "Zserio OpenApi YAML Generator",
        formatter_class=RawTextHelpFormatter)
    parser.add_argument("-s", "--service",
                        nargs=1,
                        required=True,
                        metavar="service-identifier",
                        help=inspect.cleandoc("""
                        Service class Python identifier. Example:
                            -s my.package.ServiceClass.Service
                        """))
    parser.add_argument("-p", "--path",
                        nargs=1,
                        metavar="pythonpath-entry",
                        required=False,
                        help=inspect.cleandoc("""
                        Path to *parent* directory of zserio Python package.
                        Only needed if zserio Python package is not in Python
                        environment. Example:
                            -p path/to/python/package/parent
                        """))
    parser.add_argument("-c", "--config",
                        nargs="+",
                        metavar="openapi-tag-expression",
                        help=inspect.cleandoc("""
                            Configuration tags for a specific or all methods.
                            The argument syntax follows this pattern:
                                
                                [(service-method-name):](comma-separated-tags)
                                
                            Note: The -c argument may be applied multiple times.
                            The `comma-separated-tags` must be a list of tags
                            which indicate OpenApi method generator preferences:
                            
                            get|put|post|patch|delete : HTTP method tags
                                      query|path|body : Parameter location tags
                                                 flat : Flatten request object
                                                        
                            Note:
                                * The http method defaults to `post`.
                                * The parameter location defaults to `query` for
                                  `get`, `body` otherwise.
                                * The `flat` tag is only meaningful in conjunction
                                  with `query` or `body`.
                        """),
                        action="append")
    parser.add_argument("-z", "--zs", nargs=1, metavar="Zserio source directory",
                        required=False, help=inspect.cleandoc("""
                        Zserio source directory from which documentation
                        should be extracted.
                        """))
    parser.add_argument("-o", "--output", nargs=1, type=FileType("w"), default=sys.stdout,
                        help="""
                        Output file path. If not specified, the output will be written to stdout.
                        """)

    args = parser.parse_args(sys.argv[1:])
    OpenApiSchemaGenerator(
        service=args.service[0],
        path=args.path[0] if args.path else None,
        config=[arg for args in args.config for arg in args],
        zs=args.zs[0] if args.zs else None,
        output=args.output[0]).generate()
