#!/usr/bin/env bash
#
# Integration test script for Java zswag client
# Tests the Java client against the Python Calculator server
#

set -e

# Get script directory
my_dir="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
project_root="$my_dir/../.."

# Configuration
TEST_HOST="localhost"
TEST_PORT="5555"
SERVER_START_TIMEOUT=10

echo "========================================="
echo "Java zswag Client Integration Test"
echo "========================================="
echo ""

# Check if Python zswag module is available
if ! python3 -c "import zswag.test.calc" 2>/dev/null; then
    echo "ERROR: Python zswag module not found!"
    echo ""
    echo "Please install it first:"
    echo "  pip install -r requirements.txt"
    echo "  pip install build/bin/wheel/*.whl"
    echo ""
    exit 1
fi

# Build the Java test client
echo "→ [1/4] Building Java test client..."
cd "$project_root"
./gradlew :libs:jzswag-test:build --quiet || {
    echo "ERROR: Failed to build Java test client"
    exit 1
}
echo "   ✓ Build successful"
echo ""

# Start Python server in background
echo "→ [2/4] Starting Python Calculator server on $TEST_HOST:$TEST_PORT..."
python3 -m zswag.test.calc server "$TEST_HOST:$TEST_PORT" &
SERVER_PID=$!

# Ensure server is killed on exit
trap "echo ''; echo '→ [4/4] Stopping server (PID $SERVER_PID)...'; kill $SERVER_PID 2>/dev/null; wait $SERVER_PID 2>/dev/null; echo '   ✓ Server stopped'; echo ''" EXIT

# Wait for server to start
echo "   Waiting for server to start..."
for i in $(seq 1 $SERVER_START_TIMEOUT); do
    if curl -s "http://$TEST_HOST:$TEST_PORT/openapi.json" > /dev/null 2>&1; then
        echo "   ✓ Server ready (took ${i}s)"
        break
    fi
    if [ $i -eq $SERVER_START_TIMEOUT ]; then
        echo "ERROR: Server failed to start within ${SERVER_START_TIMEOUT}s"
        exit 1
    fi
    sleep 1
done
echo ""

# Run Java test client
echo "→ [3/4] Running Java test client..."
echo "========================================="
./gradlew :libs:jzswag-test:run --quiet --args="$TEST_HOST:$TEST_PORT"
TEST_EXIT_CODE=$?
echo "========================================="
echo ""

# Check results
if [ $TEST_EXIT_CODE -eq 0 ]; then
    echo "✅ All integration tests PASSED!"
    echo ""
    exit 0
else
    echo "❌ Integration tests FAILED with exit code $TEST_EXIT_CODE"
    echo ""
    exit $TEST_EXIT_CODE
fi
