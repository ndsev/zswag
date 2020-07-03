from typing import Dict, List, Set, Optional
from enum import Enum

import yaml
import json
import os
import requests
from urllib.parse import urlparse


ZSERIO_OBJECT_CONTENT_TYPE = "application/x-zserio-object"
ZSERIO_REQUEST_PART = "x-zserio-request-part"
ZSERIO_REQUEST_PART_WHOLE = "*"


class HttpMethod(Enum):
    GET = 0
    POST = 1
    PUT = 2
    DELETE = 3
    PATCH = 4


class ParamLocation(Enum):
    QUERY = 0
    BODY = 1
    PATH = 2


class ParamFormat(Enum):
    STRING = 0
    BYTE = 1
    HEX = 2
    BINARY = 3


class ParamSpec:

    def __init__(self, *, name: str = "", format: ParamFormat, location: ParamLocation, zserio_request_part: str):
        # https://github.com/Klebert-Engineering/zswag/issues/15
        assert location in (ParamLocation.QUERY, ParamLocation.BODY)
        # https://github.com/Klebert-Engineering/zswag/issues/19
        assert zserio_request_part == ZSERIO_REQUEST_PART_WHOLE
        # https://github.com/Klebert-Engineering/zswag/issues/20
        assert \
            (format == ParamFormat.BINARY and location == ParamLocation.BODY) or \
            (format == ParamFormat.BYTE and location == ParamLocation.QUERY)
        self.name = name
        self.format = format
        self.location = location
        self.zserio_request_part = zserio_request_part


class MethodSpec:

    def __init__(self, name: str, path: str, http_method: HttpMethod, params: List[ParamSpec]):
        # https://github.com/Klebert-Engineering/zswag/issues/19
        assert len(params) == 1
        self.name = name
        self.path = path
        self.http_method = http_method
        self.params = params


class ZserioSwaggerSpec:

    def __init__(self, spec_url_or_path: str):
        spec_url_parts = urlparse(spec_url_or_path)
        if spec_url_parts.scheme in {"http", "https"}:
            spec_str = requests.get(spec_url_or_path).text
        else:
            with open(spec_url_or_path, "r") as spec_file:
                spec_str = spec_file.read()
        self.spec = yaml.load(spec_str)
        self.methods: Dict[str, MethodSpec] = {}
        for path, path_spec in self.spec["paths"].items():
            for method, method_spec in path_spec.items():
                http_method = HttpMethod[method.upper()]
                name = method_spec["operationId"]
                params: List[ParamSpec] = []
                if "requestBody" in method_spec:
                    assert "content" in method_spec["requestBody"]
                    for content_type in method_spec["requestBody"]["content"]:
                        if content_type == ZSERIO_OBJECT_CONTENT_TYPE:
                            params.append(ParamSpec(
                                format=ParamFormat.BINARY,
                                location=ParamLocation.BODY,
                                zserio_request_part=ZSERIO_REQUEST_PART_WHOLE))
                if "parameters" in method_spec:
                    for param in method_spec["parameters"]:
                        if ZSERIO_REQUEST_PART not in param:
                            continue
                        assert "schema" in param and "format" in param["schema"]
                        params.append(ParamSpec(
                            name=param["name"],
                            format=ParamFormat[param["schema"]["format"].upper()],
                            location=ParamLocation[param["in"].upper()],
                            zserio_request_part=param[ZSERIO_REQUEST_PART]))
                method_spec_object = MethodSpec(
                    name=name,
                    path=path,
                    http_method=http_method,
                    params=params)
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
