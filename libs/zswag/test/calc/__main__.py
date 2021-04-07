import zswag
import sys

from zswag.test.calc import working_dir
import zswag.test.calc.client as client
import zswag.test.calc.server as server
import calculator.api as calculator

mode = sys.argv[1] if len(sys.argv) > 1 else ""
host, port = sys.argv[2].split(':') if len(sys.argv) > 2 and ':' in sys.argv[2] else ("localhost", 5000)

if mode == "client":
    client.run(host, port)
elif mode == "server":
    app = zswag.OAServer(
        controller_module=server,
        service_type=calculator.Calculator.Service,
        yaml_path=working_dir+"/api.yaml",
        zs_pkg_path=working_dir)
    app.run(host=host, port=port)
elif mode == "path":
    print(working_dir)
else:
    print("Usage: python3 -m calc {server|client} [host:port]")
