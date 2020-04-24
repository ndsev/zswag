# Zswag

Convience functionality to create python modules from zserio services at warp speed.
Translate/verify Zserio services to/with OpenAPI/Swagger and serve them in Flask/Connexion WSGI apps.

## Installation

Clone this repository, and run

```bash
pip3 install -e .
```

## Creating a Swagger service from zserio

`ZserioSwaggerApp` gives you the power to marry a user-written app controller
with a zserio-generated app server class (argument parser/response serialiser)
and a fitting Swagger OpenAPI spec.

**Example**

```py
import zswag
import zserio
import my.app.controller

zserio.require("myapp/service.zs")
from myapp.service import Service

# The OpenApi argument `yaml_path=...` is optional
app = zswag.ZserioSwaggerApp(my.app.controller, Service)
```

Here, the API endpoints are routed to `my/app/controller.py`,
which might look as follows:

```py
# Written by you
def myApi(request):
    return "response"

# Injected by ZserioSwaggerApp
# _service = Service()
# _service.myApi = lambda request: _service._myApiMethod(request)
# _service._myApiImpl = my.app.controller.myApi
```

**FYI**

The OpenAPI spec is auto-generated if you do not specify an existing file.
If you specify an empty YAML path, the yaml file is placed next to the
zserio python-service source-file.

If you have installed `pip install connexion[swagger-ui]`, you can view
API docs of your service under `[/prefix]/ui`.
