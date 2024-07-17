from enum import Enum
import calculator.api as api
from zswag import OAClient, HTTPConfig
import json
import pickle

def run(host, port):

    server_url = f"http://{host}:{port}/openapi.json"
    counter = 0
    failed = 0
    print(f"[py-test-client] Connecting to {server_url}", flush=True)

    # Make sure that HTTP Config pickling works
    config_pickled = pickle.dumps(HTTPConfig().header("x", "42"))
    assert pickle.loads(config_pickled)

    def run_test(aspect, request, fn, expect, auth_args):
        nonlocal counter, failed
        counter += 1
        try:
            print(f"[py-test-client] Test#{counter}: {aspect}", flush=True)
            print(f"[py-test-client]   -> Instantiating client.", flush=True)
            oa_client = OAClient(f"http://{host}:{port}/openapi.json", **auth_args, server_index=0)
            # Just make sure that OpenAPI JSON content is parsable
            assert oa_client.config().content and json.loads(oa_client.config().content)
            client = api.Calculator.Client(oa_client)
            print(f"[py-test-client]   -> Running request.", flush=True)
            resp = fn(client, request)
            if resp.value == expect:
                print(f"[py-test-client]   -> Success.", flush=True)
            else:
                raise ValueError(f"Expected {expect}, got {resp.value}!")
        except Exception as e:
            failed += 1
            print(f"[py-test-client]   -> ERROR: {str(e) or type(e).__name__}", flush=True)

    run_test(
        "Pass fields in path and header",
        api.BaseAndExponent(api.I32(2), api.I32(3)),
        api.Calculator.Client.power,
        8.,
        {})

    run_test(
        "Pass hex-encoded array in query",
        api.Integers([100, -200, 400]),
        api.Calculator.Client.int_sum,
        300.,
        {
            "config": HTTPConfig().header("Authorization", "Bearer 123")
        })

    run_test(
        "Pass base64url-encoded byte array in path",
        api.Bytes([8, 16, 32, 64]),
        api.Calculator.Client.byte_sum,
        120.,
        {
            "config": HTTPConfig().basic_auth("u", "pw")
        })

    run_test(
        "Pass base64-encoded long array in path",
        api.Integers([1, 2, 3, 4]),
        api.Calculator.Client.int_mul,
        24.,
        {
            "config": HTTPConfig().query("api-key", "42")
        })

    run_test(
        "Pass float array in query.",
        api.Doubles([34.5, 2.]),
        api.Calculator.Client.float_mul,
        69.,
        {
            "api_key": "42"
        })

    run_test(
        "Pass bool array in query (expect false).",
        api.Bools([True, False]),
        api.Calculator.Client.bit_mul,
        False,
        {
            "api_key": "42"
        })

    run_test(
        "Pass bool array in query (expect true).",
        api.Bools([True, True]),
        api.Calculator.Client.bit_mul,
        True,
        {
            "config": HTTPConfig().header("X-Generic-Token", "42")
        })

    run_test(
        "Pass request as blob in body",
        api.Double(1.),
        api.Calculator.Client.identity,
        1.,
        {
            "config": HTTPConfig().cookie("api-cookie", "42")
        })

    run_test(
        "Pass base64-encoded strings.",
        api.Strings(["foo", "bar"]),
        api.Calculator.Client.concat,
        "foobar",
        {
            "bearer": "123"
        })

    run_test(
        "Pass enum.",
        api.EnumWrapper(api.Enum.TEST_ENUM_0),
        api.Calculator.Client.name,
        "TEST_ENUM_0",
        {
            "config": HTTPConfig().api_key("42")
        })

    if failed > 0:
        print(f"[py-test-client] Done, {failed} test(s) failed!", flush=True)
        exit(1)
    else:
        print(f"[py-test-client] All tests succeeded.!", flush=True)
        exit(0)

