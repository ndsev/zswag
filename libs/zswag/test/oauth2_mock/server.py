"""
OAuth2 Mock Server with OAuth 1.0 signature and HTTP Basic Auth support.
"""

from flask import Flask, request, jsonify
from functools import wraps
import base64
import secrets
import time
from typing import Dict, Optional, Tuple
import logging

from .oauth1_validator import validate_oauth1_signature

# Configure logging
logging.basicConfig(
    level=logging.INFO,
    format='[%(asctime)s] [%(name)s] %(message)s',
    datefmt='%H:%M:%S'
)
logger = logging.getLogger('oauth2-mock')

app = Flask(__name__)

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


def validate_bearer_token(token: str) -> Optional[dict]:
    """Validate Bearer token and return token info if valid."""
    token_info = tokens.get(token)
    if not token_info:
        return None

    if time.time() > token_info['expires_at']:
        del tokens[token]
        return None

    return token_info


def require_bearer_token(required_scopes: list = None):
    """Decorator to require valid Bearer token."""
    def decorator(f):
        @wraps(f)
        def decorated_function(*args, **kwargs):
            auth_header = request.headers.get('Authorization', '')

            if not auth_header.startswith('Bearer '):
                logger.warning("[API] Missing or invalid Authorization header")
                return jsonify({'error': 'unauthorized'}), 401

            token = auth_header[7:]  # Remove "Bearer " prefix
            token_info = validate_bearer_token(token)

            if not token_info:
                logger.warning("[API] Invalid or expired Bearer token")
                return jsonify({'error': 'invalid_token'}), 401

            # Check scopes if required
            if required_scopes:
                if not any(scope in token_info['scopes'] for scope in required_scopes):
                    logger.warning(f"[API] Insufficient scopes. Required: {required_scopes}, Got: {token_info['scopes']}")
                    return jsonify({'error': 'insufficient_scope'}), 403

            logger.info(f"[API] Bearer token valid for client={token_info['client_id']}, scopes={token_info['scopes']}")
            return f(*args, **kwargs)

        return decorated_function
    return decorator


@app.route('/oauth2/token', methods=['POST'])
def token_endpoint():
    """OAuth2 token endpoint supporting both Basic Auth and OAuth 1.0 signature."""
    logger.info(f"[TOKEN] Request from {request.remote_addr}")

    # Parse request body
    grant_type = request.form.get('grant_type')
    if grant_type != 'client_credentials':
        logger.error(f"[TOKEN] Unsupported grant_type: {grant_type}")
        return jsonify({'error': 'unsupported_grant_type'}), 400

    scope = request.form.get('scope', '').split()
    audience = request.form.get('audience', '')

    logger.info(f"[TOKEN] grant_type={grant_type}, scope={scope}, audience={audience}")

    # Authenticate client
    client_id, auth_method = authenticate_client(request)

    if not client_id:
        logger.error("[TOKEN] Authentication failed")
        return jsonify({'error': 'invalid_client'}), 401

    logger.info(f"[TOKEN] ✓ Authenticated via {auth_method}: client_id={client_id}")

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


@app.route('/api/protected', methods=['GET', 'POST'])
@require_bearer_token(required_scopes=['read'])
def protected_endpoint():
    """Protected API endpoint requiring Bearer token."""
    return jsonify({
        'message': 'Access granted to protected resource',
        'data': {'value': 42}
    })


@app.route('/api/public', methods=['GET'])
def public_endpoint():
    """Public API endpoint (no authentication required)."""
    logger.info("[API] Public endpoint accessed")
    return jsonify({
        'message': 'Public resource',
        'data': {'info': 'No authentication required'}
    })


@app.route('/openapi.json', methods=['GET'])
def openapi_spec():
    """Serve OpenAPI specification."""
    base_url = request.url_root.rstrip('/')

    spec = {
        "openapi": "3.0.0",
        "info": {
            "title": "OAuth2 Mock API",
            "version": "1.0.0",
            "description": "Mock OAuth2 server supporting OAuth 1.0 signature and HTTP Basic Auth"
        },
        "servers": [
            {"url": base_url}
        ],
        "paths": {
            "/api/protected": {
                "get": {
                    "summary": "Protected endpoint",
                    "security": [{"oauth2": ["read"]}],
                    "responses": {
                        "200": {"description": "Success"},
                        "401": {"description": "Unauthorized"}
                    }
                }
            },
            "/api/public": {
                "get": {
                    "summary": "Public endpoint",
                    "responses": {
                        "200": {"description": "Success"}
                    }
                }
            }
        },
        "components": {
            "securitySchemes": {
                "oauth2": {
                    "type": "oauth2",
                    "flows": {
                        "clientCredentials": {
                            "tokenUrl": f"{base_url}/oauth2/token",
                            "scopes": {
                                "read": "Read access",
                                "write": "Write access",
                                "admin": "Admin access"
                            }
                        }
                    }
                }
            }
        }
    }

    return jsonify(spec)


@app.route('/', methods=['GET'])
def index():
    """Server info page."""
    return jsonify({
        'name': 'OAuth2 Mock Server',
        'version': '1.0.0',
        'endpoints': {
            'token': '/oauth2/token',
            'protected': '/api/protected',
            'public': '/api/public',
            'openapi': '/openapi.json'
        },
        'supported_auth': [
            'OAuth 1.0 HMAC-SHA256 Signature',
            'HTTP Basic Auth',
            'Public Client (no secret)'
        ],
        'test_clients': list(MOCK_CLIENTS.keys())
    })


def run_server(host='127.0.0.1', port=8080):
    """Run the OAuth2 mock server."""
    logger.info(f"Starting OAuth2 Mock Server on {host}:{port}")
    logger.info(f"OpenAPI spec: http://{host}:{port}/openapi.json")
    logger.info(f"Test clients: {', '.join(MOCK_CLIENTS.keys())}")
    app.run(host=host, port=port, debug=False)
