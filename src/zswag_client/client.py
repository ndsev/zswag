import requests
import zserio
import base64

from .spec import ZserioSwaggerSpec, HttpMethod, ParamFormat


class HttpClient(zserio.ServiceInterface):
    """
    Implementation of HTTP client as Zserio generic service interface.
    """

    def __init__(self, *, proto="http", host="localhost", port=5000, spec_url_or_path):
        """
        Constructor.
        :param host: Host to connect.
        :param port: Port to connect.
        """
        self.spec = ZserioSwaggerSpec(spec_url_or_path)
        self.path = f"{proto}://{host}:{port}{self.spec.path()}"

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
