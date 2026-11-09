#pragma once

#include <string>

namespace zswagcl
{

/** Encode a borrowed byte range with standard alphabet and = padding; return owned text. */
std::string base64_encode(unsigned char const* bytes_to_encode, unsigned int in_len);

/** Encode with the URL-safe alphabet; padding is retained. */
std::string base64url_encode(unsigned char const* bytes_to_encode, unsigned int in_len);

/** Decode the valid prefix, stopping at padding or a non-alphabet byte; not strict validation. */
std::string base64_decode(std::string const& encoded_string);

/** As base64_decode, using the URL-safe alphabet. */
std::string base64url_decode(std::string const& encoded_string);
}
