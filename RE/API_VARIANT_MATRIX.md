# Logitech GHUB pipeline — API variant matrix (tested cells)

**Scope.** This file records which combinations of the pipeline API's
free-form dimensions were actually tested live, with the observed result of
each. It is a **coverage ledger**, not a claim of exhaustiveness: cells
marked *untested* were not probed, and "no difference observed" statements
are scoped to exactly the values listed.

**Hosts.** `updates.ghub.logitechg.com` (production CDN) and
`2pipeline.s3.amazonaws.com` (S3 origin) only. `pipeline.logitech.io`
reached DNS-only (see §1); `stg-pipeline.np.logitech.io` not probed by
constraint (staging host).

**Method (all live data in this file, 2026-09-07 UTC):** sequential
requests, 750 ms spacing, 15 s timeouts, one retry with 3 s backoff on
429/503 (none triggered), redirects captured manually, every request
recorded as `url / method / req-headers / UTC timestamp / status /
final-URL / resp-headers / body sha256 / byte length / fixture`. No
429/503 occurred; 30 requests total across three run dirs:

- `RE_Work/probes/api_matrix_run1_2026-09-07/` — 22 requests (Batches A–D)
- `RE_Work/probes/api_matrix_run2_2026-09-07/` — 5 requests (Batch E)
- `RE_Work/probes/api_matrix_run3_2026-09-07/` — 3 requests (Batch F)
- Earlier same-day runs: `reprobe_2026_09_07/` (17 requests),
  `reprobe_channel_names_2026-09-07/`, `reprobe_channel_brute_2026-09-07/`,
  `reprobe_access_keys_2026-09-07/`, `reprobe_h6_*/` — cited per cell.

Tool-level errors (DNS/TLS) are recorded separately from HTTP statuses.

---

## 1. Host dimension

| Host | Evidence source | Classification | Public reachability |
|---|---|---|---|
| `updates.ghub.logitechg.com` | binary 4-host array element 0 @0x1413D8B40 (`RE_Work/samples/lghub_updater_strings.txt` line 22787); [OWN] `Manager/UpdateManager/UpdateManager.cpp:343` | Production CDN — CloudFront distribution `d2l2wc59w4vrl3.cloudfront.net`, S3 origin | **Yes** — all pipeline/depot object traffic in this matrix |
| `2pipeline.s3.amazonaws.com` | binary array + v1 manifest absolute depot URLs | S3 origin (bucket `2pipeline`) | **Yes** — direct object GET 200; **ListObjects denied (403)** |
| `pipeline.logitech.io` | binary array element 2 | **Internal** production endpoint: DNS CNAME `internal-pipeline-prod-alb-1144201675.us-east-1.elb.amazonaws.com` → **172.30.91.104 / 172.30.90.118** (RFC1918 private, us-east-1) — an internal-facing ALB | **No** — public DNS resolves to private addresses; `fetch failed` (tool-level TCP/TLS failure, not an HTTP status; run2 `E5_alt_host_head`) |
| `stg-pipeline.np.logitech.io` | binary array element 3 | Staging (per name) | **Not probed** (constraint: no staging-host probing) |

The binary's 4-host array is the static candidate set; the runtime host
comes from Settings (`GetSettingString` 0x140EE2990) / IPC
(`Channel.pipeline_host`). Selection order among the four is a hypothesis
only.

## 2. Route × version × suffix (win, public channel unless noted)

All rows on `updates.ghub.logitechg.com`. ETags as served (quotes stripped
in tables; `W/` prefix = weak validator). Fixtures:
`reprobe_2026_09_07/resp_*.json` + `probe_summary.json`;
`reprobe_channel_{names,brute}_2026-09-07/`; `api_matrix_run1_2026-09-07/`.

