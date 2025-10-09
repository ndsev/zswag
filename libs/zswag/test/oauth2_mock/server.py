"""
OAuth2 Mock Server with zserio service, OAuth 1.0 signature and HTTP Basic Auth support.
"""

from flask import request, jsonify
from connexion.exceptions import Unauthorized
import base64
import secrets
import time
import os
from typing import Dict, Optional, Tuple
import logging

from .oauth1_validator import validate_oauth1_signature
import oauth_test.api as api

# Configure logging
logging.basicConfig(
    level=logging.INFO,
    format='[%(asctime)s] [%(name)s] %(message)s',
    datefmt='%H:%M:%S'
)
logger = logging.getLogger('oauth2-mock')

# Mock configuration
MOCK_CLIENTS = {
    'test-client': {
        'secret': 'test-secret',
        'allowed_scopes': ['read', 'write', 'admin']
    },
    'test-access-key-id': {
        'secret': 'test-access-key-secret',
        'allowed_scopes': ['read', 'write']
    }
}

# In-memory token storage
tokens: Dict[str, dict] = {}


def generate_token(client_id: str, scopes: list, expires_in: int = 3600) -> str:
    """Generate a mock Bearer token."""
    token = f"tok_{secrets.token_urlsafe(32)}"
    tokens[token] = {
        'client_id': client_id,
        'scopes': scopes,
        'expires_at': time.time() + expires_in
    }
    return token


def validate_bearer_token(token: str) -> dict:
    """
    Validate Bearer token for Connexion/OAServer.
    Called automatically by Connexion when security is required.

    Returns:
        Token info dict if valid

    Raises:
        Unauthorized: If token is invalid or expired
    """
    token_info = tokens.get(token)
    if not token_info:
        logger.warning(f"[API] Invalid Bearer token: {token[:20]}...")
        raise Unauthorized()

    if time.time() > token_info['expires_at']:
        del tokens[token]
        logger.warning(f"[API] Expired Bearer token")
        raise Unauthorized()

    logger.info(f"[API] ✓ Bearer token valid for client={token_info['client_id']}, scopes={token_info['scopes']}")
    return token_info


def token_endpoint_handler():
    """OAuth2 token endpoint supporting both Basic Auth and OAuth 1.0 signature."""
    logger.info(f"[TOKEN] Request from {request.remote_addr}")

    # NOTE: We must authenticate BEFORE accessing request.form because
    # OAuth 1.0 signature validation needs the raw request body.
    # Once request.form is accessed, the body is consumed and can't be read again.

    # Authenticate client first
    client_id, auth_method = authenticate_client(request)

    if not client_id:
        logger.error("[TOKEN] Authentication failed")
        return jsonify({'error': 'invalid_client'}), 401

    logger.info(f"[TOKEN] ✓ Authenticated via {auth_method}: client_id={client_id}")

    # Now we can safely parse request body
    grant_type = request.form.get('grant_type')
    if grant_type != 'client_credentials':
        logger.error(f"[TOKEN] Unsupported grant_type: {grant_type}")
        return jsonify({'error': 'unsupported_grant_type'}), 400

    scope = request.form.get('scope', '').split()
    audience = request.form.get('audience', '')

    logger.info(f"[TOKEN] grant_type={grant_type}, scope={scope}, audience={audience}")

    # Verify client exists
    if client_id not in MOCK_CLIENTS:
        logger.error(f"[TOKEN] Unknown client_id: {client_id}")
        return jsonify({'error': 'invalid_client'}), 401

    client_config = MOCK_CLIENTS[client_id]

    # Validate requested scopes
    if scope:
        invalid_scopes = [s for s in scope if s not in client_config['allowed_scopes']]
        if invalid_scopes:
            logger.error(f"[TOKEN] Invalid scopes requested: {invalid_scopes}")
            return jsonify({'error': 'invalid_scope'}), 400
    else:
        scope = client_config['allowed_scopes']

    # Generate token
    access_token = generate_token(client_id, scope)
    logger.info(f"[TOKEN] ✓ Issued token for client={client_id}, scopes={scope}")

    return jsonify({
        'access_token': access_token,
        'token_type': 'Bearer',
        'expires_in': 3600,
        'scope': ' '.join(scope)
    })


