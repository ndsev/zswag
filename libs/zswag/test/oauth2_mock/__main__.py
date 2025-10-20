"""
CLI entry point for OAuth2 Mock Server.

Usage:
    python -m zswag.test.oauth2_mock [--host HOST] [--port PORT]
"""

import argparse
from .server import run_server


def main():
    parser = argparse.ArgumentParser(
        description='OAuth2 Mock Server for testing OAuth 1.0 signature and HTTP Basic Auth'
    )
    parser.add_argument(
        '--host',
        default='127.0.0.1',
        help='Host to bind to (default: 127.0.0.1)'
    )
    parser.add_argument(
        '--port',
        type=int,
        default=8080,
        help='Port to bind to (default: 8080)'
    )

    args = parser.parse_args()

    print("=" * 70)
    print("OAuth2 + Zserio Mock Server")
    print("=" * 70)
    print()
    print("This server provides:")
    print("  • OAuth2 token endpoint with OAuth 1.0 signature support")
    print("  • Zserio service protected by OAuth2 Bearer tokens")
    print("  • Public endpoint (no authentication)")
    print()
    print("Supported authentication methods:")
    print("  • OAuth 1.0 HMAC-SHA256 Signature (RFC 5849)")
    print("  • HTTP Basic Auth (RFC 6749)")
    print("  • Public clients (no secret)")
    print()
    print("Test clients configured:")
    print("  • client_id: test-client")
    print("    secret:    test-secret")
    print("    scopes:    read, write, admin")
    print()
    print("  • client_id: test-access-key-id")
    print("    secret:    test-access-key-secret")
    print("    scopes:    read, write")
    print()
    print("Endpoints:")
    print(f"  • Token:     http://{args.host}:{args.port}/oauth2/token")
    print(f"  • Service:   http://{args.host}:{args.port}/getData (requires Bearer token)")
    print(f"  • Public:    http://{args.host}:{args.port}/public")
    print(f"  • OpenAPI:   http://{args.host}:{args.port}/openapi.json")
    print()
    print("=" * 70)
    print()

    run_server(host=args.host, port=args.port)


if __name__ == '__main__':
    main()
