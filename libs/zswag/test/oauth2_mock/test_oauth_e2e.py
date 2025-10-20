#!/usr/bin/env python3
"""
End-to-end test for OAuth2 Mock Server with OAuth 1.0 signature and HTTP Basic Auth.

This test demonstrates:
- Starting the OAuth2 + Zserio server as a subprocess
- Testing OAuth 1.0 signature-based token authentication
- Testing HTTP Basic Auth token authentication
- Testing OAuth2 token acquisition for OpenAPI spec fetch (useForSpecFetch flag)
- Using OAClient to call protected zserio service
- Verifying server logs show correct authentication methods
"""

import subprocess
import time
import sys
import os
import tempfile
import re
import threading
from pathlib import Path

# Import zswag.test.oauth2_mock to trigger oauth_test API generation
import zswag.test.oauth2_mock

from zswag import OAClient
import oauth_test.api as api


class TestResults:
    def __init__(self):
        self.passed = 0
        self.failed = 0
        self.tests = []

    def add(self, name, success, message=""):
        self.tests.append((name, success, message))
        if success:
            self.passed += 1
            print(f"  ✓ {name}", flush=True)
        else:
            self.failed += 1
            print(f"  ✗ {name}: {message}", flush=True)

    def summary(self):
        print()
        print("=" * 70)
        print(f"Test Results: {self.passed} passed, {self.failed} failed")
        print("=" * 70)
        return self.failed == 0


def wait_for_server(port, timeout=15):
    """Wait for server port to be open."""
    import socket
    start = time.time()
    while time.time() - start < timeout:
        try:
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            sock.settimeout(1)
            sock.connect(('127.0.0.1', port))
            sock.close()
            return True
        except (socket.error, ConnectionRefusedError):
            time.sleep(0.2)
    return False


def create_http_settings(auth_method, port, use_for_spec_fetch=True):
    """Create temporary http-settings.yaml for specified auth method."""
    # Only add useForSpecFetch if it's explicitly false (default is true)
    use_for_spec_fetch_line = ""
    if not use_for_spec_fetch:
        use_for_spec_fetch_line = "\n    useForSpecFetch: false"

    settings_content = f"""
- scope: http://127.0.0.1:{port}/*
  oauth2:
    clientId: test-client
    clientSecret: test-secret
    tokenUrl: http://127.0.0.1:{port}/oauth2/token{use_for_spec_fetch_line}
    tokenEndpointAuth:
      method: {auth_method}
      nonceLength: 16
"""
    fd, path = tempfile.mkstemp(suffix='.yaml', text=True)
    with os.fdopen(fd, 'w') as f:
        f.write(settings_content)
    return path


def test_oauth_flow(port, auth_method, results, use_for_spec_fetch=True):
    """Test complete OAuth flow with specified authentication method."""
    spec_fetch_mode = "with OAuth2 token" if use_for_spec_fetch else "without OAuth2 token"
    print(f"\n{'='*70}", flush=True)
    print(f"Testing: {auth_method} (spec fetch: {spec_fetch_mode})", flush=True)
    print(f"{'='*70}", flush=True)

    # Create temporary http settings file
    settings_file = create_http_settings(auth_method, port, use_for_spec_fetch)

    try:
        # Set environment variable
        os.environ['HTTP_SETTINGS_FILE'] = settings_file

        # Create OAClient
        test_suffix = f"{auth_method}, useForSpecFetch={use_for_spec_fetch}"
        results.add(
            f"Create OAClient ({test_suffix})",
            True
        )

        client = OAClient(f"http://127.0.0.1:{port}/openapi.json")

        # Create zserio service client
        service = api.OAuthTestService.Client(client)
        results.add(
            f"Create service client ({test_suffix})",
            True
        )

        # Call protected endpoint
        request = api.DataRequest(client_name_="TestClient")
        response = service.get_data(request)

        # Verify response
        if response.secret_value == 42 and "TestClient" in response.message:
            results.add(
                f"Call protected endpoint ({test_suffix})",
                True
            )
        else:
            results.add(
                f"Call protected endpoint ({test_suffix})",
                False,
                f"Unexpected response: {response.message}"
            )

    except Exception as e:
        results.add(
            f"OAuth flow ({test_suffix})",
            False,
            str(e)
        )
    finally:
        # Cleanup
        os.remove(settings_file)
        if 'HTTP_SETTINGS_FILE' in os.environ:
            del os.environ['HTTP_SETTINGS_FILE']


def test_public_endpoint(port, results):
    """Test public endpoint (no authentication)."""
    print(f"\n{'='*70}")
    print("Testing: Public Endpoint (no auth)")
    print(f"{'='*70}")

    import urllib.request
    import json

    try:
        url = f"http://127.0.0.1:{port}/public"
        with urllib.request.urlopen(url) as response:
            data = json.loads(response.read().decode())
            if 'message' in data and 'Public' in data['message']:
                results.add("Access public endpoint", True)
            else:
                results.add("Access public endpoint", False, "Unexpected response")
    except Exception as e:
        results.add("Access public endpoint", False, str(e))