| Route | App | Status | Bytes | ETag | Object Last-Modified | Body | Fixture |
|---|---|---|---|---|---|---|---|
| `/pipeline/v2/update/ghub10/win/public/update.json` | ghub10 | 200 | 197 | `e6a093b9c4a83e6cade28c9dabc4bd27` (strong) | 2026-06-08 17:02:47Z | plaintext JSON, buildId 634218 / 2025.9.814156 | `reprobe_2026_09_07/…public_update.json.json` |
| `…/public/details.json` | ghub10 | 200 | 938,799 | `W/"93208d1198a5be93a085849b1f128ee9"` (weak) | 2026-06-08 17:02:46Z | plaintext JSON, pretty-printed, br-compressed on wire | `…public_details.json.json` |
| `…/public/settings` (client suffix, no `.json`) | ghub10 | **403** | 263 | — | — | S3 `AccessDenied` XML | `…public_settings.settings` |
| `…/public/settings.settings` (typo path probed earlier) | ghub10 | **403** | 243 | — | — | S3 `AccessDenied` XML | round-4 capture |
| `…/win/canary/update.json` | ghub10 | 200 | 197 | `69ed3902fe9423d25b3dee099ee1f433` | 2026-06-08 17:02:44Z | plaintext JSON, `channel:"canary"`, lastModified 2025-12-11T12:24:39Z | `…canary_update.json.json` |
| `…/win/canary/details.json` | ghub10 | 200 | 938,799 | `W/"be3b37fbb5a956a7d212b9daad830317"` | 2026-06-08 17:02:43Z | plaintext JSON; same build as public, differs in `channel`+`lastModified` only | `…canary_details.json.json` |
| `…/win/canary/settings` | ghub10 | **403** | 243 | — | — | S3 XML | `…canary_settings.settings` |
| `…/win/tim/update.json` | ghub10 | 200 | 207 | (strong, in `reprobe_channel_names_2026-09-07/tim_update.resp.txt`) | 2026-08-13 10:14:43Z | **opaque** (no JSON, no known magics) | `tim_update.body` |
| `…/win/tim/details.json` | ghub10 | 200 | 809,601 | (weak, in `tim_details.resp.txt`) | 2026-08-13 10:14:42Z | **opaque**; 205 B ciphertext prefix shared with tim update | `tim_details.body` |
| `…/win/staging/update.json` | ghub10 | 200 | 198 | `016049a663d13757ceefba246b239a66` | 2025-12-10 22:39:18Z | **opaque** | `reprobe_channel_brute_2026-09-07/staging_update.body` |
| `…/win/staging/details.json` | ghub10 | 200 | 938,800 | (weak, in `staging_details.resp.txt`) | 2025-12-10 22:39:18Z | **opaque**; 196 B ciphertext prefix shared with staging update | `staging_details.body` |
| `/pipeline/v1/update/ghub10/win/public/update.json` | ghub10 | 200 | (round-4) | — | — | plaintext, v1 schema | round-4 capture |
| `/pipeline/v1/update/ghub10/win/public/details.json` | ghub10 | 200 | 224,746 | `4ba1f87828f221cf01459abfa478a888` | 2026-06-08 17:02:38Z | v1 schema: per-depot `cipherSuite:"none"`, absolute S3 URLs, no `uuid`/`signatures`/`keys` | `…v1…public_details.json.json` |
| `/pipeline/v1/update/ghub12/win/public/update.json` | ghub12 | 200 | 197 | `710845ecd40f0bb2e2cad6d7bb6276a5` — **identical to v2** | 2026-06-08 17:01:37Z | byte-identical to the v2 object (same ETag), buildId 710935 / 2026.2.861817 | `…v1…ghub12…update.json.json` |
| `/pipeline/v1/update/ghub12/win/public/details.json` | ghub12 | (round-4 capture, v1 vs v2 schema diff) | — | — | — | v1 schema | round-4 capture |
| `/` (bucket-root style path on CDN host) | — | **403** | 0 | — | — | **empty body** (not the S3 XML template) | `reprobe_2026_09_07/resp_root.bin` |

**v1/v2 observation:** for ghub12 the v1 and v2 `update.json` are the
*same object* (identical ETag/content). v1 `details.json` keeps the legacy
schema (absolute depot URLs, no signatures). Neither client binary contains
a v1 route literal — the client only constructs v2; v1 is served
server-side as legacy.

## 3. App × platform (v2, `win`/`osx`/`mac`/`linux` × `public`)

| App | win | osx | mac | linux |
|---|---|---|---|---|
| **ghub10** | 200 — update 197 B (634218 / 2025.9.814156), details 938,799 B | **200** — update 197 B: buildId 625362 / **2025.9.807501**, branch `staging/2025_9_ghub10`, lastModified 2025-12-03T16:40:22Z, ETag `d344d37fcc2b2cdaf84992507d3bd06d` (run2 `E2_osx_update_get`) | **403** (run1 `B3_mac_update`) | **403** (run1 `B5_linux_update`) |
| **ghub12** | 200 — update 197 B (710935 / 2026.2.861817), details 992,526 B | untested | untested | untested |
| **ghub13** (new — string in updater + software manager) | **200** — update **190 B**: buildId **824196** / **2026.5.939708**, branch `staging/2026_5`, lastModified 2026-08-11T14:36:47Z, ETag `05b2339f0d9a33c216e2e14d417f86f9`; details **1,106,793 B** ETag `fcf7bde981686c7f8cab578e9df589d4` (run1 `B1/B2`, run2 `E1_ghub13_update_get`) | untested | untested | untested |
| **ghub99** (control, not in any binary) | **403** (run1 `B6_ghub99_update`) | — | — | — |

