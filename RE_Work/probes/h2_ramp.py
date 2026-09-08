"""Ramp probe: for each burst size N, open a fresh h2 connection, send N HEADERS
at once, and measure how many complete within the window. Finds the per-connection
in-flight cliff on the G HUB endpoint."""
import socket
import ssl
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
BURSTS = [1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024]
WINDOW = 8.0  # seconds per phase


def run_burst(N):
    ctx = ssl.create_default_context()
    ctx.set_alpn_protocols(["h2"])
    raw = socket.create_connection((HOST, 443), timeout=15)
    tls = ctx.wrap_socket(raw, server_hostname=HOST)
    tls.setblocking(True)
    tls.settimeout(1.0)
    conn = H2Connection(H2Configuration(client_side=True, header_encoding="utf-8"))
    conn.initiate_connection()
    tls.sendall(conn.data_to_send())

    inflight = {}
    for i in range(N):
        sid = conn.get_next_available_stream_id()
        conn.send_headers(sid, [
            (":method", "GET"), (":scheme", "https"), (":authority", HOST),
            (":path", f"/pipeline/v2/update/ghub10/win/zz{i}/update.json"),
            ("user-agent", "LGHUB/2026.6.957899 (Windows NT 10.0; x64)"),
            ("accept", "*/*"),
        ], end_stream=True)
        inflight[sid] = [0, 0]
    tls.sendall(conn.data_to_send())

    done = resets = goaway = 0
    status_counts = {}
    t_end = time.time() + WINDOW
    while time.time() < t_end and inflight:
        try:
            data = tls.recv(65536)
        except socket.timeout:
            continue
        except (ssl.SSLError, OSError):
            break
        if not data:
            break
        try:
            for event in conn.receive_data(data):
                if isinstance(event, ResponseReceived):
                    st = inflight.get(event.stream_id)
                    if st:
                        st[0] = dict(event.headers).get(":status", 0)
                elif isinstance(event, DataReceived):
                    st = inflight.get(event.stream_id)
                    if st:
                        st[1] += len(event.data)
                    if event.flow_controlled_length:
                        conn.acknowledge_received_data(event.flow_controlled_length, event.stream_id)
                elif isinstance(event, StreamEnded):
                    st = inflight.pop(event.stream_id, None)
                    if st:
                        done += 1
                        status_counts[st[0]] = status_counts.get(st[0], 0) + 1
                elif isinstance(event, StreamReset):
                    inflight.pop(event.stream_id, None)
                    resets += 1
                    done += 1
                elif isinstance(event, ConnectionTerminated):
                    goaway = 1
        except ProtocolError:
            break
        if conn.data_to_send():
            tls.sendall(conn.data_to_send())
    elapsed = time.time() - (t_end - WINDOW)
    rate = done / elapsed if elapsed > 0 else 0
    print(
        f"burst N={N:>5}: done={done:>5} rate={rate:>7.0f}/s resets={resets} "
        f"goaway={goaway} pending={len(inflight)} statuses={status_counts}",
        flush=True,
    )
    try:
        tls.close()
    except OSError:
        pass


for N in BURSTS:
    run_burst(N)
