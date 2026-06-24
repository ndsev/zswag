#!/usr/bin/env python3
"""Prove that CurlHttpClient uses one TLS HTTP/2 connection with multiplexed streams."""

from __future__ import annotations

import argparse
import asyncio
import json
import ssl
import subprocess
import sys
import tempfile
import textwrap
import time
from pathlib import Path

try:
    import h2.config
    import h2.connection
    import h2.events
except ImportError:
    print("python package 'h2' is not installed; skipping HTTP/2 multiplexing test", file=sys.stderr)
    sys.exit(77)

REQUEST_COUNT = 12
RESPONSE_DELAY_SECONDS = 0.35

CERT_PEM = """
-----BEGIN CERTIFICATE-----
MIIDCTCCAfGgAwIBAgIUOr2zLuYVTXqvPcf/DckwdHZ3m2EwDQYJKoZIhvcNAQEL
BQAwFDESMBAGA1UEAwwJMTI3LjAuMC4xMB4XDTI2MDYyMzE2NDAxM1oXDTM2MDYy
MDE2NDAxM1owFDESMBAGA1UEAwwJMTI3LjAuMC4xMIIBIjANBgkqhkiG9w0BAQEF
AAOCAQ8AMIIBCgKCAQEAuaSTt1W+Fwj9RpqAybtb+XhwmDr6dbD/92sbFJTHKE3v
imB6xSl4E1RHf9tLVFkGcIJ0/IK5BxlPDWtYLxDwVj3KDJDVfC2GY8RjUgswA7Fa
pE4iTkYflDPyXp2YXmqF9Qy8ykRE9TE2w2Ri5wLVwMC4uKd57S/hwFsmzEb+tcIx
40YCnrK+/8uVPC7B2cT/RIFmtrKrNnN0wL3dv9Mno+IHgeYwLaRg7RlYBtE0Yp4u
nTj+IZqqDhDCEaLugqn4cm2z6MysL/MU4RMZllHqmec2wJAEdXhtAzFD+ILBB1WA
R2mPAMvnOBtq3AlpMqLpWIUbupVod6X+UZ+lc2Q9ywIDAQABo1MwUTAdBgNVHQ4E
FgQUzhkxUdC9W92Ent7e4E8+oawUK5swHwYDVR0jBBgwFoAUzhkxUdC9W92Ent7e
4E8+oawUK5swDwYDVR0TAQH/BAUwAwEB/zANBgkqhkiG9w0BAQsFAAOCAQEAUP1F
/EpQKWD71RspDYdOLFzzMWttE7DCI4NegkS9H9wGx6sdqPO4vAVfilN9t5YMhSDi
6P/4RkctundGgnyXGke9rVoKOSxGa9JzrhJPUmucbTKVgc12clgyfSB/EXXmLHPS
oRp8mfyCzpmQczjmxkwK6ECqfXHRHlglfBGEWb2rdGt6nqp7L9gnflwGPXcj5ULV
NLKD3jpPsshg/Ncu9QZa3RY7k2aO+8w6A2Vxy2FTi4sQeFHfeO2g+4ty07L3CTkX
Lth+SSTKcffwnkXg3AzV3fttRDkcZ/pTMcbNtXHxE/op6QvgqSaZdieLiaGg8NK7
bqVfrktrQ3KbQciPQg==
-----END CERTIFICATE-----
""".strip()

