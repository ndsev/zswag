#pragma once

#include <string>

namespace zswagcl
{

std::string base64_encode(unsigned char const* bytes_to_encode,
                          unsigned int in_len);
std::string base64url_encode(unsigned char const* bytes_to_encode,
                             unsigned int in_len);

std::string base64_decode(std::string const& encoded_string);
std::string base64url_decode(std::string const& encoded_string);

}
