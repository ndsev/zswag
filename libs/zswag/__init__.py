"""Public Python entry point for zswag.

The package exposes two complementary APIs:

* :class:`OAServer`, the pure-Python Connexion/Flask server adapter that
  connects zserio-generated Python services to generated OpenAPI documents.
* :class:`OAClient` and :class:`HTTPConfig`, the native pybind11 bindings for
  the C++ zswag client and HTTP transport.

Most applications import from this module directly instead of importing the
implementation modules. Advanced tooling can still use :mod:`zswag.gen`,
:mod:`zswag.reflect`, and :mod:`zswag.doc` explicitly.
"""

from .app import *

from .pyzswagcl import OAClient, HTTPError, HTTPConfig
