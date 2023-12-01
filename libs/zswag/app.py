import connexion
import os
import inspect
import zserio
import sys
import yaml
from typing import Type
from flask import request as flask_request

from pyzswagcl import \
    parse_openapi_config, \
    OAMethod

from .reflect import request_object_blob, service_method_request_type, to_snake

# Name of variable that is added to controller
# to hold the service instance.
CONTROLLER_SERVICE_INSTANCE = "_service"

# Name of OpenApi extension field from which connexion
# reads the target wsgi function.
CONTROLLER_OPENAPI_FIELD = "x-openapi-router-controller"

# Utility function for slash conversion in format strings
def to_slashes(s: str):
    return s.replace("\\", "/")


# Raised if the controller passed to OAServer is missing a function
class IncompleteSchemaError(RuntimeError):
    def __init__(self, schema_path: str, fn_name: str):
        super(IncompleteSchemaError, self).__init__(f"Missing operation `{fn_name}` in {schema_path}!")
        self.schema_path = schema_path
        self.fn_name = fn_name


class OAServer(connexion.App):

    def __init__(self, *,
                 controller_module,
                 service_type: Type[zserio.ServiceInterface],
                 zs_pkg_path: str = None,
                 yaml_path: str = None):
        """
        Brief

            Marry a user-written app controller with a zserio-generated app server class
            (argument parser/response serialiser) and a fitting Swagger OpenApi spec.

            The OpenApi spec is auto-generated if the user does not specify an existing file.
            If the user specifies an empty YAML path, the yaml file is placed next to the
            zserio python-service source-file.

            If you have installed `pip install connexion[swagger-ui]`, you can view
            API docs of your service under [/prefix]/ui.

            Documentation for the service is automatically extracted if `zs_pkg_path` is issued.

        Code example

            In file my.app.__init__:

                from zserio_gen.my.service import Service
                from zswag import Server
                import

                app = Server(my.app.controller, Service)

            In file my.app.controller:

                # NOTE: Injected by Server, invisible to developer!
                #  In swagger yaml, the path specs reference `my.app.controller._service.myApi`
                _service = Service()
                _service.myApi = lambda request: _service._myApiMethod(request)
                _service._myApiImpl = my.app.controller.my_api

                # Written by user
                def my_api(request):
                    return "response"

        General call structure:

            OpenAPI `yaml_file` "paths/service" references
             Zserio Server instance injected method (ns.service.service(base64): blob), which calls
             ns.service._serviceMethod(blob), which calls
             ns.service._serviceImpl(value) which is remapped to
             User function (ns.serviceImpl(value): value).

        """
        if not yaml_path:
            service_module = sys.modules[service_type.__module__]
            yaml_path = os.path.join(
                os.path.dirname(os.path.abspath(service_module.__file__)),
                f"{service_module.__name__.split('.')[-1]}.{service_type.__name__}.yaml")
            print(f"Using yaml path {yaml_path}")
        self.yaml_path = yaml_path
        yaml_parent_path = os.path.dirname(yaml_path)
        self.zs_pkg_path = zs_pkg_path

        # Initialise zserio service
        self.service_type = service_type
        assert inspect.isclass(self.service_type)

        # Initialise zserio service working instance
        self.controller_path = controller_module.__name__
        self.controller = controller_module
        self.service_instance_path = self.controller_path+f".{CONTROLLER_SERVICE_INSTANCE}"
        if not hasattr(self.controller, CONTROLLER_SERVICE_INSTANCE):
            setattr(self.controller, CONTROLLER_SERVICE_INSTANCE, self.service_type())
        self.service_instance = getattr(self.controller, CONTROLLER_SERVICE_INSTANCE)

        # Verify or generate yaml file
        if not os.path.isfile(yaml_path):
            print("\n" + inspect.cleandoc(f"""
                ERROR: File does not exist: {yaml_path}
                ----------------------------{"-"*len(yaml_path)}
                
                You can generate the file by running ...
                    
                    python -m zswag.gen \\
                        --service {self.service_instance.SERVICE_FULL_NAME} \\
                        --input "{
                            to_slashes(os.path.abspath(os.path.dirname(os.path.dirname(
                                __import__(self.service_type.__module__.split('.')[0]).__file__
                            ))))
                        }" \\
                        --config post,body \\
                        --output "{to_slashes(yaml_path)}"
                        
                    The --config argument is a comma-separated list of tags
                    to specify OpenAPI options. For more info, please run
                    
                        python -m zswag.gen --help
            """))
            exit(1)

        self.spec = parse_openapi_config(yaml_path)
        self.verify_openapi_schema()

        # Re-route service impl methods
        for method_name in self.service_instance.method_names:
            method_snake_name = to_snake(method_name)
            user_function = getattr(self.controller, method_snake_name)
            zserio_modem_function = getattr(self.service_instance, f"_{method_snake_name}_method")
            request_type = service_method_request_type(self.service_instance, method_name)
            assert zserio_modem_function

            if not user_function or not inspect.isfunction(user_function):
                print(f"WARNING: The controller {self.controller_path} does not implement {method_snake_name}!")
                continue

            print(f"Found {self.controller_path}.{method_snake_name}.")
            if len(inspect.signature(user_function).parameters) != 1:
                print(f"ERROR: {self.controller_path}.{method_snake_name} must have single 'request' parameter!")
                continue

            method_spec: OAMethod = self.spec[method_name]

            def wsgi_method(fun=zserio_modem_function, spec=method_spec, req_t=request_type, **kwargs):
                if spec.body_request_object:
                    request_blob = kwargs["body"]
                else:
                    request_blob = request_object_blob(
                        req_t=req_t,
                        spec=spec,
                        headers=flask_request.headers,
                        **kwargs)
                try:
                    return bytes(fun(request_blob, None).byte_array)
                except zserio.PythonRuntimeException as e:
                    if str(e).startswith("BitStreamReader"):
                        return "Error in BitStreamReader: Could not parse malformed request.", 400
                    else:
                        return f"Internal Server Error: {e}", 500
            setattr(self.service_instance, method_name, wsgi_method)

            def method_impl(request, ctx=None, fun=user_function):
                return fun(request)
            setattr(self.service_instance, f"_{method_snake_name}_impl", method_impl)

        # Load spec and inject openapi-router-controller
        print(f"Loading spec from {yaml_path} ...")
        with open(yaml_path, 'r') as swagger_file:
            openapi = yaml.load(swagger_file, Loader=yaml.FullLoader)
        for path_name, path_spec in openapi["paths"].items():
            for meth_name, method_spec in path_spec.items():
                if CONTROLLER_OPENAPI_FIELD not in method_spec:
                    method_spec[CONTROLLER_OPENAPI_FIELD] = self.service_instance_path
                else:
                    print(f"{meth_name} {path_name}: Using pre-set {CONTROLLER_OPENAPI_FIELD}.")

        # Initialise connexion app
        super(OAServer, self).__init__(
            self.controller_path,
            specification_dir=yaml_parent_path)

        # Add the API according to the verified yaml spec.
        self.add_api(
            openapi,
            arguments={"title": f"REST API for {service_type.__name__}"},
            pythonic_params=False)

    def verify_openapi_schema(self):
        for method_name in self.service_instance.method_names:
            if method_name not in self.spec:
                raise IncompleteSchemaError(self.yaml_path, method_name)

