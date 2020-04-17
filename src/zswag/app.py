import connexion
import os
import yaml
import inspect
import zserio
import base64
import sys
from types import ModuleType
from typing import Type


# Name of variable that is added to controller
CONTROLLER_SERVICE_INSTANCE = "_service"


class ZserioSwaggerApp(connexion.App):

    def __init__(self, *,
                 controller: ModuleType,
                 service_type: Type[zserio.ServiceInterface],
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

        Code example

            In file my.app.__init__:

                from gen.zserio import Service
                from zswag import ZserioSwaggerApp

                app = ZserioSwaggerApp(my.app.controller, Service)

            In file my.app.controller:

                # NOTE: Injected by ZserioSwaggerApp, invisible to developer!
                #  In swagger yaml, the path specs reference `my.app.controller._service.myApi`
                _service = Service()
                _service.myApi = lambda request: _service._myApiMethod(request)
                _service._myApiImpl = my.app.controller.myApiImpl

                # Written by user
                def myApi(request):
                    return "response"

        General call structure:

            OpenAPI `yaml_file` "paths/service" references
             Zserio Server instance injected method (ns.service.service(base64): blob), which calls
             ns.service._serviceMethod(blob), which calls
             ns.service._serviceImpl(value) which is remapped to
             Runtime-generated user function (ns.serviceImpl(value): value).

        """
        if not yaml_path:
            service_module = sys.modules[service_type.__module__]
            yaml_path = os.path.join(
                os.path.dirname(os.path.abspath(service_module.__file__)),
                f"{service_module.__name__.split('.')[-1]}.{service_type.__name__}.yaml")
            print(f"Using yaml path {yaml_path}")
        self.yaml_path = yaml_path
        yaml_parent_path = os.path.dirname(yaml_path)
        yaml_basename = os.path.basename(yaml_path)

        # Initialise zserio service
        self.service_type = service_type
        assert inspect.isclass(self.service_type)

        # Initialise zserio service working instance
        self.controller_path = controller.__name__
        self.controller = controller
        self.service_instance_path = self.controller_path+f".{CONTROLLER_SERVICE_INSTANCE}"
        if not hasattr(self.controller, CONTROLLER_SERVICE_INSTANCE):
            setattr(self.controller, CONTROLLER_SERVICE_INSTANCE, self.service_type())
        self.service_instance = getattr(self.controller, CONTROLLER_SERVICE_INSTANCE)

        # Re-route service impl methods
        for methodName in self.service_instance._methodMap:
            user_function = getattr(self.controller, methodName)
            zserio_modem_function = getattr(self.service_instance, f"_{methodName}Method")
            assert zserio_modem_function
            if not user_function or not inspect.isfunction(user_function):
                print(f"WARNING: The controller {self.controller_path} does not implement {methodName}!")
                continue

            print(f"Found {self.controller_path}.{methodName}.")
            if len(inspect.signature(user_function).parameters) != 1:
                print(f"ERROR: {self.controller_path}.{methodName} must have single 'request' parameter!")
                continue

            def wsgi_method(request_data, fun=zserio_modem_function):
                request_data = base64.urlsafe_b64decode(request_data)
                return bytes(fun(request_data, None))
            setattr(self.service_instance, methodName, wsgi_method)

            def method_impl(request, ctx=None, fun=user_function):
                return fun(request)
            setattr(self.service_instance, f"_{methodName}Impl", method_impl)

        # Verify or generate yaml file
        if os.path.isfile(yaml_path):
            self.verify_openapi_schema()
        else:
            self.generate_openapi_schema()

        # Initialise connexion app
        super(ZserioSwaggerApp, self).__init__(
            self.controller_path,
            specification_dir=yaml_parent_path)

        # Add the API according to the verified yaml spec.
        self.add_api(
            yaml_basename,
            arguments={"title": f"REST API for {service_type.__name__}"},
            pythonic_params=True)

    def verify_openapi_schema(self):
        with open(self.yaml_path, "r") as yaml_file:
            schema = yaml.load(yaml_file)
            for methodName in self.service_instance._methodMap:
                assert any(path for path in schema["paths"] if path.endswith(f"/{methodName}"))

    def generate_openapi_schema(self):
        print(f"NOTE: Writing OpenApi schema to {self.yaml_path}")
        schema = {
            "openapi": "3.0.0",
            "info": {
                "title": self.service_instance.SERVICE_FULL_NAME,
                "description": f"REST API for {self.service_instance.SERVICE_FULL_NAME}",
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
                f"/{methodName}": {
                    "get": {
                        "summary": "TODO: Brief one-liner.",
                        "description": "TODO: Describe operation in more detail.",
                        "operationId": methodName,
                        "parameters": [{
                            "name": "requestData",
                            "in": "query",
                            "description": "TODO: Describe parameter",
                            "required": True,
                            "schema": {
                              "type": "string",
                              "format": "byte"
                            }
                        }],
                        "responses": {
                            "200": {
                                "description": "TODO: Describe response content",
                                "content": {
                                    "application/octet-stream": {
                                        "schema": {
                                            "type": "string",
                                            "format": "binary"
                                        }
                                    }
                                }
                            }
                        },
                        "x-openapi-router-controller": self.service_instance_path
                    },
                } for methodName in self.service_instance._methodMap
            }
        }
        with open(self.yaml_path, "w") as yaml_file:
            yaml.dump(schema, yaml_file, default_flow_style=False)