KEY_PEM = """
-----BEGIN PRIVATE KEY-----
MIIEvgIBADANBgkqhkiG9w0BAQEFAASCBKgwggSkAgEAAoIBAQC5pJO3Vb4XCP1G
moDJu1v5eHCYOvp1sP/3axsUlMcoTe+KYHrFKXgTVEd/20tUWQZwgnT8grkHGU8N
a1gvEPBWPcoMkNV8LYZjxGNSCzADsVqkTiJORh+UM/JenZheaoX1DLzKRET1MTbD
ZGLnAtXAwLi4p3ntL+HAWybMRv61wjHjRgKesr7/y5U8LsHZxP9EgWa2sqs2c3TA
vd2/0yej4geB5jAtpGDtGVgG0TRini6dOP4hmqoOEMIRou6CqfhybbPozKwv8xTh
ExmWUeqZ5zbAkAR1eG0DMUP4gsEHVYBHaY8Ay+c4G2rcCWkyoulYhRu6lWh3pf5R
n6VzZD3LAgMBAAECggEAGoxJdEWb7mxigrcepXrDP5e0i7icz7mKaWYU6dUQRBhy
d04D0Jqfvtflyl2LK2/4cUeJa6uqwolL2lCYZvwGUAxRuYlO1mLQ81yG6EPhpRg3
AlngIw8j00xjI++9S1Hy1Tqclv+7WCXREzvnFD/otJd68yvpePzIzSWGlLOvF5aD
8H2917SFcVRx6iFPZrEjPhktHZKRw2QYpZz5xlu/7DP52mKtIlCDVYwozf4aq2ru
bgGLlLgtcil4w1NGCPXRke2ePhnhRZYLroi2Xu9V+N6VlPcKFB7XUDDG3R/VOIcr
hsq2uxrW5jZTL8sutbRjmMsoQ+8zI//YID/mvdfH5QKBgQD1RPSXpcMOMymyI0AH
YymLplCJOPdI73v4t1N8FTMzsFPFOK3vcS1m+WQHNHTNKbFQmpVswrH7G+G2X/HJ
10zeDugjgEEuSAhboAfQ8y6lWJcQvBwLPMddTLqzLKJjbEY1u7xOMd1b3Phj9yBo
+rQdzEbo67KiDcQBFTu7y5kn/wKBgQDBw8xbliIBVj99+JARUfVqRRCDkXPtkPGl
rofpeMLA6i0Aq5WjM8I3P+PHIv9BQQfNzPTp80pIwGsRVewD6Gl3gM2AnByzj/2X
R/+lR+jOM7Vt07TLkb2Qdx83LvJjfJxy2+KRFAON0UbfFYCfD/kQ/t4d2ufFieRH
vUU71uEKNQKBgQDA9ByQaQHI3AtDb2Ph6+s1SAQ30C8KnA0ln+P3zB3Z3jApCewc
YSdcyXoeCPCSrugmB1bil1C5wjeR6G0pY02/rG6H71BX/qdEneNISOg7gDRoH/TY
Clq1VbXTW5vtJ7McdrMvuR7yNCbdTf+bVw/4GUr31uVThAzc5T13AjddNQKBgGXX
kbu5p1noiSqe0Kop18HpVwqwEqyU+E3K9CikjkhzTQADL30+ISCE9iWeoWcc1Qs4
ZKnqc+rVJ/FOpeRP7c8f5eNpKjS+w90VvKqUpypqRzvYgDhW+7nIwqFwjXn47wn3
xJfYWx3ZF1T9qkLwVEq4iupKOnO7TD7gnlkbUeDZAoGBAIfH7/XlgPqvXenB8E7C
14JkB8tkQchVzs01cRLc59xRoaH4QJeWqd3JPvFSvNYryJK8+H+J44WOM+mQuRzm
QjPlOtmE7fu2xYhGraCWPUzMCBoaGZVJRl0MWd1QwB6RF/wQsxoJ8RlNKyNpgzS8
Xn52ifwHRBZT1G0LHiUSaAcP
-----END PRIVATE KEY-----
""".strip()


class Stats:
    def __init__(self) -> None:
        self.connection_count = 0
        self.stream_ids: set[int] = set()
        self.active_streams = 0
        self.max_concurrent_streams = 0
        self.alpn_protocols: list[str | None] = []

    def as_dict(self) -> dict[str, object]:
        return {
            "connection_count": self.connection_count,
            "stream_count": len(self.stream_ids),
            "stream_ids": sorted(self.stream_ids),
            "max_concurrent_streams": self.max_concurrent_streams,
            "alpn_protocols": self.alpn_protocols,
        }


