# Zswag

[![CI](https://github.com/Klebert-Engineering/zswag/actions/workflows/cmake.yml/badge.svg)](https://github.com/Klebert-Engineering/zswag/actions/workflows/cmake.yml)

zswag is a set of libraries for using/hosting zserio services through OpenAPI.

**Table of Contents:**

  * [Components](#components)
  * [Setup](#setup)
    + [For Python Users](#for-python-users)
    + [For C++ Users](#for-c-users)
  * [Server Component (Python)](#server-component-python)
  * [Using the Python Client](#using-the-python-client)
  * [C++ Client](#c-client)
  * [OpenAPI Generator CLI](#openapi-generator-cli)
    + [Documentation extraction](#documentation-extraction)
  * [HTTP Proxies and Authentication](#http-proxies-and-authentication)
  * [UI](#ui)
  * [OpenAPI Options Interoperability](#openapi-options-interoperability)
    + [HTTP method](#http-method)
    + [Request Body](#request-body)
    + [URL Blob Parameter](#url-blob-parameter)
    + [URL Scalar Parameter](#url-scalar-parameter)
    + [URL Array Parameter](#url-array-parameter)
    + [URL Compound Parameter](#url-compound-parameter)
    + [Server URL Base Path](#server-url-base-path)

## Components

The zswag repository contains two main libraries which provide
OpenAPI layers for zserio Python and C++ clients. For Python, there
is even a generic zserio OpenAPI server layer.

The following UML diagram provides a more in-depth overview:

![Component Overview](doc/zswag-architecture.png)

Here are some brief descriptions of the main components:

* `zswagcl` is a C++ Library which exposes the zserio OpenApi service client `ZsrClient`
  as well as the more generic `OpenApiClient` and `OpenApiConfig` classes
  which are reused in Python.
* `zswag` is a Python Library which provides both a zserio Python service client
  (`OAClient`) as well as a zserio-OpenApi server layer based on Flask/Connexion
  (`OAServer`). It also contains the command-line tool `zswag.gen` to generate
  valid OpenAPI specs from zserio Python service classes.
* `pyzswagcl` is a binding library which exposes the C++-based OpenApi
  pasrsing/request functionality to Python. Please consider it "internal".
* `httpcl` is a wrapper around the [cpp-httplib](https://github.com/yhirose/cpp-httplib),
  and http request configuration and secret injection abilities.
  
## Setup

### For Python Users

Simply run `pip install zswag`. **Note: This currently only works with
64-bit Python 3.8 or 3.9. Also, make sure that your `pip --version` is greater
than 19.3.**

### For C++ Users

Using CMake, you can ...

* 🌟run tests.
* 🌟build the zswag wheels for Python != 3.8.
* 🌟[integrate the C++ client into a C++ project.](#c-client)

The basic setup follows the usual CMake configure/build steps:
```bash
mkdir build && cd build
cmake ..
cmake --build .
```

**Note:** The Python environment used for configuration will be used
to build the resulting wheels. After building, you will find the Python
wheels under `build/bin/wheel`.

**To run tests**, just execute CTest at the top of the build directory:
```bash
cd build && ctest --verbose
```

## Server Component (Python)

The `OAServer` component gives you the power to marry a zserio-generated app
server class with a user-written app controller and a fitting OpenAPI specification.
It is based on [Flask](https://flask.palletsprojects.com/en/1.1.x/) and
[Connexion](https://connexion.readthedocs.io/en/latest/).

### Integration Example

Let's consider the following zserio service saved under `myapp/service.zs`:

```
package service;

struct Request {
  int32 value;
};

struct Response {
  int32 value;
};

service MyService {
  Response my_api(Request);
};
```

**Note:** `myapp` must be available as a module (it must be
possible to `import myapp`). We recommend to run the zserio Python generator
invocation inside the `myapp` module's `__init__.py`:

```py
import zserio
from os.path import dirname, abspath

working_dir = dirname(abspath(__file__))
zserio.generate(
  zs_dir=working_dir,
  main_zs_file="service.zs",
  gen_dir=working_dir)
```

A server script like `myapp/server.py` might then look as follows:

```py
import zswag
import myapp.controller as controller
from myapp import working_dir

# This import only works after zserio generation.
import service.api as service

app = zswag.OAServer(
  controller_module=controller,
  service_type=service.MyService.Service,
  yaml_path=working_dir+"/api.yaml",
  zs_pkg_path=working_dir)

if __name__ == "__main__":
    app.run()
```

The server script above references two important components:
* An **OpenAPI file** (`myapp/api.yaml`): Upon startup, `OAServer`
  will output an error message if this file does not exist. The
  error message already contains the correct command to
  invoke the [OpenAPI Generator CLI](#openapi-generator-cli)
  to generate `myapp/api.yaml`.
* A **controller module** (`myapp/controller.py`): This file provides
  the actual implementations for your service endpoints.
  
For the current example, `controller.py` might look as follows:

```py
import service.api as service

# Written by you
def my_api(request: service.Request):
    return service.Response(request.value * 42)
```

## Using the Python Client

The generic Python client talks to any zserio service that is running
via HTTP/REST, and provides an OpenAPI specification of it's interface.

### Integration Example

As an example, consider a Python module called `myapp` which has the
same `myapp/__init__.py` and `myapp/service.zs` zserio definition as
[previously mentioned](#server-component-python). We consider
that the server is providing its OpenAPI spec under `localhost:5000/openapi.json`.

In this setting, a client `myapp/client.py` might look as follows:

```python
from zswag import OAClient
import service.api as service

openapi_url = f"http://localhost:5000/openapi.json"

# The client reads per-method HTTP details from the OpenAPI URL.
# You can also pass a local file by setting the `is_local_file` argument
# of the OAClient constructor.
client = service.MyService.Client(OAClient(openapi_url))

# This will trigger an HTTP request under the hood.
client.my_api(service.Request(1))
```

As you can see, an instance of `OAClient` is passed into the constructor
for zserio to use as the service client's transport implementation.

**Note:** While connecting, the client will also pass ...
1. [Authentication headers/cookies](#http-proxies-and-authentication).
2. Additional HTTP headers passed into the `OAClient` constructor as a
   dictionary like `{HTTP-Header-Name: Value}`

## C++ Client

The generic C++ client talks to any zserio service that is running
via HTTP/REST, and provides an OpenAPI specification of it's interface.
The C++ client is based on the [ZSR zserio C++ reflection](https://github.com/klebert-engineering/zsr)
extension.

### Integration Example

As an example, we consider the `myapp` directory which contains a `service.zs`
zserio definition as [previously mentioned](#server-component-python).

We assume that zswag is added to `myapp` as a [Git submodule](https://git-scm.com/book/en/v2/Git-Tools-Submodules)
under `myapp/zswag`.

Next to `myapp/service.zs`, we place a `myapp/CMakeLists.txt` which describes our project:

```cmake
project(myapp)

# This is how C++ will know about the zswag lib
# and its dependencies, such as zserio.
add_subdirectory(zswag)

# This command is provided by ZSR to easily create
# a CMake C++ reflection library from zserio code.
add_zserio_module(${PROJECT_NAME}-cpp
  ROOT "${CMAKE_CURRENT_SOURCE_DIR}"
  ENTRY service.zs
  TOP_LEVEL_PKG service
  SUBDIR_DEPTH 0)

# We create a myapp client executable which links to
# the generated zserio C++ library, the zswag client
# library and the ZSR reflection runtime.
add_executable(${PROJECT_NAME} client.cpp)
target_link_libraries(${PROJECT_NAME}
    ${PROJECT_NAME}-cpp-reflection zswagcl zsr)
```

The `add_executable` command above references the file `myapp/client.cpp`,
which contains the code to actually use the zswag C++ client. Again, the
code assumes that the server provides their OpenAPI definitions under
`localhost:5000/openapi.json`:

```cpp
#include "zswagcl/zsr-client.hpp"
#include <iostream>
#include <zsr/types.hpp>
#include <zsr/find.hpp>
#include <zsr/getset.hpp>

using namespace zswagcl;
using namespace httpcl;

int main (int argc, char* argv[])
{
    // Instantiate the client
    auto openApiUrl = "http://localhost:5000/openapi.json";
    auto httpClient = std::make_unique<HttpLibHttpClient>();
    auto openApiConfig = fetchOpenAPIConfig(specUrl, *httpClient);
    auto zsrClient = ZsrClient(openApiConfig, std::move(httpClient));
    
    // Invoke the REST endpoint
    auto response = zsr::find<zsr::ServiceMethod>("service.MyService.my_api")->call(
        zsrClient,
        zsr::make(zsr::packages(), "service.Request", {{"value", 2}})
    ).get<zsr::Introspectable>().value();
    
    // Print the response
    std::cout << "Got "
              << zsr::get(response, "value").get<int>().value()
              << std::endl;
}
```

Unlike the Python client, the C++ OpenAPI client (`ZsrClient`) is passed directly to
the endpoint method invocation, not to an intermediate zserio Client object.

**Note:** While connecting, `ZsrClient` will also pass ...
1. [Authentication headers/cookies](#http-proxies-and-authentication).
2. Additional HTTP headers passed into the `OAClient` constructor
   as key-value-pairs in an `std::map<string, string>`.

## OpenAPI Generator CLI

After installing `zswag` via pip as [described above](#for-python-users),
you can run `python -m zswag.gen`, a CLI to generate OpenAPI YAML files.
The CLI offers the following options

```
usage: Zserio OpenApi YAML Generator [-h] -s service-identifier
                                     [-p pythonpath-entry]
                                     [-c openapi-tag-expression [openapi-tag-exp
ression ...]]
                                     [-d zserio-sources] [-o OUTPUT]

optional arguments:
  -h, --help            show this help message and exit
  -s service-identifier, --service service-identifier

        Service class Python identifier. Remember that
        your service identifier is preceded by an `api`
        python module, and succeeded by `.Service`.

        Example:
            -s my.package.api.ServiceClass.Service

  -p pythonpath-entry, --path pythonpath-entry

        Path to *parent* directory of zserio Python package.
        Only needed if zserio Python package is not in the
        Python environment.

        Example:
            -p path/to/python/package/parent

  -c openapi-tag-expression, --config openapi-tag-expression

        Configuration tags for a specific or all methods.
        The argument syntax follows this pattern:

            [(service-method-name):](comma-separated-tags)

        Note: The -c argument may be applied multiple times.
        The `comma-separated-tags` must be a list of tags
        which indicate OpenApi method generator preferences:

        get|put|post|patch|delete : HTTP method tags
                  query|path|body : Parameter location tags
                        flat|blob : Flatten request object,
                                    or pass it as whole blob.

        Note:
            * The http method defaults to `post`.
            * The parameter location defaults to `query` for
              `get`, `body` otherwise.
            * The `flat` tag is only meaningful in conjunction
              with `query` or `path`.
            * An unspecific tag list (no service-method-name)
              affects the defaults only for following, not
              preceding specialized tag assignments.

        Example:
            -c post getLayerByTileId:get,flat,path

  -d zserio-sources, --docs zserio-sources

        Zserio source directory from which documentation
        should be extracted.

  -o OUTPUT, --output OUTPUT

        Output file path. If not specified, the output will be
        written to stdout.
```

### Documentation extraction

When invoking `zswag.gen` with `-d zserio-source-dir` an attempt
will be made to populate the service/method/argument/result
descriptions with doc-strings which are extracted from the zserio
sources.

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

## HTTP Proxies and Authentication

Both the Python and C++ clients read a YAML file stored under
a file given by the `HTTP_SETTINGS_FILE` environment variable.
This YAML file is a list of HTTP-related configs that are applied to HTTP 
requests based on a regular expression which matches the requested URL.
For example, the following entry would match all requests due to the `.*`
url-match-pattern:

```yaml
- url: .*
  basic-auth:
    user: johndoe
    keychain: keychain-service-string
  proxy:
    host: localhost
    port: 8888
    user: test
    keychain: ...
  cookies:
    test: value
```

**Note:** For `proxy` configs, the credentials are optional.

Passwords can be stored in clear text by setting a `password` field instead
of the `keychain` field. Keychain entries can be made with different tools
on each platform:

* [Linux `secret-tool`](https://www.marian-dan.ro/blog/storing-secrets-using-secret-tool) 
* [macOS `add-generic-password`](https://www.netmeister.org/blog/keychain-passwords.html)
* [Windows `cmdkey`](https://www.scriptinglibrary.com/languages/powershell/how-to-manage-secrets-and-passwords-with-credentialmanager-and-powershell/)


## UI 

If you have installed `pip install "connexion[swagger-ui]"`, you can view
API docs of your service under `[/prefix]/ui`.

## OpenAPI Options Interoperability

The Server, Clients and Generator currently  offer several degrees of freedom
regarding HTTP-specifics in the OpenAPI YAML file:

* **HTTP Method**
* **Parameter Format**
* **Server URL Base Path**

The following sections detail which components support which aspects
of OpenAPI. The difference in compliance is mostly due to limited
development scopes. If you are missing a particular OpenAPI feature for
a particular component, feel free to create an issue!

### HTTP method

To change the **HTTP method**, simply place the desired method
as the key under the method path, such as in the following example:
```yaml
paths:
  /methodName:
    {get|post|put|patch|delete}:
      ...
```

#### Component Support

| Feature            | C++ Client | Python Client | OAServer | zswag.gen |
| ------------------ | ---------- | ------------- | -------- | --------- |
| `get` `post` `put` `patch` `delete` | ✔️ | ✔️ | ✔️ | ✔️ |

### Request Body

A server can instruct clients to transmit their zserio request object in the
request body when using HTTP `post`, `put`, `patch` or `delete`.
This is done by settings the OpenAPI `requestBody/content` to
`application/x-zserio-object`:

```yaml
requestBody:
  content:
    application/x-zserio-object:
      schema:
        type: string
```

#### Component Support

| Feature            | C++ Client | Python Client | OAServer | zswag.gen |
| ------------------ | ---------- | ------------- | -------- | --------- |
| `application/x-zserio-object` | ✔️ | ✔️ | ✔️ | ✔️ |

### URL Blob Parameter

Zswag tools support an additional OpenAPI method parameter
field called `x-zserio-request-part`. Through this field,
a service provider can express that a certain request parameter
only contains a part of, or the whole zserio request object.
When the whole request object is contained, `x-zserio-request-part`
should be set to an asterisk (`*`):

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

**Note:** When a parameter is passed with `in=path`, its value
**must not be empty**. This holds true for strings and bytes,
but also for arrays (see below).

#### Component Support

| Feature            | C++ Client | Python Client | OAServer | zswag.gen |
| ------------------ | ---------- | ------------- | -------- | --------- |
| `x-zserio-request-part: *` | ✔️ | ✔️ | ✔️ | ✔️ |
| `format: string` | ✔️ | ✔️ | ✔️ | ✔️ |
| `format: byte` | ✔️ | ✔️ | ✔️ | ❌ |
| `format: hex` | ✔️ | ✔️ | ✔️ | ❌ |

### URL Scalar Parameter

Using `x-zserio-request-part`, it is also possible to transfer
only a single scalar (nested) member of the request object:

```yaml
parameters:
- description: ''
  in: query|path
  name: parameterName
  required: true
  x-zserio-request-part: "[parent_field.]*subfield"
  schema:
    format: string|byte|base64|base64url|hex|binary
```

In this case, `x-zserio-request-part` should point to a scalar type,
such as `uint8`, `float32`, `string` etc.

The `format` value effect remains as explained above. A small
difference exists for integer types: Their hexadecimal representation
will be the natural numeric one, not binary.

#### Component Support

| Feature            | C++ Client | Python Client | OAServer | zswag.gen |
| ------------------ | ---------- | ------------- | -------- | --------- |
| `x-zserio-request-part: <[parent.]*member>` | ✔️ | ✔️ | ✔️ | ✔️ |

### URL Array Parameter

The `x-zserio-request-part` may also point to an array member of
the zserio request struct, like so:

```yaml
parameters:
- description: ''
  in: query|path
  style: form|simple|label|matrix
  explode: true|false
  name: parameterName
  required: true
  x-zserio-request-part: "[parent_field.]*array_member"
  schema:
    format: string|byte|base64|base64url|hex|binary
```

In this case, `x-zserio-request-part` should point to an array of
scalar types. The array will be encoded according
to the [format, style and explode](https://github.com/OAI/OpenAPI-Specification/blob/master/versions/3.1.0.md#parameter-object)
specifiers.

| Feature            | C++ Client | Python Client | OAServer | zswag.gen |
| ------------------ | ---------- | ------------- | -------- | --------- |
| `x-zserio-request-part: <[parent.]*array_member>`  | ✔️ | ✔️ | ✔️ | ✔️ |
| `style: simple` | ✔️ | ✔️ | ✔️ | ✔️ |
| `style: form` | ✔️ | ✔️ | ✔️ | ✔️ |
| `style: label` | ✔️ | ✔️ | ❌ | ❌ |
| `style: matrix` | ✔️ | ✔️ | ❌ | ❌ |
| `explode: true` | ✔️ | ✔️ | ✔️ | ❌ |
| `explode: false` | ✔️ | ✔️ | ✔️ | ✔️ |

### URL Compound Parameter

In this case, `x-zserio-request-part` points to a zserio compound struct.
The OpenAPI schema options are the same as for arrays. All fields
of the designated struct which have a scalar type are exposed
as key-value pairs. We strongly discourage using this OpenAPI feature, and
tool support is very limited.

#### Component Support

| Feature            | C++ Client | Python Client | OAServer | zswag.gen |
| ------------------ | ---------- | ------------- | -------- | --------- |
| `x-zserio-request-part: <[parent.]*array_member>`  | ✔️ | ❌️ | ❌️ | ❌️ |

### Server URL Base Path

OpenAPI allows for a `servers` field in the spec that lists URL path prefixes
under which the specified API may be reached. A `zswag_client.HttpClient`
instance looks into this list and determines the URL base path it uses from
the first entry in this list. A sample entry might look as follows:

```
servers:
- http://unused-host-information/path/to/my/api
``` 

The OpenAPI client will then call methods with your specified host
and port, but prefix the `/path/to/my/api` string. 

#### Component Support

| Feature            | C++ Client | Python Client | OAServer | zswag.gen |
| ------------------ | ---------- | ------------- | -------- | --------- |
| `servers`  | ✔️ | ✔️ | ✔️ | ✔️ |
