# C++ Client

The C++ client talks to any zserio service exposed via OpenAPI/REST. The relevant types live in `libs/zswagcl/` (high-level `OAClient`, `OpenApiClient`, `OpenApiConfig`) and `libs/httpcl/` (HTTP wrapper around [cpp-httplib](https://github.com/yhirose/cpp-httplib) plus OS keychain integration via [`keychain`](https://github.com/hrantzsch/keychain)).

## Requirements

- **CMake ≥ 3.22.3**
- **C++17** compiler
- The zserio C++ generator must be invoked with `-withTypeInfoCode -withReflectionCode` so the runtime reflection on which `OAClient` depends is available.

## Building

zswag uses CMake's `FetchContent` for dependencies; the basic flow is:

```bash
mkdir build && cd build
cmake ..
cmake --build .
```

For tests:

```bash
ctest --verbose
```

For wheels (Python wheels for the `zswag`/`pyzswagcl` packages — these embed parts of the C++ stack):

```bash
cmake -DZSWAG_BUILD_WHEELS=ON ..
cmake --build .
# Output under build/bin/wheel/
```

The Python environment used at CMake configure time is the one wheels are built against.

### Common build options

| Option | Default | Effect |
|---|---|---|
| `ZSWAG_BUILD_WHEELS` | `ON` | Produces wheels under `build/bin/wheel/`. |
| `ZSWAG_KEYCHAIN_SUPPORT` | `ON` | Builds OS keychain support. Set OFF on systems without `libsecret`. |
| `ZSWAG_ENABLE_TESTING` | `ON` (when top-level) | Builds and registers tests. |
| `ZSWAG_ENABLE_COVERAGE` | `OFF` | Coverage targets — Debug build, `lcov` required. Scoped to `libs/httpcl` and `libs/zswagcl`. |
| `FETCHCONTENT_FULLY_DISCONNECTED=ON` | — | Offline build (pre-fetch online first). |

For offline / disconnected builds:

```bash
# 1. Fetch dependencies once while online:
mkdir build && cd build
cmake -DFETCHCONTENT_FULLY_DISCONNECTED=OFF ..

# 2. Subsequent builds offline:
cmake -DFETCHCONTENT_FULLY_DISCONNECTED=ON ..
cmake --build .
```

Override individual deps with `-DFETCHCONTENT_SOURCE_DIR_<NAME>=/path/to/local`. Available names: `ZLIB`, `SPDLOG`, `YAML_CPP`, `STX`, `SPEEDYJ`, `HTTPLIB`, `OPENSSL`, `PYBIND11`, `PYTHON_CMAKE_WHEEL`, `ZSERIO_CMAKE_HELPER`, `KEYCHAIN`, `CATCH2`.

## Integrating into your project

In your project's `CMakeLists.txt`:

```cmake
project(myapp)

# Optional knobs for building zswag inside your project:
# set(ZSWAG_BUILD_WHEELS OFF)     # if you don't need Python wheels
# set(ZSWAG_KEYCHAIN_SUPPORT OFF)  # if libsecret isn't available

if (NOT TARGET zswag)
    FetchContent_Declare(zswag
        GIT_REPOSITORY "https://github.com/ndsev/zswag.git"
        GIT_TAG        "v1.11.1"
        GIT_SHALLOW    ON)
    FetchContent_MakeAvailable(zswag)
endif()

find_package(OpenSSL CONFIG REQUIRED)
target_link_libraries(httplib INTERFACE OpenSSL::SSL)

# zswag provides this helper to build a zserio C++ reflection library:
add_zserio_library(${PROJECT_NAME}-zserio-cpp
    WITH_REFLECTION
    ROOT "${CMAKE_CURRENT_SOURCE_DIR}"
    ENTRY services.zs
    TOP_LEVEL_PKG myapp_services)

add_executable(${PROJECT_NAME} client.cpp)
target_link_libraries(${PROJECT_NAME}
    ${PROJECT_NAME}-zserio-cpp zswagcl)
```

Note: OpenSSL is assumed to be installed or built using the `lib` (not `lib64`) directory name.

## Client usage

```cpp
#include "zswagcl/oaclient.hpp"
#include <iostream>
#include "myapp_services/services/MyService.h"

using namespace zswagcl;
using namespace httpcl;
namespace MyService = myapp_services::services::MyService;

int main(int argc, char* argv[])
{
    auto openApiUrl = "http://localhost:5000/openapi.json";

    // HTTP client to be used by OAClient
    auto httpClient = std::make_unique<HttpLibHttpClient>();

    // Fetch OpenAPI configuration
    auto openApiConfig = fetchOpenAPIConfig(openApiUrl, *httpClient);

    // Build a zserio reflection-based OpenAPI transport
    auto openApiClient = OAClient(openApiConfig, std::move(httpClient));

    // Create the typed service client (zserio-generated)
    auto myServiceClient = MyService::Client(openApiClient);

    // Make a typed call. Note: zserio C++ codegen suffixes method names with "Method".
    auto request = myapp_services::services::Request(2);
    auto response = myServiceClient.myApiMethod(request);

    std::cout << "Got " << response.getValue() << std::endl;
}
```

You can pass an adhoc `httpcl::Config` to `OAClient` (third argument) for per-instance headers, auth, and proxy:

```cpp
#include "httpcl/http-settings.hpp"

httpcl::Config adhoc;
adhoc.headers.insert({"X-Trace", "yes"});
adhoc.auth = httpcl::Config::BasicAuthentication{"alice", "secret", ""};

auto openApiClient = OAClient(openApiConfig, std::move(httpClient), adhoc);
```

The adhoc config layers on top of the [persistent settings](../README.md#http-settings-file) loaded from `HTTP_SETTINGS_FILE`.

## Code coverage

[![codecov](https://codecov.io/github/ndsev/zswag/graph/badge.svg?token=5DTX2M8IDE)](https://codecov.io/github/ndsev/zswag)

Coverage is automatically collected in CI and reported to [Codecov](https://codecov.io/gh/ndsev/zswag). Browsable HTML report at <https://ndsev.github.io/zswag/cpp/>.

Locally:

```bash
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Debug \
      -DZSWAG_ENABLE_COVERAGE=ON \
      -DZSWAG_ENABLE_TESTING=ON \
      -DZSWAG_BUILD_WHEELS=OFF \
      -DZSWAG_KEYCHAIN_SUPPORT=OFF ..
cmake --build .

ctest --output-on-failure
cmake --build . --target coverage-report
# HTML at build/coverage/html/index.html
```

Targets: `coverage-clean`, `coverage-report`, `coverage` (clean+test+report).

If you hit "gcov not found" warnings, symlink the versioned binary:

```bash
sudo ln -s /usr/bin/gcov-13 /usr/bin/gcov
```

## Persistent HTTP settings

See [HTTP Settings File in README.md](../README.md#http-settings-file). `HttpLibHttpClient` auto-loads `HTTP_SETTINGS_FILE` on construction and applies it per-request based on URL scope matching.

## OpenAPI feature support

See [the interop matrix in README.md](../README.md#openapi-options-interoperability) for the full ✅/❌ table.
