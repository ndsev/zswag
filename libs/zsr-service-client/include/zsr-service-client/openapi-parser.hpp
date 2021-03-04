// Copyright (c) Navigation Data Standard e.V. - See LICENSE file.
#pragma once

#include <istream>

#include "zsr-service-client/http-service.hpp"

namespace zsr_service
{

HTTPService::Config fetchOpenAPIConfig(const std::string& url,
                                       ndsafw::IHttpClient& client);
HTTPService::Config parseOpenAPIConfig(std::istream&);

}
