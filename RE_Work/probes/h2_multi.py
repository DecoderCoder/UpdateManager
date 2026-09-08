"""Multi-connection HTTP/2 scaling probe.

Each connection runs in its own thread, holding S streams in flight continuously
(issuing replacements as streams complete). Measures aggregate rate.

Usage: python h2_multi.py [connections] [streams_per_conn] [seconds]
"""
import socket
import ssl
import threading
import time

from h2.connection import H2Connection
from h2.config import H2Configuration
from h2.events import (
    ConnectionTerminated,
    DataReceived,
    ResponseReceived,
    StreamEnded,
    StreamReset,
)

HOST = "updates.ghub.logitechg.com"
import sys

K = int(sys.argv[1]) if len(sys.argv) > 1 else 16
S = int(sys.argv[2]) if len(sys.argv) > 2 else 128
DURATION = float(sys.argv[3]) if len(sys.argv) > 3 else 15.0

lock = threading.Lock()
stats = {"done": 0, "ok200": 0, "other": 0, "resets": 0, "goaway": 0, "errors": 0}


def conn_worker(idx, result):
    try:
        ctx = ssl.create_default_context()
        ctx.set_alpn_protocols(["h2"])
        raw = socket.create_connection((HOST, 443), timeout=15)
        tls = ctx.wrap_socket(raw, server_hostname=HOST)
        tls.setblocking(True)
        tls.settimeout(0.5)
        conn = H2Connection(H2Configuration(client_side=True, header_encoding="utf-8"))
        conn.initiate_connection()
        b = conn.data_to_send()
        tls.sendall(b)
        inflight = {}
        issued = 0

        def issue():
            nonlocal issued
            while len(inflight) < S:
                sid = conn.get_next_available_stream_id()
                path = f"/pipeline/v2/update/ghub10/win/zz{idx}-{issued}/update.json"
                conn.send_headers(sid, [
                    (":method", "GET"), (":scheme", "https"), (":authority", HOST),
                    (":path", path),
                    ("user-agent", "LGHUB/2026.6.957899 (Windows NT 10.0; x64)"),
                    ("accept", "*/*"),
                ], end_stream=True)
                inflight[sid] = 0
                issued += 1
            b = conn.data_to_send()
            if b:
                tls.sendall(b)

        issue()
        while time.time() < t_end:
            try:
                data = tls.recv(65536)
            except socket.timeout:
                continue
            except (ssl.SSLError, OSError) as e:
                print(f"[dbg] conn {idx} recv error: {e}", flush=True)
                with lock:
                    stats["errors"] += 1
                break
            if not data:
                print(f"[dbg] conn {idx} closed", flush=True)
                break
            progressed = False
            for event in conn.receive_data(data):
                if isinstance(event, ResponseReceived):
                    inflight[event.stream_id] = dict(event.headers).get(":status", 0)
                elif isinstance(event, DataReceived):
                    if event.flow_controlled_length:
                        conn.acknowledge_received_data(
                            event.flow_controlled_length, event.stream_id)
                elif isinstance(event, StreamEnded):
                    st = inflight.pop(event.stream_id, None)
                    progressed = True
                    with lock:
                        stats["done"] += 1
                        if st == 200:
                            stats["ok200"] += 1
                        else:
                            stats["other"] += 1
                elif isinstance(event, StreamReset):
                    inflight.pop(event.stream_id, None)
                    progressed = True
                    with lock:
                        stats["done"] += 1
                        stats["resets"] += 1
                elif isinstance(event, ConnectionTerminated):
                    with lock:
                        stats["goaway"] += 1
            b = conn.data_to_send()
            if b:
                tls.sendall(b)
            if progressed:
                issue()
        result.append((idx, True))
    except Exception as e:
        result.append((idx, f"{type(e).__name__}: {e}"))
        with lock:
            stats["errors"] += 1


t_start0 = time.time()
t_end = t_start0 + DURATION
results = []
threads = []
for i in range(K):
    t = threading.Thread(target=conn_worker, args=(i, results))
    threads.append(t)
    t.start()
for t in threads:
    t.join()

elapsed = time.time() - (t_end - DURATION)
rate = stats["done"] / elapsed if elapsed > 0 else 0
print(
    f"connections={K} streams/conn={S}  finished={stats['done']}  "
    f"aggregate rate={rate:.0f}/s  ok200={stats['ok200']} other={stats['other']} "
    f"resets={stats['resets']} goaway={stats['goaway']} errors={stats['errors']}"
)
fails = [r for r in results if not r[1]]
if fails:
    print("failed conns:", fails[:5])
