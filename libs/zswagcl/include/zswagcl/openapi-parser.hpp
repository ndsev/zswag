// Copyright (c) Navigation Data Standard e.V. - See LICENSE file.
#pragma once

#include <istream>

#include "zswagcl/http-service.hpp"

namespace zswagcl
{

HTTPService::Config fetchOpenAPIConfig(const std::string& url,
                                       httpcl::IHttpClient& client);
HTTPService::Config parseOpenAPIConfig(std::istream&);

}
