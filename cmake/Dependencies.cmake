
include(FetchContent)

# Option to enable offline builds
option(FETCHCONTENT_FULLY_DISCONNECTED "Enable offline builds" OFF)

# OpenSSL
FetchContent_Declare(
  openssl
  GIT_REPOSITORY https://github.com/openssl/openssl.git
  GIT_TAG        3.0.12
)

# zlib
FetchContent_Declare(
  zlib
  GIT_REPOSITORY https://github.com/madler/zlib.git
  GIT_TAG        v1.2.13
)

# spdlog
FetchContent_Declare(
  spdlog
  GIT_REPOSITORY https://github.com/gabime/spdlog.git
  GIT_TAG        v1.12.0
)

# yaml-cpp
FetchContent_Declare(
  yaml-cpp
  GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
  GIT_TAG        yaml-cpp-0.8.0
)

# stx
FetchContent_Declare(
  stx
  GIT_REPOSITORY https://github.com/Klebert-Engineering/stx.git
  GIT_TAG        v1.0.0
)

# speedyj
FetchContent_Declare(
  speedyj
  GIT_REPOSITORY https://github.com/Klebert-Engineering/speedyj.git
  GIT_TAG        v1.0.0
)

# catch2
FetchContent_Declare(
  catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG        v3.4.0
)

# httplib
FetchContent_Declare(
  httplib
  GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
  GIT_TAG        v2.8.1
)

# zserio-cmake-helper
FetchContent_Declare(
  zserio-cmake-helper
  GIT_REPOSITORY https://github.com/Klebert-Engineering/zserio-cmake-helper.git
  GIT_TAG        v1.0.0
)

# keychain
FetchContent_Declare(
  keychain
  GIT_REPOSITORY https://github.com/Klebert-Engineering/keychain.git
  GIT_TAG        v1.0.0
)

# pybind11
FetchContent_Declare(
  pybind11
  GIT_REPOSITORY https://github.com/pybind/pybind11.git
  GIT_TAG        v2.11.1
)

# python-cmake-wheel
FetchContent_Declare(
  python-cmake-wheel
  GIT_REPOSITORY https://github.com/Klebert-Engineering/python-cmake-wheel.git
  GIT_TAG        v1.0.0
)

# Make all dependencies available
FetchContent_MakeAvailable(
  openssl
  zlib
  spdlog
  yaml-cpp
  stx
  speedyj
  catch2
  httplib
  zserio-cmake-helper
  keychain
  pybind11
  python-cmake-wheel
)
