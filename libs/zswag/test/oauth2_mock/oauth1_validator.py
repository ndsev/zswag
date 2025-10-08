"""
OAuth 1.0 signature validation (RFC 5849).
Supports HMAC-SHA256 signature method.
"""

import hmac
import hashlib
import base64
from urllib.parse import quote, unquote, parse_qs
from typing import Dict, Optional


def percent_encode(value: str) -> str:
    """RFC 3986 percent encoding for OAuth 1.0."""
    return quote(str(value), safe='')


def parse_authorization_header(auth_header: str) -> Dict[str, str]:
    """Parse OAuth Authorization header into parameters."""
    if not auth_header.startswith('OAuth '):
        raise ValueError("Not an OAuth authorization header")

    params = {}
    oauth_params = auth_header[6:]  # Remove "OAuth " prefix

    # Split by comma and parse key="value" pairs
    for param in oauth_params.split(','):
        param = param.strip()
        if '=' in param:
            key, value = param.split('=', 1)
            # Remove quotes from value
            value = value.strip('"')
            # Decode percent-encoded values
            params[key] = unquote(value)

    return params


def build_signature_base_string(
    method: str,
    url: str,
    oauth_params: Dict[str, str],
    body_params: Dict[str, str]
) -> str:
    """Build OAuth 1.0 signature base string."""
    # Combine OAuth and body parameters
    all_params = {**oauth_params, **body_params}

    # Sort parameters
    sorted_params = sorted(all_params.items())

    # Build parameter string
    param_string = '&'.join(
        f"{percent_encode(k)}={percent_encode(v)}"
        for k, v in sorted_params
    )

    # Build signature base string: METHOD&URL&PARAMS
    base_string = (
        method.upper() + '&' +
        percent_encode(url) + '&' +
        percent_encode(param_string)
    )

    return base_string


def compute_signature(
    base_string: str,
    consumer_secret: str,
    token_secret: str = ""
) -> str:
    """Compute HMAC-SHA256 signature."""
    # Build signing key: consumer_secret&token_secret
    signing_key = percent_encode(consumer_secret) + '&' + percent_encode(token_secret)

    # Compute HMAC-SHA256
    signature = hmac.new(
        signing_key.encode('utf-8'),
        base_string.encode('utf-8'),
        hashlib.sha256
    ).digest()

    # Base64 encode
    return base64.b64encode(signature).decode('utf-8')


def validate_oauth1_signature(
    method: str,
    url: str,
    auth_header: str,
    body: str,
    consumer_secret: str
) -> Optional[str]:
    """
    Validate OAuth 1.0 signature from Authorization header.

    Returns:
        consumer_key if valid, None if invalid
    """
    try:
        # Parse OAuth parameters from header
        oauth_params = parse_authorization_header(auth_header)

        # Extract signature from params
        provided_signature = oauth_params.pop('oauth_signature', None)
        if not provided_signature:
            return None

        # Verify required OAuth parameters
        required = ['oauth_consumer_key', 'oauth_signature_method',
                   'oauth_timestamp', 'oauth_nonce', 'oauth_version']
        if not all(key in oauth_params for key in required):
            return None

        # Verify signature method
        if oauth_params['oauth_signature_method'] != 'HMAC-SHA256':
            return None

        # Parse body parameters (application/x-www-form-urlencoded)
        body_params = {}
        if body:
            for key, value in parse_qs(body).items():
                body_params[key] = value[0] if value else ''

        # Build signature base string
        base_string = build_signature_base_string(
            method, url, oauth_params, body_params
        )

        # Compute expected signature
        expected_signature = compute_signature(base_string, consumer_secret)

        # Compare signatures
        if hmac.compare_digest(provided_signature, expected_signature):
            return oauth_params['oauth_consumer_key']

        return None

    except Exception:
        return None
