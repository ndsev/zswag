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

## Running the remote calculator test example

```bash
PYTHONPATH=$PWD/test
python3 -m calc server &
python3 -m calc client
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
It is planned to make this more flexible in a future release.

## Using the client

If you have a service called `my.package.Service`, then zserio
will automatically generate a client for the service under
`my.package.Service.Client`. This client can be instantiated alas ...

```python
from my.package import Service
import zswag_client
client = Service.Client(zswag_client.HttpClient(spec=f"http://localhost:5000/openapi.json"))
```

`zswag.HttpClient` provides the service client interface expected by zserio.
It reads HTTP specifics for the service from an OpenAPI YAML or JSON spec
that must be located under the given path or URL. 
For more options with the `HttpClient` constructor check out it's doc-string.

## Swagger UI 

If you have installed `pip install connexion[swagger-ui]`, you can view
API docs of your service under `[/prefix]/ui`.

## OpenAPI YAML spec

### YAML file location/auto-generation

* If you specify a non-empty path to a file which does not yet exist, the OpenAPI spec is auto-generated in that location.
* If you specify an empty YAML path, the yaml file is placed next to the
`<service>.zs` source-file.
* If you specify an existing file, `zswag` will simply verify
  all the methods specified in your zserio service are also reflected in
  the OpenAPI-spec.

### OpenAPI Spec Options

#### Options Overview

`ZserioSwaggerApp` and `zswag_client.HttpClient` currently
offer several degrees of freedom regarding HTTP-specifics in the
OpenAPI YAML file:
* **HTTP Method**
* **Parameter Format**
* **Server URL Base Path**

#### Option: HTTP method

To change the **HTTP method**, simply place the desired method
as the key under the method path, such as in the following example:
```yaml
paths:
  /methodName:
    {get|post|put|patch|delete}:
      ...
```

#### Option: Base64 URL Parameter Format

To use the __Base64 URL parameter format__, use the snippet below in you method spec.
```yaml
parameters:
- description: ''
  in: query
  name: requestData
  required: true
  x-zserio-request-part: "*"  # The parameter represents the whole zserio request object
  schema:
    format: byte
    type: string
```

#### Option: Binary Body Parameter Format

To use the Binary Body Parameter Format, use the snippet below in your method spec and remove the `requestData` parameter.
```yaml
requestBody:
  content:
    application/x-zserio-object:
      schema:
        type: string
```

#### Option: Server URL Base Path

OpenAPI allows for a `servers` field in the spec that lists URL path prefixes
under which the specified API may be reached. A `zswag_client.HttpClient`
instance looks into this list and determines the URL base path it uses from
the first entry in this list. A sample entry might look as follows:
```
servers:
- http://unused-host-information/path/to/my/api
``` 
The `zswag_client.HttpClient` will then call methods with your specified host
and port, but prefix the `/path/to/my/api` string. 

### Documentation extraction

When the OpenAPI/Swagger YAML is auto-generated, `ZserioSwaggerApp`
tries to populate the service/method/argument/result descriptions
with doc-strings which are extracted from the zserio sources.

For structs and services, the documentation is expected to be
enclosed by `/*! .... !*/` markers preceding the declaration:

```C
/*!
### My Markdown Struct Doc
I choose to __highlight__ this word.
!*/

struct MyStruct {
    ...
}
``` 

For service methods, a single-line doc-string is parsed which
immediately precedes the declaration:

```C
/** This method is documented. */
ReturnType myMethod(ArgumentType);
```
 
