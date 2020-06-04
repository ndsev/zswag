import requests
import zserio
import base64

from .spec import ZserioSwaggerSpec, HttpMethod, ParamFormat
from urllib.parse import urlparse
import os


class HttpClient(zserio.ServiceInterface):
    """
    Implementation of HTTP client as Zserio generic service interface.
    """

    def __init__(self, *, proto=None, host=None, port=None, spec):
        """
        Brief

            Constructor to instantiate a client based on an OpenApi specification.
            The specification must be located at `spec`, which can be
            a URL or a local path to a valid JSON/YAML OpenApi3 spec.

            Note: The default URL for the spec with a ZserioSwaggerApp-based server is
             {http|https}://{host}:{port}{/path}/openapi.json

        Example

            from my.package import Service
            import zswag
            client = Service.Client(zswag.HttpClient(spec=f"http://localhost:5000/openapi.json"))

        Arguments

            `spec`: URL or local path to a JSON or YAML file which holds the valid
              OpenApi3 description of the service. The following information is
              extracted from the specification:
              - A path to the service is extracted from the first entry in the servers-list.
                If such an entry is not available, the client will fall back to the
                path-part of the spec URL (without the trailing openapi.json).
              - For each operation:
                * HTTP method (GET or POST)
                * Argument passing scheme (Base64-URL-Param or Binary Body)

            `proto`: (Optional) HTTP protocol type, such as "http" or "https".
              If this argument is not given, the protocol will be extracted
              from the `spec` string, qssuming that is is a URL.

            `host`: (Optional) Hostname of the target server, such as an IP or
              a DNS name. If this argument is not given, the protocol will be extracted
              from the `spec` string, qssuming that is is a URL.

            `port`: (Optional) Port to use for connection. MUST be issued together with
              `host`. If the argument is set and `host` is not, it will be ignored.
        """
        spec_url_parts = urlparse(spec)
        netloc = \
            host if host and not port else \
            f"{host}:{port}" if host and port else \
            spec_url_parts.netloc
        self.spec = ZserioSwaggerSpec(spec)
        path = self.spec.path() or os.path.split(spec_url_parts.path)[0]
        self.path: str = f"{proto or spec_url_parts.scheme}://{netloc}{path}"
        if not self.path.endswith("/"):
            self.path += "/"

    def callMethod(self, method_name, request_data, context=None):
        """
        Implementation of ServiceInterface.callMethod.
        """
        try:
            method_spec = self.spec.method_spec(method_name)
            kwargs = {}
            if method_spec.param_format == ParamFormat.QUERY_PARAM_BASE64:
                kwargs["params"] = {"requestData": base64.urlsafe_b64encode(request_data)}
            else:
                kwargs["data"] = request_data
            if method_spec.http_method == HttpMethod.GET:
                response = requests.get(self.path + method_name, **kwargs)
            else:
                response = requests.post(self.path + method_name, **kwargs)
            if response.status_code != requests.codes.ok:
                raise zserio.ServiceException(str(response.status_code))
            return response.content
        except Exception as e:
            raise zserio.ServiceException("HTTP call failed: " + str(e))
