# Maps the bundled OpenSSL targets to the cache variables consumed by curl's
# nested find_package(OpenSSL) call.
function(zswag_set_openssl_discovery_variables)
    if(NOT TARGET OpenSSL::SSL OR NOT TARGET OpenSSL::Crypto)
        message(FATAL_ERROR "Bundled OpenSSL targets must exist before configuring discovery variables")
    endif()

    get_target_property(_openssl_include_dir OpenSSL::SSL INTERFACE_INCLUDE_DIRECTORIES)
    get_target_property(_openssl_ssl_library OpenSSL::SSL IMPORTED_LOCATION)
    get_target_property(_openssl_crypto_library OpenSSL::Crypto IMPORTED_LOCATION)

    set(OPENSSL_INCLUDE_DIR "${_openssl_include_dir}" CACHE PATH "OpenSSL include directory" FORCE)
    set(OPENSSL_SSL_LIBRARY "${_openssl_ssl_library}" CACHE FILEPATH "OpenSSL SSL library" FORCE)
    set(OPENSSL_CRYPTO_LIBRARY "${_openssl_crypto_library}" CACHE FILEPATH "OpenSSL crypto library" FORCE)
    set(OPENSSL_USE_STATIC_LIBS ON CACHE BOOL "Use static OpenSSL" FORCE)

    if(MSVC)
        # FindOpenSSL's MSVC branch reconstructs the public OPENSSL_* variables
        # from these legacy release variables. Seed both so curl discovers the
        # bundled libraries instead of overwriting them with NOTFOUND values.
        set(SSL_EAY_RELEASE "${_openssl_ssl_library}" CACHE FILEPATH "Bundled OpenSSL SSL library" FORCE)
        set(LIB_EAY_RELEASE "${_openssl_crypto_library}" CACHE FILEPATH "Bundled OpenSSL crypto library" FORCE)
    endif()
endfunction()
