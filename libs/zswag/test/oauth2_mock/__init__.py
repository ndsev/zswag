"""
OAuth2 Mock Server for testing OAuth 1.0 signature and HTTP Basic Auth.
"""

from .server import app, run_server

__all__ = ['app', 'run_server']
