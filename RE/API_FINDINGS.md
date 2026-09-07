# Logitech GHUB pipeline — API findings

**Scope.** Evidence-backed discoveries about Logitech's externally
observable update pipeline API, plus the hypotheses that remain open.
Every finding cites its fixtures (all under `RE_Work/`); the companion
`RE/API_VARIANT_MATRIX.md` is the cell-by-cell coverage ledger. The
canonical deep-dive remains `RE_Work/API_REFERENCE.md` (numbered findings
in its §11).

**Live evidence date:** 2026-09-07 UTC. Allowed hosts:
`updates.ghub.logitechg.com` (CDN) and `2pipeline.s3.amazonaws.com`
(origin). All live requests were sequential with 750 ms spacing; no
429/503 were encountered.

---

## A. Findings (evidence-backed)

### F1. Live app inventory: `ghub10`, `ghub12`, `ghub13` — and ghub13 is the current-generation (local) app
- `ghub13/win/public/update.json` → 200, **buildId 824196, version
  2026.5.939708**, branch `staging/2026_5`, lastModified
  2026-08-11T14:36:47Z; `details.json` → 1,106,793 B.
  Fixtures: `probes/api_matrix_run1_2026-09-07/B1_ghub13_update.resp.txt`,
  `api_matrix_run2_2026-09-07/E1_ghub13_update_get.body`.
- The served build **exactly matches the locally installed updater**
  (2026.5.939708) and the depot id (824196) recorded in the local
  software-manager log of 2026-08-08 (`C:\ProgramData\Logi\GHUB\Logs\software_manager\lghub_08_08_2026.log`).
  The updater binary's factory default app id is `ghub13`
  (0x140e02460) and the string `ghub13` also appears in the software
  manager binary.
- **Consequence (H7, partially closed):** the earlier "local is newer than
  every live channel" observation was an app-identity artifact — the local
  machine is on **ghub13**, whose public channel serves exactly the local
  build. ghub10's 2025.9.814156 was never the app this install consumes.
  Residual: the `User-Agent: 2026.6.957899` string origin (see B4).

### F2. Platform tokens: `win` and `osx` are live; `mac`/`linux` are unknown
- `ghub10/osx/public/update.json` → 200, buildId 625362 /
  2025.9.807501 (fixture `api_matrix_run2_2026-09-07/E2_osx_update_get.body`).
  Depot names in manifests use `osx`, matching the token.
- `ghub10/mac/…` → 403, `ghub10/linux/…` → 403
  (fixtures `api_matrix_run1_2026-09-07/B3_mac_update.resp.txt`,
  `B5_linux_update.resp.txt`). A Linux build may exist under a different
  token — untested.

### F3. Channels: per-app availability, 403-only for unknown names
- ghub10: `public`, `canary` (200, plaintext), `tim` (200, opaque,
  207 B / 809,601 B), `staging` (200, opaque, 198 B / 938,800 B).
- ghub12: `public`, `staging` (200, update 198 B, ETag `7d44412e…`,
  Last-Modified 2026-03-25) — but **`tim` → 403** (fixture
  `api_matrix_run2_2026-09-07/E3_ghub12_tim_head.resp.txt`).
- 14 random names on ghub10 → 403 (round-4 census,
  `probes/reprobe_channel_names_2026-09-07/`).
- `canary` is reachable two ways: path segment **or** server-side
  bucketing via the `logi-install-id` header (F5).

### F4. Opaque channel bodies + the update⊂details prefix-extension structure
- tim/staging bodies fail JSON parse, carry no depot magics at byte 0, are
  high-entropy, and have no framing header. **Encoding and consumer are
  unresolved; the bodies are documented as captured, opaque data** (no
  cipher/format is asserted). Fixtures:
  `reprobe_channel_names_2026-09-07/tim_{update,details}.body`,
  `reprobe_channel_brute_2026-09-07/staging_{update,details}.body`.
- Structural fact (offline, saved bodies): **`update.json` is the leading
  section of `details.json`**. Public (plaintext): first 195 of 197 B
  identical, details continues the same leading object by adding
  `"uuid": …`. Tim: 205 of 207 B common; staging: 196 of 198 B common —
  i.e. on encoded channels `update.json` is the encoded leading section
  (truncated stream) of the same object as `details.json`.
- Sizes: staging details = public + 1 B (consistent with the 7- vs 6-char
  channel name, not proven); tim details is 129,198 B smaller than public
  ⇒ tim's manifest content differs.
