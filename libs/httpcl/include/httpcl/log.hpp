#pragma once

#include "spdlog/spdlog.h"

namespace httpcl
{

/**
 * Obtain the process logger, initialized once on first use. HTTP_LOG_LEVEL is
 * case-insensitive; HTTP_LOG_FILE selects a rotating file instead of stderr,
 * and HTTP_LOG_FILE_MAXSIZE is bytes (default 1 GiB, two rotated files).
 * Later environment changes do not reconfigure the existing logger.
 */
spdlog::logger& log();

/**
 * Log a runtime error and return the throwable object.
 * @param what Runtime error message.
 * @return std::runtime_error to throw.
 */
template<typename error_t = std::runtime_error>
error_t logRuntimeError(std::string const& what) {
    log().error(what);
    return error_t(what);
}

}
