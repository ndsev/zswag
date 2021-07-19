# Zswag

[![CI](https://github.com/Klebert-Engineering/zswag/actions/workflows/cmake.yml/badge.svg)](https://github.com/Klebert-Engineering/zswag/actions/workflows/cmake.yml)
[![Release](https://img.shields.io/github/release/Klebert-Engineering/zswag)](https://GitHub.com/Klebert-Engineering/zswag/releases/)
[![License](https://img.shields.io/github/license/klebert-engineering/zswag)](https://github.com/Klebert-Engineering/zswag/blob/master/LICENSE)

zswag is a set of libraries for using/hosting zserio services through OpenAPI.

**Table of Contents:**

  * [Components](#components)
  * [Setup](#setup)
    + [For Python Users](#for-python-users)
    + [For C++ Users](#for-c-users)
  * [OpenAPI Generator CLI](#openapi-generator-cli)
    + [Generator Usage Example](#generator-usage-example)
    + [Documentation extraction](#documentation-extraction)
  * [Server Component (Python)](#server-component)
  * [Using the Python Client](#using-the-python-client)
  * [C++ Client](#c-client)
  * [HTTP Proxies and Authentication](#persistent-http-headers-proxy-cookie-and-authentication)
  * [Swagger User Interface](#swagger-user-interface)
  * [OpenAPI Options Interoperability](#openapi-options-interoperability)
    + [HTTP method](#http-method)
    + [Request Body](#request-body)
    + [URL Blob Parameter](#url-blob-parameter)
    + [URL Scalar Parameter](#url-scalar-parameter)
    + [URL Array Parameter](#url-array-parameter)
    + [URL Compound Parameter](#url-compound-parameter)
    + [Server URL Base Path](#server-url-base-path)
    + [Authentication Schemes](#authentication-schemes)

## Components

The zswag repository contains two main libraries which provide
OpenAPI layers for zserio Python and C++ clients. For Python, there
is even a generic zserio OpenAPI server layer.

The following UML diagram provides a more in-depth overview:

![Component Overview](doc/zswag-architecture.png)

Here are some brief descriptions of the main components:

* `zswagcl` is a C++ Library which exposes the zserio OpenAPI service client `ZsrClient`
  as well as the more generic `OpenApiClient` and `OpenApiConfig` classes
  which are reused in Python.
* `zswag` is a Python Library which provides both a zserio Python service client
  (`OAClient`) as well as a zserio-OpenAPI server layer based on Flask/Connexion
  (`OAServer`). It also contains the command-line tool `zswag.gen`, which can be
  used to generate an OpenAPI specification from a zserio Python service class.
* `pyzswagcl` is a binding library which exposes the C++-based OpenApi
  parsing/request functionality to Python. Please consider it "internal".
* `httpcl` is a wrapper around the [cpp-httplib](https://github.com/yhirose/cpp-httplib),
  and http request configuration and secret injection abilities.
  
## Setup

### For Python Users

Simply run `pip install zswag`. **Note: This only works with ...**

* 64-bit Python 3.8.x, `pip --version` >= 19.3
* 64-bit Python 3.9.x, `pip --version` >= 19.3

**Note:** On Windows, make sure that you have the *Microsoft Visual C++ Redistributable Binaries* installed. You can find the x64 installer here: https://aka.ms/vs/16/release/vc_redist.x64.exe
 
### For C++ Users

Using CMake, you can ...

* 🌟run tests.
* 🌟build the zswag wheels for a custom Python version.
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


## OpenAPI Generator CLI

After installing `zswag` via pip as [described above](#for-python-users),
you can run `python -m zswag.gen`, a CLI to generate OpenAPI YAML files.
The CLI offers the following options

```
Usage: Zserio OpenApi YAML Generator
  -s service-identifier -i zserio-or-python-path
  [-p top-level-package]
  [-c tags [tags ...]]
  [-o output]

optional arguments:
  -h, --help
  
        Show this help message and exit.
        
  -s service-identifier, --service service-identifier

        Fully qualified zserio service identifier.

        Example:
            -s my.package.ServiceClass

  -i zserio-or-python-path, --input zserio-or-python-path

        Can be either ...
        (A) Path to a zserio .zs file.
        (B) Path to parent dir of a zserio Python package.

        Examples:
            -i path/to/schema/main.zs         (A)
            -i path/to/python/package/parent  (B)

  -p top-level-package, --package top-level-package

        When -i specifies a zs file (Option A), indicate
        that a top-level zserio package name should be used.

        Examples:
            -p zserio_pkg_name

  -c tags [tags ...], --config tags [tags ...]

        Configuration tags for a specific or all methods.
        The argument syntax follows this pattern:

            [(service-method-name):](comma-separated-tags)

        Note: The -c argument may be applied multiple times.
        The `comma-separated-tags` must be a list of tags
        which indicate OpenApi method generator preferences:

        get|put|post|delete : HTTP method tags
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

  -o output, --output output

        Output file path. If not specified, the output will be
        written to stdout.
```

### Generator Usage example

Let's consider the following zserio service saved under `myapp/services.zs`:

```
package services;

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

An OpenAPI file `api.yaml` for `MyService` can now be
created with the following `zswag.gen` invocation:

```bash
cd myapp
python -m zswag.gen -s services.MyService -i services.zs -o api.yaml
```

You can further customize the generation using `-c` configuration
arguments. For example, `-c get,flat,path` will recursively "flatten"
the zserio request object into it's compound scalar fields using
[x-zserio-request-part](#url-scalar-parameter) for all methods.
If you want to change OpenAPI parameters only for one particular
method, you can prefix the tag config argument with the method
name (`-c methodName:tags...`).

### Documentation Extraction

When invoking `zswag.gen` with `-i zserio-file` an attempt
will be made to populate the service/method/request/response
descriptions with doc-strings that are extracted from the zserio
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
};
```

For service methods, a single-line doc-string is parsed which
immediately precedes the declaration:

```C
/** This method is documented. */
ReturnType myMethod(ArgumentType);
```

## Server Component

The `OAServer` component gives you the power to marry a zserio-generated app
server class with a user-written app controller and a fitting OpenAPI specification.
It is based on [Flask](https://flask.palletsprojects.com/en/1.1.x/) and
[Connexion](https://connexion.readthedocs.io/en/latest/).

### Integration Example

We consider the same `myapp` directory with a `services.zs` zserio file
as already used in the [OpenAPI Generator Example](#generator-usage-example).

**Note:**

* `myapp` must be available as a module (it must be
possible to `import myapp`). 
* We recommend to run the zserio Python generator invocation
  inside the `myapp` module's `__init__.py`, like this:

```py
import zserio
from os.path import dirname, abspath

working_dir = dirname(abspath(__file__))
zserio.generate(
  zs_dir=working_dir,
  main_zs_file="services.zs",
  gen_dir=working_dir)
```

A server script like `myapp/server.py` might then look as follows:

```py
import zswag
import myapp.controller as controller
from myapp import working_dir

# This import only works after zserio generation.
import services.api as services

app = zswag.OAServer(
  controller_module=controller,
  service_type=services.MyService.Service,
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
import services.api as services

# Written by you
def my_api(request: services.Request):
    return services.Response(request.value * 42)
```

## Using the Python Client

The generic Python client talks to any zserio service that is running
via HTTP/REST, and provides an OpenAPI specification of it's interface.

### Integration Example

As an example, consider a Python module called `myapp` which has the
same `myapp/__init__.py` and `myapp/services.zs` zserio definition as
[previously mentioned](#generator-usage-example). We consider
that the server is providing its OpenAPI spec under `localhost:5000/openapi.json`.

In this setting, a client `myapp/client.py` might look as follows:

```python
from zswag import OAClient
import services.api as services

openapi_url = "http://localhost:5000/openapi.json"

# The client reads per-method HTTP details from the OpenAPI URL.
# You can also pass a local file by setting the `is_local_file` argument
# of the OAClient constructor.
client = services.MyService.Client(OAClient(openapi_url))

# This will trigger an HTTP request under the hood.
client.my_api(services.Request(1))
```

As you can see, an instance of `OAClient` is passed into the constructor
for zserio to use as the service client's transport implementation.

**Note:** While connecting, the client will also use ...
1. [Persistent HTTP configuration](#persistent-http-headers-proxy-cookie-and-authentication).
2. Additional HTTP query/header/cookie/proxy/basic-auth configs passed
   into the `OAClient` constructor using an instance of `zswag.HTTPConfig`.
   For example:
   
   ```python
   from zswag import OAClient, HTTPConfig
   import services.api as services
   config = HTTPConfig() \
       .header(key="X-My-Header", val="value") \
       .proxy(host="localhost", port=5050, user="john", pw="doe") \
       .basic_auth(user="john", pw="doe") \
       .cookie(key="MyCookie", val="value") \
       .query(key="MyCookie", val="value")
   client = services.MyService.Client(
       OAClient("http://localhost:8080/openapi.", config))
   ```
   
   **Note:** The additional `config` will only enrich, not overwrite the
   default persistent configuration. If you would like to prevent persistent
   config from being considered at all, set `HTTP_SETTINGS_FILE` to empty,
   e.g. via `os.environ['HTTP_SETTINGS_FILE']=''`

## C++ Client

The generic C++ client talks to any zserio service that is running
via HTTP/REST, and provides an OpenAPI specification of it's interface.
The C++ client is based on the [ZSR zserio C++ reflection](https://github.com/klebert-engineering/zsr)
extension.

### Integration Example

As an example, we consider the `myapp` directory which contains a `services.zs`
zserio definition as [previously mentioned](#generator-usage-example).

We assume that zswag is added to `myapp` as a [Git submodule](https://git-scm.com/book/en/v2/Git-Tools-Submodules)
under `myapp/zswag`.

Next to `myapp/services.zs`, we place a `myapp/CMakeLists.txt` which describes our project:

```cmake
project(myapp)

# This is how C++ will know about the zswag lib
# and its dependencies, such as zserio.
add_subdirectory(zswag)

# This command is provided by ZSR to easily create
# a CMake C++ reflection library from zserio code.
add_zserio_module(${PROJECT_NAME}-cpp
  ROOT "${CMAKE_CURRENT_SOURCE_DIR}"
  ENTRY services.zs
  TOP_LEVEL_PKG services
  SUBDIR_DEPTH 0)

# We create a myapp client executable which links to
# the generated zserio C++ library, the zswag client
# library and the ZSR reflection runtime.
add_executable(${PROJECT_NAME} client.cpp)
target_link_libraries(${PROJECT_NAME}
    ${PROJECT_NAME}-cpp-reflection zswagcl zsr)
```

The `add_executable` command above references the file `myapp/client.cpp`,
which contains the code to actually use the zswag C++ client.

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
    // Assume that the server provides its OpenAPI definition here
    auto openApiUrl = "http://localhost:5000/openapi.json";
    
    // Create an HTTP client to be used by our OpenAPI client
    auto httpClient = std::make_unique<HttpLibHttpClient>();
    
    // Fetch the OpenAPI configuration using the HTTP client
    auto openApiConfig = fetchOpenAPIConfig(specUrl, *httpClient);
    
    // Create a Zserio reflection-based OpenAPI client that
    // uses the OpenAPI configuration we just retrieved.
    auto zsrClient = ZsrClient(openApiConfig, std::move(httpClient));
        
    // Use reflection to find the service method that we want to call.
    auto serviceMethod = zsr::find<zsr::ServiceMethod>("services.MyService.my_api");
    
    // Use reflection to create the request object
    auto request = zsr::make(zsr::packages(), "services.Request", {{"value", 2}});

    // Invoke the REST endpoint
    auto response = serviceMethod->call(zsrClient, request);
    
    // Unpack the response variant as an introspectable struct 
    auto unpackedResponse = response.get<zsr::Introspectable>().value();
    
    // Use reflection to read the response's value member
    auto responseValue = zsr::get(unpackedResponse, "value").get<int>().value();
    
    // Print the response
    std::cout << "Got " << responseValue << std::endl;
}
```

Unlike the Python client, the C++ OpenAPI client (`ZsrClient`) is passed directly to
the endpoint method invocation, not to an intermediate zserio Client object.

**Note:** While connecting, `HttpLibHttpClient` will also use ...
1. [Persistent HTTP configuration](#persistent-http-headers-proxy-cookie-and-authentication).
2. Additional HTTP query/header/cookie/proxy/basic-auth configs passed
   into the `ZsrClient` constructor using an instance of `httpcl::Config`.
   You can include this class via `#include "httpcl/http-settings.hpp"`.
   The additional `Config` will only enrich, not overwrite the
   default persistent configuration. If you would like to prevent persistent
   config from being considered at all, set `HTTP_SETTINGS_FILE` to empty,
   e.g. via `setenv`.

## Persistent HTTP Headers, Proxy, Cookie and Authentication

Both the Python `OAClient` and C++ `HttpLibHttpClient` read a YAML file
stored under a path which is given by the `HTTP_SETTINGS_FILE` environment
variable. The YAML file contains a list of HTTP-related configs that are
applied to HTTP requests based on a regular expression which is matched
against the requested URL.

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
    key: value
  headers:
    key: value
  query:
    key: value
```

**Note:** For `proxy` configs, the credentials are optional.

Passwords can be stored in clear text by setting a `password` field instead
of the `keychain` field. Keychain entries can be made with different tools
on each platform:

* [Linux `secret-tool`](https://www.marian-dan.ro/blog/storing-secrets-using-secret-tool) 
* [macOS `add-generic-password`](https://www.netmeister.org/blog/keychain-passwords.html)
* [Windows `cmdkey`](https://www.scriptinglibrary.com/languages/powershell/how-to-manage-secrets-and-passwords-with-credentialmanager-and-powershell/)

## Swagger User Interface 

If you have installed `pip install "connexion[swagger-ui]"`, you can view
API docs of your service under `[/prefix]/ui`.

## OpenAPI Options Interoperability

The Server, Clients and Generator offer various degrees of freedom
regarding the OpenAPI YAML file. The following sections detail which
components support which aspects of OpenAPI. The difference in compliance
is mostly due to limited development scopes. If you are missing a particular
OpenAPI feature for a particular component, feel free to create an issue!

**Note:** For all options that are not supported by `zswag.gen`, you
will need to manually edit the OpenAPI YAML file to achieve the desired
configuration. You will also need to edit the file manually to fill in
meta-info (provider name, service version, etc.).

### HTTP method

To change the **HTTP method**, the desired method name is placed 
as the key under the method path, such as in the following example:
```yaml
paths:
  /methodName:
    {get|post|put|delete}:
      ...
```

#### Component Support

| Feature            | C++ Client | Python Client | OAServer | zswag.gen |
| ------------------ | ---------- | ------------- | -------- | --------- |
| `get` `post` `put` `delete` | ✔️ | ✔️ | ✔️ | ✔️ |
| `patch` | ❌️ | ❌️ | ❌️ | ❌️ |

**Note:** Patch is unsupported, because the required semantics of
a partial object update cannot be realized in the zserio transport
layer interface.

### Request Body

A server can instruct clients to transmit their zserio request object in the
request body when using HTTP `post`, `put` or `delete`.
This is done by setting the OpenAPI `requestBody/content` to
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
When parameter contains the whole request object, `x-zserio-request-part`
should be set to an asterisk (`*`):

```yaml
parameters:
- description: ''
  in: query|path|header
  name: parameterName
  required: true
  x-zserio-request-part: "*"
  schema:
    format: string|byte|base64|base64url|hex|binary
```

About the `format` specifier value:
* Both `string` and `binary` result in a raw URL-encoded string buffer.
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
  in: query|path|header
  name: parameterName
  required: true
  x-zserio-request-part: "[parent.]*member"
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
  in: query|path|header
  style: form|simple|label|matrix
  explode: true|false
  name: parameterName
  required: true
  x-zserio-request-part: "[parent.]*array_member"
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
under which the specified API may be reached. The OpenAPI clients
looks into this list to determine a URL base path from
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
| `servers`  | ✔️ | ✔️ | ✔️ | ❌️ |

### Authentication Schemes

To facilitate the communication of authentication needs for the whole or parts
of a service, OpenAPI allows for a `securitySchemes` and `security` fields in the spec.
Please refer to the relevant parts of the [OpenAPI 3 specification](https://swagger.io/docs/specification/authentication/) for some
examples on how to integrate these fields into your spec.

Zswag currently understands the following authentication schemes:

* **HTTP Basic Authorization:** If a called endpoint requires HTTP basic auth,
  zswag will verify that the HTTP config contains basic-auth credentials.
  If there are none, zswag will throw a descriptive runtime error.
* **HTTP Bearer Authorization:** If a called endpoint requires HTTP bearer auth,
  zswag will verify that the HTTP config contains a header with the
  key name `Authorization` and the value `Bearer <token>`, *case-sensitive*.
* **API-Key Cookie:** If a called endpoint requires a Cookie API-Key,
  zswag will verify that the HTTP config contains a cookie with the
  required name, *case-sensitive*.
* **API-Key Query Parameter:** If a called endpoint requires a Query API-Key,
  zswag will verify that the HTTP config contains a query key-value pair with the
  required name, *case-sensitive*.
* **API-Key Header:** If a called endpoint requires an API-Key Header,
  zswag will verify that the HTTP config contains a header key-value pair with the
  required name, *case-sensitive*.

**Note**: If you don't want to pass your Basic-Auth/Bearer/Query/Cookie/Header
credential through your [persistent config](#persistent-http-headers-proxy-cookie-and-authentication),
you can pass a `httpcl::Config`/[`HTTPConfig`](#using-the-python-client) object to the `ZsrClient`/[`OAClient`](#using-the-python-client).
constructor in C++/Python with the relevant detail.

#### Component Support

| Feature            | C++ Client | Python Client | OAServer | zswag.gen |
| ------------------ | ---------- | ------------- | -------- | --------- |
| `HTTP Basic-Auth` `HTTP Bearer-Auth` `Cookie API-Key` `Header API-Key` `Query API-Key`  | ✔️ | ✔️ | ✔️(**) | ❌️ |
| `OpenID Connect` `OAuth2`  | ❌️ | ❌️ | ✔️(**) | ❌️ |

**(\*\*)**: The server support for all authentication schemes depends on your
configuration of the WSGI server (Apache/Nginx/...) which wraps the zswag Flask app.