- **Disproven:** the earlier "staging recovered byte-for-byte" claim —
  `reprobe_channel_brute_2026-09-07/staging_details_RECOVERED.json` is a
  circular XOR of an assumed plaintext (K = ct ⊕ A ⇒ ct ⊕ K = A); it is
  labeled synthetic and excluded from the evidence.

### F5. Request semantics: `logi-install-id` drives channel bucketing; `logi-app-version` and query parameters have no observable effect
- `logi-install-id`: absent → public; malformed → public; well-formed
  64-hex (e.g. 64×`0`) → **canary** (re-confirmed live 2026-09-07,
  fixtures `api_matrix_run1_2026-09-07/C1–C3*.body`). Server-side hash
  threshold: ~4/6 of the 64-hex ids tested bucket to canary.
- `logi-app-version` ∈ {current, far-future, current+canary-id}:
  **identical responses** (fixtures `api_matrix_run3_2026-09-07/F1–F3*.body`)
  — no observable version gating in the tested range.
- Query parameters (`?foo=bar&channel=canary`): response byte-identical to
  the no-query baseline — ignored (tested keys only).
- The client sends exactly these two headers
  (`pipeline_build_install_headers` 0x14022BE90) and builds no query
  parameters — server-side acceptance beyond this is untested.

### F6. HTTP object semantics: S3 semantics exposed through CloudFront
- **206** for `Range` on both manifests and depots (depot range 0–15
  returned the `0x20170110` plain-depot header, fixture
  `api_matrix_run1_2026-09-07/D2_depot_range16.body`).
- **304** for `If-Modified-Since` (future) on manifests and `If-None-Match`
  on depots.
- Key lookup is exact: trailing slash → 403, uppercase → 403, no
  redirects, **no 404 surface** (403 is the only error status observed).
- Per-object metadata pattern: update.json = strong ETag, identity
  encoding; details.json = weak ETag, `br`, chunked, `vary:
  Accept-Encoding`; depots = strong ETag, octet-stream.

### F7. Infrastructure: CloudFront + S3 origin on every object class; listing disabled; one host is internal
- Every capture (manifests, depots, 403s) carries **both**
  `server: AmazonS3` and CloudFront headers (`via: …cloudfront.net
  (CloudFront)`, `x-amz-cf-id`, `x-cache`); DNS:
  `updates.ghub.logitechg.com` → `d2l2wc59w4vrl3.cloudfront.net`.
  The earlier "S3 alias, not CloudFront" reading is **[DISPROVEN]**.
- Depot objects are CDN-cached (HEAD: `x-cache: Hit`, `age` ≈ 19.4 h);
  Range requests come back `Miss` (cache key/behavior per range —
  observation only).
- `2pipeline.s3.amazonaws.com`: direct object GET 200 (mac-match verified
  on `depots/780f7572-…/g560_dfu.depot`), **ListObjectsV2 denied (403)**
  for prefixes `pipeline/`, `pipeline/v2/update/ghub10/win/public/`,
  `depots/` — enumeration is not possible; discovery is manifest-driven.
- `pipeline.logitech.io` (4-host array element 2) resolves to
  **`internal-pipeline-prod-alb-1144201675.us-east-1.elb.amazonaws.com` →
  172.30.91.104/172.30.90.118** — a **private** us-east-1 ALB, i.e. an
  internal production endpoint not reachable from the public internet
  (DNS-only evidence; HTTP attempt failed at connection level, fixture
  `api_matrix_run2_2026-09-07/E5_alt_host_head` in summary.json).

### F8. 403 is a single error surface (template constant)
- All 403 bodies are the same S3 `<Error><Code>AccessDenied</Code>…`
  template; the 263 B vs 243 B length difference seen across captures is
  just the variable-length random `RequestId`/`HostId` fields (both
  variants captured this round, fixtures `api_matrix_run1_2026-09-07/
  C7_update_trailslash.body` and `C8_update_uppercase.body`). Missing and
  denied keys are indistinguishable.
- Root path on the CDN host → 403 with an **empty** body (only 403 without
  the XML).

### F9. v1 is a server-side legacy of the same store
- v1 and v2 `update.json` for ghub12 are the **same object** (identical
  ETag `710845ec…`, both 197 B). v1 `details.json` (ghub10, 224,746 B)
  keeps the legacy schema: per-depot `cipherSuite:"none"`, absolute S3
  URLs, no `uuid`/`signatures`/`keys`.
- Neither client binary (updater or software manager) contains a v1 route
  literal (`pipeline/v2/update` only) — the client is v2-only; v1 exists
  server-side for older clients.

---

