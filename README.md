# Zswag

[![CI](https://github.com/Klebert-Engineering/zswag/actions/workflows/cmake.yml/badge.svg)](https://github.com/Klebert-Engineering/zswag/actions/workflows/cmake.yml)

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

## Using the client

If you have a service called `my.package.Service`, then zserio
will automatically generate a client for the service under
`my.package.Service.Client`. This client can be instantiated alas ...

```python
from my.package import Service
from pyzswagcl import HttpClient
client = Service.Client(HttpClient(spec=f"http://localhost:5000/openapi.json"))
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

#### Option 🌟1: HTTP method

To change the **HTTP method**, simply place the desired method
as the key under the method path, such as in the following example:
```yaml
paths:
  /methodName:
    {get|post|put|patch|delete}:
      ...
```

#### Option 🌟2: Zserio request blob in body

```yaml
requestBody:
  content:
    application/x-zserio-object:
      schema:
        type: string
```

#### Option 🌟3: Zserio request blob in URL parameter

```yaml
parameters:
- description: ''
  in: query|path
  name: parameterName
  required: true
  x-zserio-request-part: "*"
  schema:
    format: string|byte|base64|base64url|hex|binary
```

About the `format` specifier value:
* Both `string` and `binary` result in a raw URL-encoded string.
* Both `byte` and `base64` result in a standard Base64-encoded value.
  The `base64url` option indicates URL-safe Base64 format.
* The `hex` encoding produces a hexadecimal encoding of the request blob.

#### Option 🌟4: Zserio request part, single value

```yaml
parameters:
- description: ''
  in: query|path
  name: parameterName
  required: true
  x-zserio-request-part: "field[.subfield]*"
  schema:
    format: string|byte|base64|base64url|hex|binary
```

In this case, `x-zserio-request-part` should point to an atomic built-in type,
such as `uint8`, `float32`, `extern` etc. Note that `uint8[]` array fields receive
special treatment: They are transferred as single-value blobs, not as arrays.

The `format` value effect remains as explained above. A small
difference exists for integer types: Their hexadecimal representation
will be the natural numeric one, not the binary. 

#### Option 🌟5: Zserio request part, array

```yaml
parameters:
- description: ''
  in: query|path
  style: form|simple|label|matrix
  explode: true|false
  name: parameterName
  required: true
  x-zserio-request-part: "field[.subfield]*"
  schema:
    format: string|byte|base64|base64url|hex|binary
```

In this case, `x-zserio-request-part` should point to an array of
atomic built-in types. The array will be encoded according
to the `format`, `style` and `explode` specifiers.

#### Option 🌟6: Zserio request part, dictionary

In this case, `x-zserio-request-part` should point to a zserio struct.
The OpenAPI schema options are the same as for arrays. All fields
of the designated struct which have an atomic built-in type are exposed
as key-value pairs.  The key-value-pairs will be encoded according
to the `format`, `style` and `explode` specifiers.

#### Option 🌟7: Server URL Base Path

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
