"""Single-connection HTTP/2 concurrency probe for the G HUB update endpoint.

Usage: python h2_py_probe.py [inflight] [seconds]
"""
import select
import socket
import ssl
import sys
import time

from h2.connection import H2Connection
from h2.config import H2Configuration
from h2.exceptions import ProtocolError
from h2.events import (
    ConnectionTerminated,
    DataReceived,
    ResponseReceived,
    StreamEnded,
    StreamReset,
)

HOST = "updates.ghub.logitechg.com"
N = int(sys.argv[1]) if len(sys.argv) > 1 else 1024
DURATION = float(sys.argv[2]) if len(sys.argv) > 2 else 15.0


def build_conn():
    ctx = ssl.create_default_context()
    ctx.set_alpn_protocols(["h2"])
    raw = socket.create_connection((HOST, 443), timeout=15)
    tls = ctx.wrap_socket(raw, server_hostname=HOST)
    assert tls.selected_alpn_protocol() == "h2", tls.selected_alpn_protocol()
    tls.setblocking(True)
    tls.settimeout(0.5)
    conn = H2Connection(H2Configuration(client_side=True, header_encoding="utf-8"))
    conn.initiate_connection()
    tls.sendall(conn.data_to_send())
    return tls, conn


def main():
    inflight = {}  # stream_id -> [status, body_bytes, t0]
    issued = finished = errors = ok200 = other = resets = 0
    t_end = time.time() + DURATION
    tls, conn = build_conn()
    print(f"connected, ALPN={tls.selected_alpn_protocol()}, in-flight target={N}")

    def issue():
        nonlocal issued
        while len(inflight) < N and time.time() < t_end:
            sid = conn.get_next_available_stream_id()
            path = f"/pipeline/v2/update/ghub10/win/zz{issued}/update.json"
            headers = [
                (":method", "GET"),
                (":scheme", "https"),
                (":authority", HOST),
                (":path", path),
                ("user-agent", "LGHUB/2026.6.957899 (Windows NT 10.0; x64)"),
                ("accept", "*/*"),
            ]
            conn.send_headers(sid, headers, end_stream=True)
            inflight[sid] = [0, 0, time.time()]
            issued += 1
        if conn.data_to_send():
            tls.sendall(conn.data_to_send())

    def handle(data):
        nonlocal finished, errors, ok200, other, resets
        for event in conn.receive_data(data):
            if isinstance(event, ResponseReceived):
                inflight[event.stream_id][0] = dict(event.headers).get(":status", 0)
            elif isinstance(event, DataReceived):
                st = inflight.get(event.stream_id)
                if st:
                    st[1] += len(event.data)
                if event.flow_controlled_length:
                    conn.acknowledge_received_data(event.flow_controlled_length, event.stream_id)
            elif isinstance(event, StreamEnded):
                st = inflight.pop(event.stream_id, None)
                if st:
                    finished += 1
                if st and st[0] == 200:
                    ok200 += 1
                elif st:
                    other += 1
            elif isinstance(event, StreamReset):
                inflight.pop(event.stream_id, None)
                resets += 1
                finished += 1
            elif isinstance(event, ConnectionTerminated):
                pass
        if conn.data_to_send():
            tls.sendall(conn.data_to_send())

    issue()
    while time.time() < t_end:
        try:
            data = tls.recv(65536)
        except socket.timeout:
            pass
        except (ssl.SSLError, OSError) as e:
            print("read error:", e)
            break
        else:
            if not data:
                print("connection closed by server")
                break
            try:
                handle(data)
            except ProtocolError as e:
                print("protocol error:", e)
                break
        issue()

    elapsed = time.time() - (t_end - DURATION)
    rate = finished / elapsed if elapsed > 0 else 0
    print(
        f"finished={finished} rate={rate:.0f}/s ok200={ok200} other={other} "
        f"resets={resets} errors={errors} issued={issued}"
    )
    try:
        tls.close()
    except OSError:
        pass


if __name__ == "__main__":
    main()
