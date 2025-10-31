# Zswag

[![CI](https://github.com/ndsev/zswag/actions/workflows/build-deploy.yml/badge.svg)](https://github.com/ndsev/zswag/actions/workflows/build-deploy.yml)
[![codecov](https://codecov.io/github/ndsev/zswag/graph/badge.svg?token=5DTX2M8IDE)](https://codecov.io/github/ndsev/zswag)
[![Quality Gate Status](https://sonarcloud.io/api/project_badges/measure?project=ndsev_zswag&metric=alert_status)](https://sonarcloud.io/summary/new_code?id=ndsev_zswag)
[![Release](https://img.shields.io/github/release/ndsev/zswag)](https://GitHub.com/ndsev/zswag/releases/)
[![Coverage](https://sonarcloud.io/api/project_badges/measure?project=ndsev_zswag&metric=coverage)](https://sonarcloud.io/summary/new_code?id=ndsev_zswag)
[![License](https://img.shields.io/github/license/ndsev/zswag)](https://github.com/ndsev/zswag/blob/master/LICENSE)

zswag is a set of libraries for using/hosting zserio services through OpenAPI.

**Table of Contents:**

  * [Components](#components)
  * [Setup](#setup)
    + [For Python Users](#for-python-users)
    + [For C++ Users](#for-c-users)
      - [Offline/Disconnected Builds](#offlinedisconnected-builds)
  * [CI/CD and Release Process](#cicd-and-release-process)
    + [Continuous Integration](#continuous-integration)
    + [Release Process](#release-process)
    + [Development Snapshots](#development-snapshots)
    + [Version Validation](#version-validation)
  * [OpenAPI Generator CLI](#openapi-generator-cli)
    + [Generator Usage Example](#generator-usage-example)
    + [Documentation extraction](#documentation-extraction)
  * [Server Component (Python)](#server-component)
  * [Using the Python Client](#using-the-python-client)
  * [C++ Client](#c-client)
  * [Client Environment Settings](#client-environment-settings)
  * [HTTP Proxies and Authentication](#persistent-http-headers-proxy-cookie-and-authentication)
  * [Swagger User Interface](#swagger-user-interface)
  * [Client Result Code Handling](#client-result-code-handling)
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

* `zswagcl` is a C++ Library which exposes the zserio OpenAPI service client `OAClient`
  as well as the more generic `OpenApiClient` and `OpenApiConfig` classes.
  The latter two are reused for the Python client library.
* `zswag` is a Python Library which provides both a zserio Python service client
  (`OAClient`) as well as a zserio-OpenAPI server layer based on Flask/Connexion
  (`OAServer`). It also contains the command-line tool `zswag.gen`, which can be
  used to generate an OpenAPI specification from a zserio Python service class.
* `pyzswagcl` is a binding library which exposes the C++-based OpenApi
  parsing/request functionality to Python. **Please consider it "internal".**
* `httpcl` is a wrapper around [cpp-httplib](https://github.com/yhirose/cpp-httplib),
  HTTP request configuration and OS secret storage abilities based on
  the [keychain](https://github.com/hrantzsch/keychain) library.
  
## Setup

### For Python Users

Simply run `pip install zswag`. **Note: This only works with ...**

* 64-bit Python 3.10-3.13, `pip --version` >= 19.3
* Supported platforms: Linux (x86_64), macOS (x86_64 and arm64), Windows (x64)

**Notes:**
* On Windows, make sure that you have the *Microsoft Visual C++ Redistributable Binaries* installed. You can find the x64 installer here: https://aka.ms/vs/16/release/vc_redist.x64.exe
* zswag for Python 3.10 is not supported on Apple Silicon (arm64) because no compatible GitHub Actions runner is available.
  However, this is typically not an issue, as macOS includes more recent Python versions by default.

### For C++ Users

Using CMake, you can ...

* 🌟run tests.
* 🌟build the zswag wheels for a custom Python version.
* 🌟[integrate the C++ client into a C++ project.](#c-client)

Dependencies are managed via CMake's `FetchContent` mechanism. Make sure you have a recent version of CMake (>= 3.22.3) installed.

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

#### Offline/Disconnected Builds

For environments without internet access or for reproducible builds, zswag supports offline builds using CMake's FetchContent mechanism.

**Offline Build Process**

For offline builds, you can pre-fetch all dependencies while online and then build without network access:

```bash
# First, fetch all dependencies while online
mkdir build && cd build
cmake -DFETCHCONTENT_FULLY_DISCONNECTED=OFF ..
# This will download all dependencies

# Then build offline
cmake -DFETCHCONTENT_FULLY_DISCONNECTED=ON ..
cmake --build .
```

The `FETCHCONTENT_FULLY_DISCONNECTED=ON` option tells CMake to use only the pre-fetched dependencies and never attempt network access.

**Local Development with Custom Dependencies**

For development, you can override specific dependencies with local sources:
```bash
mkdir build && cd build
cmake -DFETCHCONTENT_SOURCE_DIR_SPDLOG=/path/to/local/spdlog ..
cmake --build .
```

Available override variables:
- `FETCHCONTENT_SOURCE_DIR_ZLIB` - zlib compression library
- `FETCHCONTENT_SOURCE_DIR_SPDLOG` - spdlog logging library  
- `FETCHCONTENT_SOURCE_DIR_YAML_CPP` - yaml-cpp parsing library
- `FETCHCONTENT_SOURCE_DIR_STX` - stx utility library
- `FETCHCONTENT_SOURCE_DIR_SPEEDYJ` - speedyj JSON library
- `FETCHCONTENT_SOURCE_DIR_HTTPLIB` - cpp-httplib HTTP library
- `FETCHCONTENT_SOURCE_DIR_OPENSSL` - OpenSSL cryptography library
- `FETCHCONTENT_SOURCE_DIR_PYBIND11` - pybind11 (when `ZSWAG_BUILD_WHEELS=ON`)
- `FETCHCONTENT_SOURCE_DIR_PYTHON_CMAKE_WHEEL` - python-cmake-wheel (when `ZSWAG_BUILD_WHEELS=ON`)
- `FETCHCONTENT_SOURCE_DIR_ZSERIO_CMAKE_HELPER` - zserio build helpers
- `FETCHCONTENT_SOURCE_DIR_KEYCHAIN` - keychain library (when `ZSWAG_KEYCHAIN_SUPPORT=ON`)
- `FETCHCONTENT_SOURCE_DIR_CATCH2` - Catch2 testing framework (when `ZSWAG_ENABLE_TESTING=ON`)

**Build Options**

Common build configuration options:
```bash
# Minimal build (no wheels, no keychain, no tests)
cmake -DZSWAG_BUILD_WHEELS=OFF -DZSWAG_KEYCHAIN_SUPPORT=OFF -DZSWAG_ENABLE_TESTING=OFF ..

# Offline build with custom spdlog
cmake -DFETCHCONTENT_FULLY_DISCONNECTED=ON -DFETCHCONTENT_SOURCE_DIR_SPDLOG=/path/to/spdlog ..

# Development build with wheels enabled
cmake -DZSWAG_BUILD_WHEELS=ON -DZSWAG_ENABLE_TESTING=ON ..
```

#### Code Coverage

[![codecov](https://codecov.io/gh/ndsev/zswag/branch/master/graph/badge.svg)](https://codecov.io/gh/ndsev/zswag)

zswag includes comprehensive C++ test coverage analysis for the `httpcl` and `zswagcl` libraries. Coverage is automatically collected in CI and reported to [Codecov](https://codecov.io/gh/ndsev/zswag).

**Local Coverage Analysis**

To generate coverage reports locally, you'll need:
- GCC or Clang compiler
- `lcov` and `genhtml` tools (install with `sudo apt-get install lcov` on Ubuntu/Debian)

Build with coverage enabled:
```bash
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Debug \
      -DZSWAG_ENABLE_COVERAGE=ON \
      -DZSWAG_ENABLE_TESTING=ON \
      -DZSWAG_BUILD_WHEELS=OFF \
      -DZSWAG_KEYCHAIN_SUPPORT=OFF ..
cmake --build .
```

**Note:** The flags `-DZSWAG_BUILD_WHEELS=OFF` and `-DZSWAG_KEYCHAIN_SUPPORT=OFF` disable features that aren't needed for coverage analysis and avoid requiring Python development headers or system keychain libraries.

Generate coverage reports:
```bash
# Run tests to generate coverage data
ctest --output-on-failure

# Generate HTML coverage report
cmake --build . --target coverage-report
```

The HTML coverage report will be available at `build/coverage/html/index.html`.

**Available Coverage Targets:**
- `coverage-clean` - Remove all coverage data
- `coverage-report` - Generate coverage report from existing test runs
- `coverage` - Clean, run tests, and generate report (all-in-one)

**Coverage Configuration:**
- **Goal:** 70%+ line coverage (initial), near 100% line and branch coverage (ultimate)
- **Scope:** Coverage is tracked only for library source files (`libs/httpcl`, `libs/zswagcl`)
- **Not included:** Test code, dependencies, generated code (zserio)

**Troubleshooting Coverage Builds:**

If you get "gcov not found" warnings:
```bash
# Check if versioned gcov exists
which gcov-13 gcov-12 gcov-11

# Create symlink (example for gcov-13)
sudo ln -s /usr/bin/gcov-13 /usr/bin/gcov

# Or install gcc package
sudo apt-get install gcc
```

If you want to build coverage **with** wheel support (requires Python development headers):
```bash
cmake -DCMAKE_BUILD_TYPE=Debug \
      -DZSWAG_ENABLE_COVERAGE=ON \
      -DZSWAG_ENABLE_TESTING=ON \
      -DZSWAG_BUILD_WHEELS=ON \
      -DZSWAG_KEYCHAIN_SUPPORT=OFF ..
```
Note: This requires `python3-dev` package (Ubuntu/Debian) or equivalent on your system.

## CI/CD and Release Process

### Continuous Integration

The project uses GitHub Actions for automated building and deployment:

- **Platforms**: Linux (x86_64), macOS (Intel x86_64 and Apple Silicon arm64), Windows (x64)
- **Python versions**: 3.10, 3.11, 3.12, 3.13
- **Triggers**: Pull requests, pushes to main branch, and version tags

### Release Process

Releases are automated through the CI/CD pipeline:

1. **Update version**: Modify `ZSWAG_VERSION` in `CMakeLists.txt`
2. **Create release tag**: Tag the commit with `v{version}` (e.g., `v1.7.2`)
3. **Automatic deployment**: The CI pipeline will:
   - Validate that the tag version matches the CMake version
   - Build wheels for all supported platforms
   - Deploy to PyPI automatically

### Development Snapshots

Pushes to the main branch automatically create development releases:
- Version format: `{base_version}.dev{commit_count}` (e.g., `1.7.2.dev3`)
- Automatically deployed to PyPI for testing

### Version Validation

The build process ensures version consistency:
- Git tags must match the version in `CMakeLists.txt`
- Mismatched versions will cause the build to fail
- This prevents accidental deployment of incorrect versions

## OpenAPI Generator CLI

After installing `zswag` via pip as [described above](#for-python-users),
you can run `python -m zswag.gen`, a CLI to generate OpenAPI YAML files.
The CLI offers the following options

```
usage: Zserio OpenApi YAML Generator [-h] -s service-identifier -i
                                     zserio-or-python-path
                                     [-r zserio-src-root-dir]
                                     [-p top-level-package] [-c tags [tags ...]]
                                     [-o output] [-b BASE_CONFIG_YAML]

optional arguments:
  -h, --help
        show this help message and exit
  -s service-identifier, --service service-identifier

        Fully qualified zserio service identifier.

        Example:
            -s my.package.ServiceClass

  -i zserio-or-python-path, --input zserio-or-python-path

        Can be either ...
        (A) Path to a zserio .zs file. Must be either a top-
            level entrypoint (e.g. all.zs), or a subpackage
            (e.g. services/myservice.zs) in conjunction with
            a "--zserio-source-root|-r <dir>" argument.
        (B) Path to parent dir of a zserio Python package.

        Examples:
            -i path/to/schema/main.zs         (A)
            -i path/to/python/package/parent  (B)

  -r zserio-src-root-dir, --zserio-source-root zserio-src-root-dir

        When -i specifies a zs file (Option A), indicate the
        directory for the zserio -src directory argument. If
        not specified, the parent directory of the zs file
        will be used.

  -p top-level-package, --package top-level-package

        When -i specifies a zs file (Option A), indicate
        that a specific top-level zserio package name
        should be used.

        Examples:
            -p zserio_pkg_name

  -c tags [tags ...], --config tags [tags ...]

        Configuration tags for a specific or all methods.
        The argument syntax follows this pattern:

           [(service-method-name):](comma-separated-tags)

        Note: The -c argument may be applied multiple times.
        The `comma-separated-tags` must be a list of tags
        which indicate OpenApi method generator preferences.
        The following tags are supported:

        get|put|post|delete : HTTP method tags
                query|path| : Parameter location tags
                header|body
                  flat|blob : Flatten request object,
                              or pass it as whole blob.
          (param-specifier) : Specify parameter name, format
                              and location for a specific
                              request-part. See below.
            security=(name) : Set a particular security
                              scheme to be used. The scheme
                              details must be provided through
                              the --base-config-yaml.
         path=(method-path) : Set a particular method path.
                              May contain placeholders for
                              path params.

        A (param-specifier) tag has the following schema:

            (field?name=...
                  &in=[path|body|query|header]
                  &format=[binary|base64|hex]
                  [&style=...]
                  [&explode=...])

        Examples:

          Expose all methods as POST, but `getLayerByTileId`
          as GET with flat path-parameters:

            `-c post getLayerByTileId:get,flat,path`

          For myMethod, put the whole request blob into the a
          query "data" parameter as base64:

            `-c myMethod:*?name=data&in=query&format=base64`

          For myMethod, set the "AwesomeAuth" auth scheme:

            `-c myMethod:security=AwesomeAuth`

          For myMethod, provide the path and place myField
          explicitely in a path placeholder:

            `-c 'myMethod:path=/my-method/{param},...
                 myField?name=param&in=path&format=string'`

        Note:
            * The HTTP-method defaults to `post`.
            * The parameter 'in' defaults to `query` for
              `get`, `body` otherwise.
            * If a method uses a parameter specifier, the
              `flat`, `body`, `query`, `path`, `header` and
              `body`-tags are ignored.
            * The `flat` tag is only meaningful in conjunction
              with `query` or `path`.
            * An unspecific tag list (no service-method-name)
              affects the defaults only for following, not
              preceding specialized tag assignments.

  -o output, --output output

        Output file path. If not specified, the output will be
        written to stdout.

  -b BASE_CONFIG_YAML, --base-config-yaml BASE_CONFIG_YAML

        Base configuration file. Can be used to fully or partially
        substitute --config arguments, and to provide additional
        OpenAPI information. The YAML file must look like this:

          method: # Optional method tags dictionary
            <method-name|*>: <list of config tags>
          securitySchemes: ... # Optional OpenAPI securitySchemes
          info: ...            # Optional OpenAPI info section
          servers: ...         # Optional OpenAPI servers section
          security: ...        # Optional OpenAPI global security
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
  Response myApi(Request);
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

**Implementation choice regarding HTTP response codes:** The server as implemented
here will return HTTP code `400` (Bad Request) when the user request could not
be parsed, and `500` (Internal Server Error) when a different exception occurred while
generating the response/running the user's controller implementation.

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
       .header(key="X-My-Header", val="value") \  # Can be specified 
       .cookie(key="MyCookie", val="value")    \  # multiple times.
       .query(key="MyCookie", val="value")     \  # 
       .proxy(host="localhost", port=5050, user="john", pw="doe") \
       .basic_auth(user="john", pw="doe") \
       .bearer("bearer-token") \
       .api_key("token")
   
   client = services.MyService.Client(
       OAClient("http://localhost:8080/openapi.", config=config))
   
   # Alternative when specifying api-key or bearer
   client = services.MyService.Client(
       OAClient("http://localhost:8080/openapi.", api_key="token", bearer="token"))
   ```
   
   **Note:** The additional `config` will only enrich, not overwrite the
   default persistent configuration. If you would like to prevent persistent
   config from being considered at all, set `HTTP_SETTINGS_FILE` to empty,
   e.g. via `os.environ['HTTP_SETTINGS_FILE']=''`

## C++ Client

The generic C++ client talks to any zserio service that is running
via HTTP/REST, and provides an OpenAPI specification of its interface.
When using the C++ `OAClient` with your zserio schema, make sure
that the flags [`-withTypeInfoCode` and `-withReflectionCode`](http://zserio.org/doc/ZserioUserGuide.html#zserio-command-line-interface) are passed to the zserio C++ emitter.

### Integration Example

As an example, we consider the `myapp` directory which contains a `services.zs`
zserio definition as [previously mentioned](#generator-usage-example).

We assume that zswag is added to `myapp` as a [Git submodule](https://git-scm.com/book/en/v2/Git-Tools-Submodules)
under `myapp/zswag`.

Next to `myapp/services.zs`, we place a `myapp/CMakeLists.txt` which describes our project:

```cmake
project(myapp)

# If you are not interested in building zswag Python
# wheels, you can set the following option:
# set(ZSWAG_BUILD_WHEELS OFF)

# If your compilation environment does not provide
# libsecret, the following switch will disable keychain integration:
# set(ZSWAG_KEYCHAIN_SUPPORT OFF)

# Optional: For offline/disconnected builds, you can
# predefine dependency sources using FETCHCONTENT_SOURCE_DIR_*
# variables (see README offline builds section for details)

# This is how C++ will know about the zswag lib
# and its dependencies, such as zserio.
if (NOT TARGET zswag)
        FetchContent_Declare(zswag
                GIT_REPOSITORY "https://github.com/ndsev/zswag.git"
                GIT_TAG        "v1.6.7"
                GIT_SHALLOW    ON)
        FetchContent_MakeAvailable(zswag)
endif()

find_package(OpenSSL CONFIG REQUIRED)
target_link_libraries(httplib INTERFACE OpenSSL::SSL)

# This command is provided by zswag to easily create
# a CMake C++ reflection library from zserio code.
add_zserio_library(${PROJECT_NAME}-zserio-cpp
  WITH_REFLECTION
  ROOT "${CMAKE_CURRENT_SOURCE_DIR}"
  ENTRY services.zs
  TOP_LEVEL_PKG myapp_services)

# We create a myapp client executable which links to
# the generated zserio C++ library and the zswag client
# library.
add_executable(${PROJECT_NAME} client.cpp)

# Make sure to link to the `zswagcl` target
target_link_libraries(${PROJECT_NAME}
    ${PROJECT_NAME}-zserio-cpp zswagcl)
```

**Note:** OpenSSL is assumed to be installed or built using the `lib` (not `lib64`) directory name.

The `add_executable` command above references the file `myapp/client.cpp`,
which contains the code to actually use the zswag C++ client.

```cpp
#include "zswagcl/oaclient.hpp"
#include <iostream>
#include "myapp_services/services/MyService.h"

using namespace zswagcl;
using namespace httpcl;
namespace MyService = myapp_services::services::MyService;

int main (int argc, char* argv[])
{
    // Assume that the server provides its OpenAPI definition here
    auto openApiUrl = "http://localhost:5000/openapi.json";
    
    // Create an HTTP client to be used by our OpenAPI client
    auto httpClient = std::make_unique<HttpLibHttpClient>();
    
    // Fetch the OpenAPI configuration using the HTTP client
    auto openApiConfig = fetchOpenAPIConfig(openApiUrl, *httpClient);
    
    // Create a Zserio reflection-based OpenAPI client that
    // uses the OpenAPI configuration we just retrieved.
    auto openApiClient = OAClient(openApiConfig, std::move(httpClient));
        
    // Create a MyService client based on the OpenApi-Client
    // implementation of the zserio::IServiceClient interface.
    auto myServiceClient = MyService::Client(openApiClient);
    
    // Create the request object
    auto request = myapp_services::services::Request(2);

    // Invoke the REST endpoint. Mind that your method-
    // name from the schema is appended with a "...Method" suffix.
    auto response = myServiceClient.myApiMethod(request);
    
    // Print the response
    std::cout << "Got " << response.getValue() << std::endl;
}
```

**Note:** While connecting, `HttpLibHttpClient` will also use ...
1. [Persistent HTTP configuration](#persistent-http-headers-proxy-cookie-and-authentication).
2. Additional HTTP query/header/cookie/proxy/basic-auth configs passed
   into the `OAClient` constructor using an instance of `httpcl::Config`.
   You can include this class via `#include "httpcl/http-settings.hpp"`.
   The additional `Config` will only enrich, not overwrite the
   default persistent configuration. If you would like to prevent persistent
   config from being considered at all, set `HTTP_SETTINGS_FILE` to empty,
   e.g. via `setenv`.

## Client Environment Settings

Both the Python and C++ Clients can be configured using the following
environment variables:

| Variable Name | Details   |
| ------------- | --------- |
| `HTTP_SETTINGS_FILE` | Path to settings file for HTTP proxies and authentication, see [next section](#persistent-http-headers-proxy-cookie-and-authentication) |
| `HTTP_LOG_LEVEL` | Verbosity level for console/log output. Set to `debug` for detailed output. |
| `HTTP_LOG_FILE` | Logfile-path (including filename) to redirect console output. The log will rotate with three files (`HTTP_LOG_FILE`, `HTTP_LOG_FILE-1`, `HTTP_LOG_FILE-2`). |
| `HTTP_LOG_FILE_MAXSIZE` | Maximum size of the logfile, in bytes. Defaults to 1GB. |
| `HTTP_TIMEOUT` | Timeout for HTTP requests (connection+transfer) in seconds. Defaults to 60s. |
| `HTTP_SSL_STRICT` | Set to any nonempty value for strict SSL certificate validation. |

## Persistent HTTP Headers, Proxy, Cookie and Authentication

Both the Python `OAClient` and C++ `HttpLibHttpClient` read a YAML file
stored under a path which is given by the `HTTP_SETTINGS_FILE` environment
variable. The YAML file contains a list of HTTP-related configs that are
applied to HTTP requests based on a regular expression which is matched
against the requested URL.

For example, the following entry would match all requests due to the `*`
url-match-pattern for the `scope` field:

```yaml
http-settings:
  # Under http-settings, a list of settings is defined for specific URL scopes.
  - scope: *     # URL scope - e.g. https://*.nds.live/* or *.google.com.
    basic-auth:  # Basic auth credentials for matching requests.
      user: johndoe
      keychain: keychain-service-string
      password: cleartext-password  # alternative to keychain
    proxy:      # Proxy settings for matching requests.
      host: localhost
      port: 8888
      user: test
      keychain: ...
      password: cleartext-password  # alternative to keychain
    cookies:    # Additional Cookies for matching requests.
      key: value
    headers:    # Additional Headers for matching requests.
      key: value
    query:      # Additional Query parameters for matching requests.
      key: value
    api-key: value  # API Key as required by OpenAPI config - see description below.
    oauth2:
      # REQUIRED fields
      clientId: my-client-id                               # REQUIRED: OAuth2 client identifier

      # REQUIRED if useForSpecFetch=true (default), OPTIONAL otherwise
      tokenUrl: https://issuer.example.com/oauth/token     # Token endpoint URL (see precedence rules below)

      # Client secret (choose one method)
      clientSecretKeychain: keychain-service-string        # RECOMMENDED: Load secret from OS keychain
      clientSecret: cleartext-secret                       # DISCOURAGED: Cleartext secret (use keychain instead)

      # OPTIONAL fields (with defaults/precedence)
      refreshUrl: https://issuer.example.com/oauth/token   # Optional override; defaults to refreshUrl from OpenAPI, then tokenUrl
      audience: https://api.example.com/                   # Optional: audience parameter (required by some providers)
      scope: ["orders.read", ...]                          # Optional: scope override; defaults to OpenAPI spec's per-operation scopes
      useForSpecFetch: true                                # Optional: acquire token before fetching OpenAPI spec (default: true)
      tokenEndpointAuth:                                   # Optional: token endpoint authentication method
        method: rfc6749-client-secret-basic                # Options: rfc6749-client-secret-basic (default), rfc5849-oauth1-signature
        nonceLength: 16                                    # For rfc5849-oauth1-signature: nonce length (8-64, default: 16)
```

**Note:** For `proxy` configs, the credentials are optional.

#### OAuth2 Configuration: Required vs Optional Fields

**Important:** Zswag only supports the **OAuth2 `clientCredentials` flow**. Other flows (`authorizationCode`, `implicit`, `password`) are not supported.

**Field Requirements:**

| Field | Required? | Notes |
|-------|-----------|-------|
| `clientId` | ✅ Always | OAuth2 client identifier |
| `tokenUrl` | ⚠️ Conditional | **REQUIRED** when `useForSpecFetch: true` (default)<br>**OPTIONAL** when `useForSpecFetch: false` (defaults to OpenAPI spec) |
| `clientSecret` / `clientSecretKeychain` | ⚠️ Conditional | **REQUIRED** for confidential clients<br>**OPTIONAL** for public clients (if omitted, client_id is sent in request body) |
| `refreshUrl` | ❌ Optional | Defaults to `refreshUrl` from OpenAPI spec, then `tokenUrl` |
| `scope` | ❌ Optional | Defaults to scopes from OpenAPI spec's per-operation `security` requirements |
| `audience` | ❌ Optional | Only required by some OAuth2 providers |
| `useForSpecFetch` | ❌ Optional | Default: `true` (acquire token before fetching OpenAPI spec) |
| `tokenEndpointAuth` | ❌ Optional | Default: `rfc6749-client-secret-basic` |

**Precedence Rules (http-settings.yaml vs OpenAPI spec):**

When both http-settings.yaml and the OpenAPI specification provide values, the following precedence applies:

1. **`tokenUrl`**: http-settings.yaml `tokenUrl` **overrides** OpenAPI spec's `flows.clientCredentials.tokenUrl`
2. **`refreshUrl`**: http-settings.yaml `refreshUrl` **overrides** OpenAPI spec's `flows.clientCredentials.refreshUrl`
3. **`scope`**: http-settings.yaml `scope` **overrides** OpenAPI spec's per-operation `security` scopes

**Common Scenarios:**

| Scenario | `useForSpecFetch` | `tokenUrl` in http-settings | `tokenUrl` in OpenAPI spec | Result |
|----------|-------------------|----------------------------|---------------------------|--------|
| **Protected OpenAPI spec** | `true` (default) | ✅ Required | Used as fallback | http-settings value used |
| **Public OpenAPI spec** | `false` | ❌ Optional | ✅ Required in spec | OpenAPI spec value used |
| **Override spec settings** | `true` or `false` | ✅ Provided | Any | http-settings value **always wins** |

#### OAuth2 Token Endpoint Authentication Methods

The `tokenEndpointAuth` field controls how the client authenticates when requesting OAuth2 tokens. Two methods are supported:

**`rfc6749-client-secret-basic` (default):** HTTP Basic Authentication (RFC 6749 Section 2.3.1)
- Both `clientId` and `clientSecret` are sent in the `Authorization: Basic` header
- Works with most OAuth2 providers

**`rfc5849-oauth1-signature`:** OAuth 1.0 HMAC-SHA256 request signing (RFC 5849)
- `clientId` is sent as `oauth_consumer_key` in both header and body
- `clientSecret` is used only for HMAC-SHA256 signature computation (never transmitted)
- Required by some providers that use OAuth 1.0 signature-based token authentication
- Provides enhanced security through cryptographic request signing
- **Note:** Only HMAC-SHA256 signature method is supported

#### OAuth2-Authenticated OpenAPI Spec Fetching

By default, when OAuth2 is configured, zswag will acquire an OAuth2 access token **before** fetching the OpenAPI specification. This solves the "chicken-and-egg" problem where the OpenAPI spec endpoint itself requires authentication.

**How it works:**

1. **With `useForSpecFetch: true` (default):**
   - Client acquires OAuth2 token using configured authentication method
   - Token is included as `Authorization: Bearer <token>` header when fetching OpenAPI spec
   - OpenAPI spec fetch succeeds even if endpoint requires authentication
   - Same token is cached and reused for subsequent API calls

2. **With `useForSpecFetch: false`:**
   - OpenAPI spec is fetched without OAuth2 token (plain HTTP GET)
   - OAuth2 token acquisition is deferred until first API method call
   - Use this when the OpenAPI spec endpoint is public (doesn't require authentication)

**Configuration:**

```yaml
- scope: https://api.example.com/*
  oauth2:
    clientId: your-client-id
    clientSecret: your-client-secret
    tokenUrl: https://api.example.com/oauth/token
    useForSpecFetch: true  # Default: true. Set to false if spec is public.
```

**When to use `useForSpecFetch: false`:**
- OpenAPI spec endpoint is publicly accessible
- Avoids unnecessary token acquisition if only the spec is needed
- Improves performance by deferring OAuth2 flow until first API call

**When to keep `useForSpecFetch: true` (default):**
- OpenAPI spec endpoint requires authentication
- Service returns 401/403 when fetching spec without credentials
- Most secure option (ensures token is available from the start)

**Debugging OAuth2 Issues:**

To troubleshoot OAuth2 authentication problems, enable detailed logging:

```bash
export HTTP_LOG_LEVEL=debug   # Shows OAuth2 flow (token acquisition, cache hits/misses)
export HTTP_LOG_LEVEL=trace   # Shows additional details (request/response bodies, signatures)
```

The logs will show:
- Token endpoint authentication method being used
- Token request/response status
- Token cache hit/miss/expired events
- OAuth2 configuration status for OpenAPI spec fetch
- Whether OAuth2 token is being used for spec fetch

#### Testing OAuth 1.0 Signature with Your Service

To verify OAuth 1.0 signature authentication with your service:

**1. Install zswag:**

For official releases:
```bash
pip install zswag
```

For custom builds or development snapshots:
```bash
pip install /path/to/pyzswagcl-*.whl /path/to/zswag-*.whl
```

**2. Create `http-settings.yaml`** with your service details:
```yaml
- scope: https://your-api.example.com/*
  oauth2:
    clientId: your-client-id
    clientSecret: your-client-secret
    tokenUrl: https://your-api.example.com/oauth/token
    tokenEndpointAuth:
      method: rfc5849-oauth1-signature
      nonceLength: 16
```

**3. Create a test script** (`test_oauth1.py`):
```python
import os
from zswag import OAClient

# Point to your http-settings file
os.environ['HTTP_SETTINGS_FILE'] = '/path/to/http-settings.yaml'

# Create client with your OpenAPI spec
client = OAClient("https://your-api.example.com/openapi.json")

# Import your generated zserio service
import your_service.api as api

# Create service client and make a test call
service = api.YourService.Client(client)
request = api.YourRequest(...)
response = service.your_method(request)

print(f"Success! Response: {response}")
```

**4. Run the test:**
```bash
python test_oauth1.py
```

**Verification:** Check your server logs to confirm OAuth 1.0 HMAC-SHA256 signatures are being validated correctly and the token endpoint receives properly signed requests.

The **`api-key`** setting will be applied under the correct
cookie/header/query parameter, if the service
you are connecting to uses an [OpenAPI `apiKey` auth scheme](#authentication-schemes).

Passwords can be stored in clear text by setting a `password` field instead
of the `keychain` field. Keychain entries can be made with different tools
on each platform:

* [Linux `secret-tool`](https://www.marian-dan.ro/blog/storing-secrets-using-secret-tool) 
* [macOS `add-generic-password`](https://www.netmeister.org/blog/keychain-passwords.html)
* [Windows `cmdkey`](https://www.scriptinglibrary.com/languages/powershell/how-to-manage-secrets-and-passwords-with-credentialmanager-and-powershell/)

## Client Result Code Handling

Both clients (Python and C++) will treat any HTTP response code other than `200` as an error since zserio services are expected to return a parsable response object. The client will throw an exception with a descriptive message if the response code is not `200`.

In case applications want to utilize for example the `204 (No Content)` response code, they have to catch the exception and handle it accordingly.

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
| `format: byte` | ✔️ | ✔️ | ✔️ | ✔️ |
| `format: hex` | ✔️ | ✔️ | ✔️ | ✔️ |

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
| `style: label` | ✔️ | ✔️ | ❌ | ✔️ |
| `style: matrix` | ✔️ | ✔️ | ❌ | ✔️ |
| `explode: true` | ✔️ | ✔️ | ✔️ | ✔️ |
| `explode: false` | ✔️ | ✔️ | ✔️ | ✔️ |

### URL Compound Parameter

In this case, `x-zserio-request-part` points to a zserio compound struct
instead of a field with a scalar value. **This is currently not supported.**

#### Component Support

| Feature            | C++ Client | Python Client | OAServer | zswag.gen |
| ------------------ | ---------- | ------------- | -------- | --------- |
| `x-zserio-request-part: <[parent.]*compound_member>`  | ❌️ | ❌️ | ❌️ | ❌️ |

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
| `servers`  | ✔️ | ✔️ | ✔️ | ✔️ |

### Authentication Schemes

To facilitate the communication of authentication needs for the whole or parts
of a service, OpenAPI allows for `securitySchemes` and `security` fields in the spec.
Please refer to the relevant parts of the [OpenAPI 3 specification](https://swagger.io/docs/specification/authentication/) for some
examples on how to integrate these fields into your spec.

#### When Security Schemes Are Applied

**Important:** Security schemes (including OAuth2) are **only applied when explicitly declared** in the OpenAPI specification. Zswag clients respect the security requirements defined in the spec according to the [OpenAPI 3.0 Security Requirement specification](https://spec.openapis.org/oas/v3.0.3#security-requirement-object).

**Security Configuration Levels:**

1. **Global Security** - Applied to all endpoints by default (root-level `security` field):
   ```yaml
   components:
     securitySchemes:
       HeaderAuth:
         type: apiKey
         in: header
         name: X-Generic-Token

   security:
     - HeaderAuth: []  # Applied to all endpoints by default

   paths:
     /methodWithGlobalAuth:
       get:
         # Uses global HeaderAuth
   ```

2. **Per-Endpoint Security** - Override global security for specific operations:
   ```yaml
   paths:
     /protected:
       post:
         security:
           - oauth2: [read, write]  # Overrides global security
     /admin:
       post:
         security:
           - oauth2: [admin]  # Different scopes for admin endpoint
   ```

3. **No Authentication** - Explicitly disable security for public endpoints:
   ```yaml
   paths:
     /public:
       get:
         security: []  # Explicitly no authentication required (overrides global)
   ```

**Precedence Rule:** Per-operation `security` settings **override** global `security` settings. If an operation specifies its own security requirements (including `security: []`), the global security configuration is ignored for that operation.

**Complete Working Examples:**

- **OAuth2 with per-endpoint security**: See [`libs/zswagcl/test/testdata/oauth2-openapi.yaml`](libs/zswagcl/test/testdata/oauth2-openapi.yaml) which demonstrates different OAuth2 scopes per endpoint and public endpoints without authentication.
- **Global security with overrides**: See [`libs/zswag/test/calc/api.yaml`](libs/zswag/test/calc/api.yaml) which shows global `HeaderAuth` security with per-endpoint overrides and explicit no-auth declarations.

Zswag currently understands the following authentication schemes:

* **HTTP Basic Authorization:** If a called endpoint requires HTTP basic auth,
  zswag will verify that the HTTP config contains basic-auth credentials.
  If there are none, zswag will throw a descriptive runtime error.
* **HTTP Bearer Authorization:** If a called endpoint requires HTTP bearer auth,
  zswag will verify that the HTTP config contains a header with the
  key name `Authorization` and the value `Bearer <token>`, *case-sensitive*.
* **API-Key Cookie:** If a called endpoint requires a Cookie API-Key,
  zswag will either apply [the `api-key` setting](#persistent-http-headers-proxy-cookie-and-authentication), or verify that the
  HTTP config contains a cookie with the required name, *case-sensitive*.
* **API-Key Query Parameter:** If a called endpoint requires a Query API-Key,
  zswag will either apply the `api-key` setting, or verify that the
  HTTP config contains a query key-value pair with the required name, *case-sensitive*.
* **API-Key Header:** If a called endpoint requires an API-Key Header,
  zswag will either apply the `api-key` setting, or verify that the
  HTTP config contains a header key-value pair with the required name, *case-sensitive*.
* **OAuth2 Client Credentials:** If a called endpoint requires OAuth2 authentication,
  zswag will **automatically acquire, cache, and refresh** access tokens from the configured
  OAuth2 token endpoint. The client handles the entire OAuth2 client credentials flow
  transparently, including token expiry and refresh. **Note:** Only the `clientCredentials`
  flow is supported. See the [OAuth2 configuration section](#persistent-http-headers-proxy-cookie-and-authentication)
  for detailed setup instructions.

**Note**: If you don't want to pass your Basic-Auth/Bearer/Query/Cookie/Header
credential through your [persistent config](#persistent-http-headers-proxy-cookie-and-authentication),
you can pass a `httpcl::Config`/[`HTTPConfig`](#using-the-python-client) object to the `OAClient`/[`OAClient`](#using-the-python-client).
constructor in C++/Python with the relevant detail.

#### Component Support

| Feature                                                                                | C++ Client | Python Client | OAServer | zswag.gen |
|----------------------------------------------------------------------------------------| ---------- | ------------- | -------- | --------- |
| `HTTP Basic-Auth` `HTTP Bearer-Auth` `Cookie API-Key` `Header API-Key` `Query API-Key` | ✔️ | ✔️ | ✔️(**) | ✔️ |
| `OAuth2[clientCredentials]`                                                            | ✔️ | ✔️ | ✔️(**) | ✔️ |
| `OpenID Connect` `OAuth2[authorizationCode]` `OAuth2[implicit]` `OAuth2[password]`     | ❌️ | ❌️ | ✔️(**) | ❌️ |

**(\*\*)**: The server support for all authentication schemes depends on your
configuration of the WSGI server (Apache/Nginx/...) which wraps the zswag Flask app.
