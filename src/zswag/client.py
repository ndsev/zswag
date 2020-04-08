import requests
import zserio
import base64


class HttpClient(zserio.ServiceInterface):
    """
    Implementation of HTTP client as Zserio generic service interface.
    """

    def __init__(self, *, method_prefix="/", proto="http", host="localhost", port=5000, op="GET"):
        """
        Constructor.
        :param method_prefix: Prefix for request URLs to put after proto://host:port and in front of method-names.
        :param host: Host to connect.
        :param port: Port to connect.
        """
        self.url_base = f"{proto}://{host}:{port}{method_prefix}"
        self.op = op

    def callMethod(self, methodName, requestData, context=None):
        """
        Implementation of ServiceInterface.callMethod.
        """
        try:
            params = {"requestData": base64.urlsafe_b64encode(requestData)}
            if self.op == "GET":
                response = requests.get(self.url_base + methodName, params=params)
            else:
                response = requests.post(self.url_base + methodName, params=params)
            if response.status_code != requests.codes.ok:
                raise zserio.ServiceException(str(response.status_code))
            return response.content
        except Exception as e:
            raise zserio.ServiceException("HTTP call failed: " + str(e))
