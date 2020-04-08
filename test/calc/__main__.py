import zswag
import sys
from os.path import dirname, abspath

zswag.package(dirname(abspath(__file__))+"/calculator.zs")

import calc.client
import calc.server
import zserio
import calculator.api as api

mode = sys.argv[1] if len(sys.argv) > 1 else ""
host, port = sys.argv[2].split(':') if len(sys.argv) > 2 and ':' in sys.argv[2] else ("localhost", 5000)

if mode == "client":
    calc.client.run(host, port)
elif mode == "server":
    app = zswag.ZserioSwaggerApp(
        controller=calc.server,
        service_type=api.Calculator.Service)
    app.run(host=host, port=port)
else:
    print("Usage: python3 -m calc {server|client} [host:port]")