def authenticate_client(req) -> Tuple[Optional[str], Optional[str]]:
    """
    Authenticate client using Authorization header.
    Returns (client_id, auth_method) or (None, None) if authentication fails.
    """
    auth_header = req.headers.get('Authorization', '')

    # Try OAuth 1.0 signature
    if auth_header.startswith('OAuth '):
        logger.info("[TOKEN] Attempting OAuth 1.0 signature authentication")

        # Try each known client
        for client_id, config in MOCK_CLIENTS.items():
            full_url = request.url_root.rstrip('/') + request.path

            validated_client = validate_oauth1_signature(
                method=req.method,
                url=full_url,
                auth_header=auth_header,
                body=req.get_data(as_text=True),
                consumer_secret=config['secret']
            )

            if validated_client == client_id:
                return client_id, "OAuth 1.0 HMAC-SHA256 Signature"

        logger.warning("[TOKEN] OAuth 1.0 signature validation failed")
        return None, None

    # Try HTTP Basic Auth
    elif auth_header.startswith('Basic '):
        logger.info("[TOKEN] Attempting HTTP Basic authentication")

        try:
            b64_credentials = auth_header[6:]  # Remove "Basic " prefix
            credentials = base64.b64decode(b64_credentials).decode('utf-8')
            client_id, client_secret = credentials.split(':', 1)

            # Verify credentials
            if client_id in MOCK_CLIENTS and MOCK_CLIENTS[client_id]['secret'] == client_secret:
                return client_id, "HTTP Basic Auth (RFC 6749)"

            logger.warning(f"[TOKEN] Invalid credentials for client_id={client_id}")
        except Exception as e:
            logger.error(f"[TOKEN] Basic auth parsing error: {e}")

        return None, None

    # Try client_id in body (public client)
    else:
        logger.info("[TOKEN] Attempting public client authentication")
        client_id = req.form.get('client_id')

        if client_id and client_id in MOCK_CLIENTS:
            # Allow public clients (no secret)
            return client_id, "Public Client (no secret)"

        return None, None


def public_endpoint():
    """Public endpoint - no authentication required."""
    logger.info("[API] Public endpoint accessed")
    return jsonify({
        'message': 'Public resource - no authentication required',
        'info': 'This endpoint demonstrates public access'
    })


# ==================== Zserio Service Implementation ====================

def get_data(request: api.DataRequest) -> api.DataResponse:
    """
    Zserio service method - protected by OAuth2.
    This is called automatically by OAServer after Bearer token validation.
    """
    try:
        logger.info(f"[SERVICE] getData called with request: {request.client_name}")

        response = api.DataResponse(
            message_=f"Hello {request.client_name}! You have been authenticated.",
            secret_value_=42
        )

        logger.info(f"[SERVICE] getData returning response: {response}")
        return response
    except Exception as e:
        logger.error(f"[SERVICE] Exception in getData: {e}")
        import traceback
        traceback.print_exc()
        raise


# ==================== Server Setup ====================

def create_app():
    """Create and configure the OAServer with zserio service and OAuth2 token endpoint."""
    from zswag import OAServer
    import sys

    # Get current directory
    working_dir = os.path.dirname(os.path.abspath(__file__))

    # Create OAServer with zserio service
    server = OAServer(
        controller_module=sys.modules[__name__],  # This module
        service_type=api.OAuthTestService.Service,
        yaml_path=os.path.join(working_dir, 'oauth_test.yaml'),
        zs_pkg_path=working_dir
    )

    # Add custom OAuth2 token endpoint to the Flask app
    server.app.route('/oauth2/token', methods=['POST'])(token_endpoint_handler)

    # Add public endpoint
    server.app.route('/public', methods=['GET'])(public_endpoint)

    return server


def run_server(host='127.0.0.1', port=8080):
    """Run the OAuth2 + Zserio mock server."""
    logger.info(f"Starting OAuth2 + Zserio Mock Server on {host}:{port}")
    logger.info(f"OpenAPI spec: http://{host}:{port}/openapi.json")
    logger.info(f"Token endpoint: http://{host}:{port}/oauth2/token")
    logger.info(f"Test clients: {', '.join(MOCK_CLIENTS.keys())}")

    server = create_app()
    server.run(host=host, port=port)
