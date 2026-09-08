import socket
import ssl
import time

from h2.connection import H2Connection
from h2.config import H2Configuration
from h2.events import ResponseReceived, DataReceived, StreamEnded, StreamReset

HOST = "updates.ghub.logitechg.com"
S = 128

ctx = ssl.create_default_context()
ctx.set_alpn_protocols(["h2"])
raw = socket.create_connection((HOST, 443), timeout=15)
tls = ctx.wrap_socket(raw, server_hostname=HOST)
tls.setblocking(True)
tls.settimeout(0.5)
conn = H2Connection(H2Configuration(client_side=True, header_encoding="utf-8"))
conn.initiate_connection()
tls.sendall(conn.data_to_send())

inflight = {}
issued = 0
while len(inflight) < S:
    sid = conn.get_next_available_stream_id()
    conn.send_headers(sid, [
        (":method", "GET"), (":scheme", "https"), (":authority", HOST),
        (":path", f"/pipeline/v2/update/ghub10/win/zz0-{issued}/update.json"),
        ("user-agent", "LGHUB/2026.6.957899 (Windows NT 10.0; x64)"),
        ("accept", "*/*"),
    ], end_stream=True)
    inflight[sid] = 0
    issued += 1
tls.sendall(conn.data_to_send())
print(f"sent preface + {issued} HEADERS", flush=True)

t0 = time.time()
done = resets = 0
printed = 0
t_end = t0 + 20
while time.time() < t_end:
    try:
        d = tls.recv(65536)
    except socket.timeout:
        continue
    except (ssl.SSLError, OSError) as e:
        print(f"ERR at {time.time()-t0:.1f}s:", e, flush=True)
        break
    if not d:
        print("closed", flush=True)
        break
    evs = conn.receive_data(d)
    for e in evs:
        t = time.time() - t0
        if isinstance(e, (ResponseReceived, StreamReset, StreamEnded)) and printed < 20:
            printed += 1
            print(f"t={t:6.2f} {type(e).__name__} stream={e.stream_id} "
                  f"{'status=' + str(dict(e.headers).get(':status')) if isinstance(e, ResponseReceived) else ''}",
                  flush=True)
        elif isinstance(e, DataReceived):
            if e.flow_controlled_length:
                conn.acknowledge_received_data(e.flow_controlled_length, e.stream_id)
    if isinstance(evs[-1], StreamEnded) if evs else False:
        pass
    # count completions
    if conn.data_to_send():
        tls.sendall(conn.data_to_send())
    for e in evs:
        if isinstance(e, StreamEnded):
            inflight.pop(e.stream_id, None)
            done += 1
        elif isinstance(e, StreamReset):
            inflight.pop(e.stream_id, None)
            resets += 1
print(f"total: done={done} resets={resets} pending={len(inflight)}", flush=True)
