"""Public Python entry point for zswag.

The package exposes two complementary APIs:

* :class:`OAServer`, the pure-Python Connexion/Flask server adapter that
  connects zserio-generated Python services to generated OpenAPI documents.
* :class:`OAClient` and :class:`HTTPConfig`, the native nanobind bindings for
  the C++ zswag client and HTTP transport.

Most applications import from this module directly instead of importing the
implementation modules. Advanced tooling can still use :mod:`zswag.gen`,
:mod:`zswag.reflect`, and :mod:`zswag.doc` explicitly.
"""

from .app import *

from zserio import ServiceInterface as _ServiceInterface
from .pyzswagcl import OAClient as _NativeClient, HTTPError, HTTPConfig


class OAClient(_ServiceInterface):
    """Native OpenAPI client implementing the generated zserio service interface."""

    def __init__(self, url, is_local_file=False, config=None, api_key=None, bearer=None, server_index=None):
        self._client = _NativeClient(url, is_local_file, HTTPConfig() if config is None else config,
                                     api_key, bearer, server_index)

    def call_method(self, method_name, request, unused=None):
        """Serialize a zserio request and return the response byte values."""
        return self._client.call_method(method_name, request, unused)

    def config(self):
        """Return the OpenAPI configuration retained by this client."""
        return self._client.config()
