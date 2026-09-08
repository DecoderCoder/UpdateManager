import socket, ssl, time, sys
from h2.connection import H2Connection
from h2.config import H2Configuration

host = "updates.ghub.logitechg.com"
ctx = ssl.create_default_context()
ctx.set_alpn_protocols(["h2"])
raw = socket.create_connection((host, 443), timeout=15)
tls = ctx.wrap_socket(raw, server_hostname=host)
print("ALPN", tls.selected_alpn_protocol(), flush=True)
conn = H2Connection(H2Configuration(client_side=True, header_encoding="utf-8"))
conn.initiate_connection()
tls.sendall(conn.data_to_send())
print("sent preface", len(conn.data_to_send()), flush=True)

# one stream
sid = conn.get_next_available_stream_id()
conn.send_headers(sid, [
    (":method", "GET"), (":scheme", "https"), (":authority", host),
    (":path", "/pipeline/v2/update/ghub10/win/public/update.json"),
    ("user-agent", "probe"), ("accept", "*/*"),
], end_stream=True)
out = conn.data_to_send()
tls.sendall(out)
print("sent HEADERS", len(out), flush=True)

tls.settimeout(8)
got = 0
while got < 4000:
    try:
        d = tls.recv(65536)
    except socket.timeout:
        print("recv timeout"); break
    except Exception as e:
        print("recv err", e); break
    if not d:
        print("closed"); break
    got += len(d)
    print(f"recv {len(d)} bytes: {d[:24].hex()}", flush=True)
    try:
        ev = conn.receive_data(d)
        for e in ev:
            print("   event:", type(e).__name__, getattr(e, "stream_id", ""), getattr(e, "headers", "") and dict(e.headers).get(":status", ""), flush=True)
    except Exception as e:
        print("   parse err", e, flush=True)
print("total recv", got)