## B. Unresolved hypotheses / open items

| # | Item | Status |
|---|---|---|
| B1 | **Opaque-channel encoding + consumer.** tim/staging bodies are captured but not decoded; a bounded client-side key-material search (71 group keys × 6 modes, 1,426 derived candidates × 6 modes, PBKDF2-IV variants, ECB over 58,675 positions) found 0 hits, and this updater build has no manifest-decryption code path ⇒ key material is not in the updater. Likely consumer: software manager (unverified). Per user directive this thread stays closed for crypto work; the bodies remain documented opaque data. | Open (no crypto work) |
| B2 | **`iat.json` semantics.** `{"lastModified":1765286821}` — presumed "identity/access-token" or install-attestation timestamp; the client fetch path exists (builder 0x140269dc0) but the consumer of the value is not yet mapped. | Open |
| B3 | **Server bucketing hash/threshold** (which 64-hex install-ids → canary). Deterministic per value (~4/6 tested → canary) but the function is server-side. | Open |
| B4 | **`User-Agent: 2026.6.957899` origin.** Local install (now known to be ghub13, 2026.5.939708) still doesn't explain where 2026.6.957899 comes from — possibly a newer software-manager/app version observed in a captured request; untraced. | Open |
| B5 | **`0x20210521` depot live sample.** Layout resolved in the binary (§5.3 of the reference) but no live sample captured yet (ghub13's 1.1 MB details.json is the best candidate to inspect — its depot list was not fetched). | Open |
| B6 | **`/settings` schema** — 403 on this host (both `settings` and `settings.settings` paths); content unknown; FeatureCanary consumes it client-side (state 3 = disabled here). | Open |
| B7 | **ghub13 manifest content** — details.json (1,106,793 B) not yet fetched/analyzed (would close B5 and give the current-generation depot list). | Open |
| B8 | **`/scarif/keyswap`** endpoint semantics (string present in binary). | Open |
| B9 | **xdelta depot layout** (referenced in depot entries). | Open |
| B10 | **Methods beyond GET/HEAD** (POST/PUT/OPTIONS) — untested. | Open |
| B11 | **`stg-pipeline.np.logitech.io` + CN hosts** — not probed (constraint). | Excluded by constraint |

## C. How the evidence was produced (methodology)

1. **Record correction first** (per review mandate): the circular staging
   "recovery", the CloudFront misreading, the host-literal contradiction,
   the canary "byte-identical" claim, and the `/settings` suffix confusion
   were corrected in `RE_Work/API_REFERENCE.md` and
   `RE/LOGITECH_API_REFERENCE.md` before any expansion (commits
   `280668bd`, `63cc6fc6`).
2. **Offline re-measurement** of saved bodies (prefix lengths, ETags,
   Last-Modified, header sets) before any new request.
3. **Live batches** (30 requests, 2026-09-07): Batch A (S3 listing ×3,
   corrected form), B (app/platform/channel HEAD ×8), C (header/query/HTTP
   behavior on update.json ×8), D (depot HEAD/Range/conditional ×3), E
   (representative GETs + ghub12 channel cross-check + alt host ×5), F
   (app-version ×3). HEAD/small-range first; GET only for representative
   hits and small objects.
4. **Separation of tool errors from HTTP status:** the
   `pipeline.logitech.io` failure is recorded as a connection-level failure
   (DNS → private IP), not as an HTTP response.
5. **No base conversions by hand** (int_convert MCP where needed); no
   crypto implemented (established libraries only; the closed GCM thread
   stays closed).

## D. Fixture index

| Evidence | Fixture |
|---|---|
| Matrix run 1 (22 reqs, full metadata) | `RE_Work/probes/api_matrix_run1_2026-09-07/summary.json` |
| Matrix run 2 (5 reqs) | `RE_Work/probes/api_matrix_run2_2026-09-07/summary.json` |
| Matrix run 3 (3 reqs) | `RE_Work/probes/api_matrix_run3_2026-09-07/summary.json` |
| 2026-09-07 17-request reprobe | `RE_Work/probes/reprobe_2026_09_07/probe_summary.json` |
| Channel census (tim/staging/public/canary + 403s) | `RE_Work/probes/reprobe_channel_names_2026-09-07/` |
| Staging/tim brute + key-search results | `RE_Work/probes/reprobe_channel_brute_2026-09-07/` |
| Access-group content.json/iat.json | `RE_Work/probes/reprobe_access_keys_2026-09-07/` |
| Probe scripts | `RE_Work/probes/probe_api_matrix{,2,3}.mjs` |