class H2MultiplexProtocol(asyncio.Protocol):
    def __init__(self, stats: Stats, delay: float) -> None:
        self.stats = stats
        self.delay = delay
        self.transport: asyncio.Transport | None = None
        self.conn = h2.connection.H2Connection(
            config=h2.config.H2Configuration(client_side=False, header_encoding="utf-8")
        )

    def connection_made(self, transport: asyncio.BaseTransport) -> None:
        self.transport = transport  # type: ignore[assignment]
        self.stats.connection_count += 1
        ssl_object = transport.get_extra_info("ssl_object")
        self.stats.alpn_protocols.append(
            ssl_object.selected_alpn_protocol() if ssl_object is not None else None
        )
        self.conn.initiate_connection()
        self._flush()

    def data_received(self, data: bytes) -> None:
        for event in self.conn.receive_data(data):
            if isinstance(event, h2.events.RequestReceived):
                self.stats.stream_ids.add(event.stream_id)
                self.stats.active_streams += 1
                self.stats.max_concurrent_streams = max(
                    self.stats.max_concurrent_streams, self.stats.active_streams
                )
            elif isinstance(event, h2.events.StreamEnded):
                asyncio.create_task(self._respond(event.stream_id))
            elif isinstance(event, h2.events.ConnectionTerminated):
                if self.transport is not None:
                    self.transport.close()
        self._flush()

    async def _respond(self, stream_id: int) -> None:
        await asyncio.sleep(self.delay)
        self.conn.send_headers(
            stream_id,
            [
                (":status", "200"),
                ("content-type", "text/plain"),
                ("content-length", "2"),
            ],
        )
        self.conn.send_data(stream_id, b"ok", end_stream=True)
        self.stats.active_streams -= 1
        self._flush()

    def _flush(self) -> None:
        if self.transport is not None:
            data = self.conn.data_to_send()
            if data:
                self.transport.write(data)


def create_ssl_context(tmp: Path) -> ssl.SSLContext:
    cert = tmp / "cert.pem"
    key = tmp / "key.pem"
    cert.write_text(CERT_PEM + "\n", encoding="utf-8")
    key.write_text(KEY_PEM + "\n", encoding="utf-8")

    context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    context.load_cert_chain(cert, key)
    context.set_alpn_protocols(["h2"])
    return context


async def run_test(client: Path) -> None:
    stats = Stats()
    with tempfile.TemporaryDirectory() as tmp_dir:
        ssl_context = create_ssl_context(Path(tmp_dir))
        loop = asyncio.get_running_loop()
        server = await loop.create_server(
            lambda: H2MultiplexProtocol(stats, RESPONSE_DELAY_SECONDS),
            "127.0.0.1",
            0,
            ssl=ssl_context,
        )
        sockets = server.sockets or []
        if not sockets:
            raise RuntimeError("HTTP/2 test server did not bind a socket")
        port = sockets[0].getsockname()[1]
        url = f"https://127.0.0.1:{port}/slow"

        start = time.monotonic()
        process = await asyncio.create_subprocess_exec(
            str(client),
            url,
            str(REQUEST_COUNT),
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
        )
        stdout, stderr = await process.communicate()
        elapsed = time.monotonic() - start

        server.close()
        await server.wait_closed()

    print(stdout.decode(errors="replace"), end="")
    if stderr:
        print(stderr.decode(errors="replace"), file=sys.stderr, end="")
    print(json.dumps({"elapsed_seconds": elapsed, **stats.as_dict()}, indent=2))

    if process.returncode != 0:
        raise AssertionError(f"client exited with {process.returncode}")
    if stats.alpn_protocols != ["h2"]:
        raise AssertionError(f"expected one h2 ALPN connection, got {stats.alpn_protocols}")
    if stats.connection_count != 1:
        raise AssertionError(f"expected one TCP/TLS connection, got {stats.connection_count}")
    if len(stats.stream_ids) != REQUEST_COUNT:
        raise AssertionError(f"expected {REQUEST_COUNT} streams, got {len(stats.stream_ids)}")
    if stats.max_concurrent_streams < 2:
        raise AssertionError("expected at least two concurrently active HTTP/2 streams")
    if elapsed > RESPONSE_DELAY_SECONDS * 4:
        raise AssertionError(
            f"requests took {elapsed:.3f}s; multiplexed run should stay near one delay batch"
        )


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("client", type=Path)
    args = parser.parse_args()

    if not args.client.exists():
        print(f"client executable does not exist: {args.client}", file=sys.stderr)
        return 2

    try:
        asyncio.run(run_test(args.client))
    except Exception as exc:
        print(textwrap.indent(str(exc), "ERROR: "), file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