def verify_server_logs(server_output, results):
    """Verify server logs show correct authentication methods."""
    print(f"\n{'='*70}")
    print("Verifying Server Logs")
    print(f"{'='*70}")

    # Check for OAuth 1.0 signature authentication
    if re.search(r"OAuth 1\.0 HMAC-SHA256 Signature", server_output):
        results.add("Server logged OAuth 1.0 signature auth", True)
    else:
        results.add("Server logged OAuth 1.0 signature auth", False, "Not found in logs")

    # Check for HTTP Basic Auth authentication
    if re.search(r"HTTP Basic Auth \(RFC 6749\)", server_output):
        results.add("Server logged HTTP Basic auth", True)
    else:
        results.add("Server logged HTTP Basic auth", False, "Not found in logs")

    # Check for successful token issuance
    token_count = len(re.findall(r"✓ Issued token", server_output))
    if token_count >= 2:
        results.add(f"Server issued {token_count} tokens", True)
    else:
        results.add("Server issued tokens", False, f"Only {token_count} found")

    # Check for Bearer token validation
    if re.search(r"✓ Bearer token valid", server_output):
        results.add("Server validated Bearer tokens", True)
    else:
        results.add("Server validated Bearer tokens", False, "Not found in logs")


def _start_stream_reader(stream, storage, prefix):
    """Continuously drain a stream to avoid blocking the subprocess pipes."""
    if stream is None:
        return None

    def _reader():
        for line in stream:
            storage.append(line)
            print(f"{prefix}{line}", end="", flush=True)
        stream.close()

    thread = threading.Thread(target=_reader, daemon=True)
    thread.start()
    return thread


def main():
    """Run end-to-end tests."""
    print("=" * 70, flush=True)
    print("OAuth2 Mock Server - End-to-End Test", flush=True)
    print("=" * 70, flush=True)
    print(flush=True)
    print("This test verifies:", flush=True)
    print("  • OAuth 1.0 HMAC-SHA256 signature authentication", flush=True)
    print("  • HTTP Basic Auth authentication", flush=True)
    print("  • OAuth2 token endpoint", flush=True)
    print("  • OAuth2 token acquisition for OpenAPI spec fetch (useForSpecFetch)", flush=True)
    print("  • Zserio service with Bearer token protection", flush=True)
    print("  • Public endpoint access", flush=True)
    print(flush=True)

    results = TestResults()
    server_process = None
    port = 8899

    try:
        # Start server
        print(f"Starting server on port {port}...", flush=True)
        # Ensure UTF-8 encoding and unbuffered output for subprocess (especially Windows)
        env = os.environ.copy()
        env['PYTHONIOENCODING'] = 'utf-8'
        env['PYTHONUNBUFFERED'] = '1'  # Force unbuffered output on Windows
        server_process = subprocess.Popen(
            [sys.executable, '-m', 'zswag.test.oauth2_mock', '--port', str(port)],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            encoding='utf-8',
            bufsize=1,
            env=env
        )

        stdout_lines = []
        stderr_lines = []
        stdout_thread = _start_stream_reader(server_process.stdout, stdout_lines, "[SERVER stdout] ")
        stderr_thread = _start_stream_reader(server_process.stderr, stderr_lines, "[SERVER stderr] ")

        # Wait for server to start
        if not wait_for_server(port, timeout=15):
            results.add("Server startup", False, "Server did not start in time")
            return 1

        results.add("Server startup", True)
        # Give Flask extra time to fully initialize all routes (especially on Windows)
        time.sleep(10)

        # Test OAuth 1.0 signature auth with useForSpecFetch=true (default)
        test_oauth_flow(port, "rfc5849-oauth1-signature", results, use_for_spec_fetch=True)
        time.sleep(0.5)

        # Test OAuth 1.0 signature auth with useForSpecFetch=false
        test_oauth_flow(port, "rfc5849-oauth1-signature", results, use_for_spec_fetch=False)
        time.sleep(0.5)

        # Test HTTP Basic Auth with useForSpecFetch=true (default)
        test_oauth_flow(port, "rfc6749-client-secret-basic", results, use_for_spec_fetch=True)
        time.sleep(0.5)

        # Test HTTP Basic Auth with useForSpecFetch=false
        test_oauth_flow(port, "rfc6749-client-secret-basic", results, use_for_spec_fetch=False)
        time.sleep(0.5)

        # Test public endpoint
        test_public_endpoint(port, results)

        # Give server time to flush logs
        time.sleep(0.5)

        # Get server output
        server_process.terminate()
        server_process.wait(timeout=5)
        if stdout_thread:
            stdout_thread.join(timeout=2)
        if stderr_thread:
            stderr_thread.join(timeout=2)
        server_stdout = ''.join(stdout_lines)
        server_stderr = ''.join(stderr_lines)
        server_output = server_stdout + server_stderr

        # Verify server logs
        verify_server_logs(server_output, results)

        # Show relevant server logs
        print(f"\n{'='*70}")
        print("Server Log Excerpt (authentication events)")
        print(f"{'='*70}")
        for line in server_stderr.split('\n'):
            if any(keyword in line for keyword in ['TOKEN', 'API', 'SERVICE', '✓', 'DEBUG', 'POST', 'GET']):
                print(f"  {line}")

    except KeyboardInterrupt:
        print("\n\nTest interrupted by user", flush=True)
        return 1

    except Exception as e:
        print(f"\n\nFATAL ERROR: {e}", flush=True)
        import traceback
        traceback.print_exc()
        return 1

    finally:
        # Cleanup
        if server_process:
            try:
                server_process.terminate()
                server_process.wait(timeout=5)
            except Exception:
                server_process.kill()

    # Print summary
    success = results.summary()
    return 0 if success else 1


if __name__ == '__main__':
    sys.exit(main())
