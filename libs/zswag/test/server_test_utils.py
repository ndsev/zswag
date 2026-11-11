"""Helpers shared by the source-tree HTTP server tests."""

import socketserver

from werkzeug.serving import ThreadedWSGIServer


class _LoopbackTestServer(ThreadedWSGIServer):
    """Werkzeug test server that binds without a reverse DNS lookup."""

    def server_bind(self):
        # HTTPServer resolves its bound address before listening. macOS 15
        # runners can stall for roughly 35 seconds resolving 127.0.0.1.
        socketserver.TCPServer.server_bind(self)
        self.server_name, self.server_port = self.server_address[:2]


def run_loopback_test_server(app, host, port):
    """Serve a WSGI test application on a numeric loopback address.

    The source-tree integration tests use dedicated subprocesses and do not use
    the server's resolved hostname. Bypassing ``HTTPServer.server_bind`` avoids
    macOS 15's slow loopback reverse lookup without changing the production
    ``OAServer`` or globally replacing ``socket.getfqdn``.
    """
    if host not in {"127.0.0.1", "::1"}:
        raise ValueError(f"Test server host must be a numeric loopback address: {host}")

    server = _LoopbackTestServer(host, port, app)
    server.socket.set_inheritable(True)
    server.log_startup()
    server.serve_forever()
