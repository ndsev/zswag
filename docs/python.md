# Python Client and Server

The Python module `zswag` provides:

- **`OAClient`** — a client transport that talks to any zserio service exposed via OpenAPI/REST.
- **`OAServer`** — a Flask/Connexion-based server layer that wraps a zserio-Python service controller.
- **`zswag.gen`** — a CLI for generating an OpenAPI YAML from a zserio service. See [`openapi-generator.md`](openapi-generator.md).

## Install

```bash
pip install zswag
```

Wheels are published for 64-bit Python 3.10–3.13 on Linux (x86_64), macOS (x86_64 / arm64), and Windows (x64). On Windows make sure the [Microsoft Visual C++ Redistributable](https://aka.ms/vs/16/release/vc_redist.x64.exe) is installed.

## Client usage

The Python client talks to any zserio service running over HTTP/REST that publishes an OpenAPI spec. Given a `myapp` Python module containing zserio-generated code (e.g. from `services.zs`):

```python
from zswag import OAClient
import services.api as services

openapi_url = "http://localhost:5000/openapi.json"

# OAClient reads per-method HTTP details from the spec.
# is_local_file=True if the URL is a filesystem path instead.
client = services.MyService.Client(OAClient(openapi_url))

# This triggers an HTTP request under the hood.
client.my_api(services.Request(1))
```

You can pass an adhoc `HTTPConfig` for per-call headers/auth/proxy:

```python
from zswag import OAClient, HTTPConfig

config = (HTTPConfig()
    .header(key="X-My-Header", val="value")
    .cookie(key="MyCookie", val="value")
    .query(key="MyQuery", val="value")
    .proxy(host="localhost", port=5050, user="john", pw="doe")
    .basic_auth(user="john", pw="doe")
    .bearer("bearer-token")
    .api_key("token"))

client = services.MyService.Client(
    OAClient("http://localhost:8080/openapi.json", config=config))

# Shortcuts for the two most common forms:
client = services.MyService.Client(
    OAClient("http://localhost:8080/openapi.json", api_key="token", bearer="token"))
```

The adhoc `config` enriches the [persistent settings](http-settings.md) loaded from `HTTP_SETTINGS_FILE`; it does not replace them. To suppress persistent settings (e.g. in tests), set `HTTP_SETTINGS_FILE` to empty.

## Server usage

`OAServer` marries a zserio-generated service skeleton with a user-written controller and an OpenAPI spec. It's based on Flask and Connexion.

A typical server script:

```python
import zswag
import myapp.controller as controller
from myapp import working_dir

# This import only resolves after zserio Python codegen has run.
import services.api as services

app = zswag.OAServer(
  controller_module=controller,
  service_type=services.MyService.Service,
  yaml_path=working_dir + "/api.yaml",
  zs_pkg_path=working_dir)

if __name__ == "__main__":
    app.run()
```

We recommend invoking the zserio Python generator from your `__init__.py`:

```python
import zserio
from os.path import dirname, abspath

working_dir = dirname(abspath(__file__))
zserio.generate(
  zs_dir=working_dir,
  main_zs_file="services.zs",
  gen_dir=working_dir)
```

Two things `OAServer` looks for at startup:

- **OpenAPI spec** (`yaml_path`): if missing, the error message contains the exact `zswag.gen` invocation that would generate it. See [`openapi-generator.md`](openapi-generator.md).
- **Controller module**: a Python module whose top-level functions match the service method names and accept the typed zserio request:

```python
# myapp/controller.py
import services.api as services

def my_api(request: services.Request):
    return services.Response(request.value * 42)
```

### Response codes

`OAServer` returns:

- `400 Bad Request` — when the user request can't be parsed.
- `500 Internal Server Error` — when the controller raises an unhandled exception.
- `200 OK` — on success.

If a Connexion-supported `[swagger-ui]` extra is installed (`pip install "connexion[swagger-ui]"`), the API docs become available at `[/prefix]/ui`.

## Persistent HTTP settings

See [`http-settings.md`](http-settings.md) for the YAML format. The Python client auto-loads `HTTP_SETTINGS_FILE` and applies it to every request whose URL matches a registered scope.

## Environment variables

See the [environment variables table](http-settings.md#environment-variables).

## OpenAPI feature support

See [the interop matrix in README.md](../README.md#openapi-options-interoperability) for the full ✅/❌ table comparing Python with C++ and Java.

## Where things live in the repo

- `libs/zswag/` — the Python package proper (`OAServer`, `OAClient`, `zswag.gen`).
- `libs/pyzswagcl/` — pybind11 bindings exposing the C++ `zswagcl` core to Python; treat as internal.
- `libs/zswag/test/calc/` — the canonical end-to-end fixture (Calculator service, OpenAPI YAML, Python server, Python client, used by C++ and Java integration tests too).