**ghub13 = the local build.** Its served public build (824196 /
2026.5.939708) exactly matches the locally installed updater version and
the depot id in the local software-manager log of 2026-08-08 — see
`RE/API_FINDINGS.md` F1.

**Platform tokens:** the valid platform segment for macOS is **`osx`**
(depots in manifests use `osx` in their names); `mac` and `linux` are
unknown keys → 403. Linux may exist under a different token — *untested*.

## 4. Channel × app (v2, `win`)

| Channel | ghub10 | ghub12 | ghub13 |
|---|---|---|---|
| `public` | 200 (row §2) | 200 (row §2) | 200 (row §3) |
| `canary` | 200 (row §2); also reached by `logi-install-id` bucketing instead of path (rows §6) | untested | untested |
| `tim` | 200 — opaque (207 / 809,601 B) | **403** (run2 `E3_ghub12_tim_head`) | untested |
| `staging` | 200 — opaque (198 / 938,800 B) | **200** — update 198 B, ETag `7d44412e060cc7e1e9343308ba665354`, Last-Modified 2026-03-25 (run2 `E4_ghub12_staging_head`); details not fetched | untested |
| 14 random names (`bob`, `zzqq`, `dev`, `test`, `qa`, `alpha`, `beta`, `rc`, `nightly`, `internal`, `preview`, `experimental`, `canary2`, `stable`) | **403** ×14 | untested | untested |

Channel is a **free-form path segment** (no client whitelist; allowlist
memcmp @0x140E01248 covers the *manifest* app-ids, not channels). Channel
availability is **per-app**: `tim` exists on ghub10 but not on ghub12.

## 5. Access-group routes `/pipeline/v2/access/{id}/{content.json,iat.json}`

| Group id | content.json | iat.json |
|---|---|---|
| `323e77f5-68c2-43d8-8457-818bbd663938` (ghub10 manifest `keys.accessGroup`) | 200 — **11,536 B**, 71 entries (16-B keys, `version` field; `key-id` naming matches depot headers) — `reprobe_access_keys_2026-09-07/content.json.body` | 200 — 30 B `{"lastModified":1765286821}` — `iat.json.body` |
| `e37b37e8-a368-477e-8cb9-2cc6cb1d8324` (ghub12 group) | 200 — 12,985 B (round-4 capture) | **untested** this round |
| `logitech`, `ghub`, `logigames`, `ghub10` (non-UUID names) | **403** ×4 (263 B) | **403** (243 B, `logitech`) |
| malformed UUID | untested (non-UUID names already cover "unknown key → 403") | untested |

## 6. HTTP-behavior matrix (canonical object: ghub10/win/public `update.json`, 197 B)

All run1 (`api_matrix_run1_2026-09-07/`) unless noted.

| Variant | Status | Result |
|---|---|---|
| GET, no custom headers (baseline) | 200 | 197 B, body = pretty-printed JSON `{\n  …` (C5 hex `7b0a2020` = `{\n  `) |
| HEAD | 200 | no body; `content-length` + full metadata present |
| `Range: bytes=0-3` | **206** | 4 B; `content-range: bytes 0-3/197` — byte ranges supported on manifests |
| `Range: bytes=0-15` on a **depot** (`780f7572-…/g560_dfu.depot`, 632 B) | **206** | 16 B = magic `0x20170110` (plain depot) + chunk u32 `0x5a` + `{"files"` JSON start — ranges work on depots, `content-range: bytes 0-15/632` |
| `If-Modified-Since: <future date>` | **304** | conditional requests honored on manifests |
| `If-None-Match: <value>` on a **depot** | **304** | conditional requests honored on depots |
| Trailing slash `…/update.json/` | **403** | 263 B S3 XML — a different key, not a redirect |
| Uppercase `…/UPDATE.JSON` | **403** | 243 B S3 XML — keys are case-sensitive |
| Query `?foo=bar&channel=canary` | 200 | body **byte-identical to baseline** (`channel:"public"`) — query parameters do not affect routing or bucketing (tested keys: `foo`, `channel`) |
| `logi-install-id` absent | 200 | `channel:"public"` |
| `logi-install-id: not-a-valid-id` (malformed) | 200 | `channel:"public"` |
| `logi-install-id: 0000…0000` (64-hex, canary-mapping) | 200 | `channel:"canary"` (197 B — same size as public because `public`/`canary` are equal length) |
| `logi-app-version: 2025.9.814156` (current) | 200 | identical to baseline (run3 `F1`) |
| `logi-app-version: 9999.1.1` (far future) | 200 | identical to baseline (run3 `F2`) — **no observable server-side gating** by app version in the tested range |
| `logi-install-id: <canary id>` + `logi-app-version: 9999.1.1` | 200 | `channel:"canary"` (run3 `F3`) — install-id alone drives bucketing; no interaction observed |
| `Accept-Encoding` negotiation | — | `details.json` → `content-encoding: br`, chunked, `vary: Accept-Encoding`; `update.json` → identity (no br) |
| S3 `ListObjectsV2` on origin (`?list-type=2&prefix=pipeline/`, `…/ghub10/win/public/`, `depots/`, bucket-root form) | **403** ×3 | 263 B S3 XML — **public listing is disabled**; only direct object GET is allowed |

