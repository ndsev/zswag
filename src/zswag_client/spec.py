from typing import Dict, List, Set
from enum import Enum


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

    def __init__(self, yaml_path, zserio_service_class=None):
        pass

    def method_spec(self) -> MethodSpec:
        pass

    def has_valid_method_spec(self, method: str) -> bool:
        pass

    def servers(self) -> List[str]:
        raise NotImplementedError()
