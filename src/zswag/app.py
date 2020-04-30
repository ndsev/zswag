import connexion
import os
import yaml
import inspect
import zserio
import base64
import sys
from types import ModuleType
from typing import Type

from .doc import get_doc_str, IdentType, md_filter_definition


# Name of variable that is added to controller
CONTROLLER_SERVICE_INSTANCE = "_service"


class MethodInfo:
    """
    (Private) Return value of ZserioSwaggerApp._method_info()
    """
    def __init__(self, *, name, docstring="", returntype="", argtype="", returndoc="", argdoc=""):
        self.name = name
        self.docstring = docstring
        self.returntype = returntype
        self.argtype = argtype
        self.returndoc = returndoc
        self.argdoc = argdoc


class ZserioSwaggerApp(connexion.App):

    def __init__(self, *,
                 controller: ModuleType,
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
        self.zs_pkg_path = zs_pkg_path

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
                f"/{method_info.name}": {
                    "get": {
                        "summary": method_info.docstring,
                        "description": method_info.docstring,
                        "operationId": method_info.name,
                        "parameters": [{
                            "name": "requestData",
                            "in": "query",
                            "description": method_info.argdoc,
                            "required": True,
                            "schema": {
                                "type": "string",
                                "default": "Base64-encoded bytes",
                                "format": "byte"
                            }
                        }],
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
                        },
                        "x-openapi-router-controller": self.service_instance_path
                    },
                } for method_info in (self._method_info(method_name) for method_name in self.service_instance._methodMap)
            }
        }
        with open(self.yaml_path, "w") as yaml_file:
            yaml.dump(schema, yaml_file, default_flow_style=False)

    def _method_info(self, method_name: str) -> MethodInfo:
        result = MethodInfo(name=method_name)
        if not self.zs_pkg_path:
            return result
        doc_strings = get_doc_str(
            ident_type=IdentType.RPC,
            pkg_path=self.zs_pkg_path,
            ident=f"{self.service_instance.SERVICE_FULL_NAME}.{method_name}")
        if not doc_strings:
            return result
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
        return result
