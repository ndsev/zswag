# Zswag

Convenience functionality to create python modules from zserio services at warp speed.
Translate/verify Zserio services to/with OpenAPI/Swagger and serve them in Flask/Connexion WSGI apps.

## Installation

Just run

```bash
pip3 install zswag
```

Alternatively, clone this repository, and run

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

zserio.generate("myapp/service.zs", "myapp")
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

**Note:** The server is currently generated such that the
zserio RPC method parameter is expected to be a Base64-encoded
string called `requestData`, which is placed in the URL query part.
It is planned to making this more flexible in a future release.

## Using the client

If you have a service called `my.package.Service`, then zserio
will automatically generate a client for the service under
`my.package.Service.Client`. This client can be instantiated alas ...

```python
from my.package import Service
import zswag

client = Service.Client(zswag.HttpClient(host=host, port=port))
```

`zswag.HttpClient` provides the service client interface expected
For more options with `HttpClient` apart from `host` and `port`,
check out it's doc-string.

## Swagger UI 

If you have installed `pip install connexion[swagger-ui]`, you can view
API docs of your service under `[/prefix]/ui`.

## OpenAPI YAML spec

### YAML file location/auto-generation

* If you do not specify a non-existing file path, the OpenAPI spec is auto-generated.
* If you specify an empty YAML path, the yaml file is placed next to the
`<service>.zs` source-file.
* If you specify an existing file, the `zswag` will simply verify
  all the methods specified in your zserio service are also reflected in
  the OpenAPI-spec.

### Documentation extraction

When the OpenAPI/Swagger YAML is auto-generated, `ZserioSwaggerApp`
tries to populate the service/method/argument/result descriptions
with doc-strings extracted from the zserio sources.

For structs and services, the documentation is expected to be
enclosed by `/*! .... !*/` markers preceding the declaration:

```C
/*!
### My Markdown Struct Doc
I choose to __highlight__ this word.
!*/

sruct MyStruct {
    ...
}
``` 

For service methods, a single-line doc-string is parsed which
immediately precedes the declaration:

```
/** This method is documented. */
ReturnType myMethod(ArgumentType);
```
 