from typing import Dict, List, Set, Optional
from enum import Enum

import yaml
import json
import os
import requests
from urllib.parse import urlparse


ALLOWED_SPEC_URI_EXTS = {".json", ".yml", ".yaml"}


class HttpMethod(Enum):
    GET = 0
    POST = 1


class ParamFormat(Enum):
    QUERY_PARAM_BASE64 = 0
    BODY_BINARY = 1


class MethodSpec:

    def __init__(self, name: str, path: str, http_method: HttpMethod, param_format: ParamFormat, param_name: str):
        self.name = name
        self.path = path
        self.http_method = http_method
        self.param_format = param_format
        self.param_name = param_name
        assert \
            self.http_method == HttpMethod.GET and param_format == ParamFormat.QUERY_PARAM_BASE64 or \
            self.http_method == HttpMethod.POST
        assert \
            self.param_format == ParamFormat.QUERY_PARAM_BASE64 and param_name or \
            self.param_format == ParamFormat.BODY_BINARY


class ZserioSwaggerSpec:

    def __init__(self, spec_url_or_path: str):
        spec_url_parts = urlparse(spec_url_or_path)
        extension = "yaml"
        if "." in spec_url_parts.path:
            extension = os.path.splitext(spec_url_parts.path)[1].lower()
        if spec_url_parts.scheme in {"http", "https"}:
            spec_str = requests.get(spec_url_or_path).text
        else:
            with open(spec_url_or_path, "r") as spec_file:
                spec_str = spec_file.read()
        assert extension in ALLOWED_SPEC_URI_EXTS
        if extension == ".json":
            self.spec = json.loads(spec_str)
        else:
            self.spec = yaml.load(spec_str)

        self.methods: Dict[str, MethodSpec] = {}
        for path, path_spec in self.spec["paths"].items():
            for method, method_spec in path_spec.items():
                http_method = HttpMethod[method.upper()]
                name = method_spec["operationId"]
                param_format = ParamFormat.BODY_BINARY
                expected_param_name = "requestData"
                if "parameters" in method_spec and \
                   any(param for param in method_spec["parameters"] if param["name"] == expected_param_name):
                    param_format = ParamFormat.QUERY_PARAM_BASE64
                else:
                    expected_param_name = ""
                method_spec_object = MethodSpec(
                    name=name,
                    path=path,
                    http_method=http_method,
                    param_format=param_format,
                    param_name=expected_param_name)
                assert name not in self.methods
                self.methods[name] = method_spec_object

    def method_spec(self, method: str) -> Optional[MethodSpec]:
        if method not in self.methods:
            return None
        return self.methods[method]

    def path(self) -> Optional[str]:
        if "servers" in self.spec:
            servers = self.spec["servers"]
            if len(servers):
                server = servers[0]
                if "url" in server and server["url"]:
                    server = urlparse(server["url"])
                    return server.path
        return None