## 7. Per-object metadata pattern

| Object class | ETag | Wire encoding | CDN cache (observed) | Other |
|---|---|---|---|---|
| `update.json` | strong `"32hex"` | identity | `x-cache: Miss` in every capture (incl. fresh ghub13/osx objects) | `accept-ranges: bytes`, `x-amz-server-side-encryption: AES256` |
| `details.json` | weak `W/"32hex"` | `br`, chunked | Hit (staging, age 15/16) / Miss (tim) | `vary: Accept-Encoding` |
| depot (`.depot`) | strong `"32hex"` | identity | **Hit** with `age` (depot HEAD: age 69764 ≈ 19.4 h); **Miss** on Range requests | `content-type: application/octet-stream` |
| `settings` (any channel) | — | — | — | always 403 |
| 403 error | — | S3 `AccessDenied` XML | — | template constant; `RequestId`/`HostId` are per-request random strings ⇒ body length varies (263 vs 243 B is *not* a different error class) |

CloudFront + S3 origin confirmed on **all** object classes (depot included):
every capture carries both `server: AmazonS3` and
`via: …cloudfront.net (CloudFront)` + `x-amz-cf-id` + `x-cache`. The
earlier "S3 alias, not CloudFront" reading is **[DISPROVEN]**.

## 8. Error surface

- **403 is the only error status observed** on both hosts (unknown app,
  unknown platform, unknown channel, unknown access group, `/settings`,
  wrong-case keys, trailing-slash keys, S3 listing, HTTP-on-TLS-host depot
  paths).
- 404 is **never** returned — missing vs denied objects are
  indistinguishable by status.
- Root path on the CDN host → 403 with an **empty** body (only 403 without
  the XML template).
- HTTP (non-TLS) on `updates.ghub.logitechg.com` → 403 for depot paths
  (TLS required); S3 origin speaks both.

## 9. Coverage statement (exact)

**Tested live this round (30 requests):** 22 + 5 + 3 listed above.

**Tested earlier on 2026-09-07 (same day, cited per row):** 17-request
reprobe (§2), channel census (§4), access-group re-probe (§5), H6
bucketing ids (§6), S3-origin H3 depots.

**Explicitly NOT tested (do not infer):**
- ghub12/ghub13 × `osx`/`mac`/`linux`; ghub13 × canary/tim/staging;
  ghub12 × canary; any `details.json`/`settings` fetch on ghub13 or osx
  (HEAD + update.json only); v1 × osx; v1 × tim/staging.
- `iat.json` for the ghub12 access group; malformed-format UUID groups.
- Query keys beyond `foo`/`channel`; query interaction with tim/staging
  objects; `logi-app-version` beyond the 3 tested values; other header
  names entirely (the client sends only the two `logi-*` headers).
- S3 listing with other prefixes; `list-type=2` at the bare bucket root
  with no prefix (prefixes were always supplied).
- `pipeline.logitech.io` and `stg-pipeline.np.logitech.io` over HTTP(S).
- Any CN-host (constraint).
- Methods beyond GET/HEAD (POST/PUT/OPTIONS not probed).

**Known malformed test superseded:** `probe_channel_brute.mjs:30` issued
S3 `list-type=2` with `prefix` *on the object path* — invalid as a listing
test. The corrected bucket-root + `prefix` form is row §6 (all 403).
