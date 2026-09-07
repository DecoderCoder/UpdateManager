# Logitech G HUB Update Pipeline — API Reference

**Status:** working reference, 2026-09-07 (Europe/Berlin)
**Binary under analysis:** `C:\Program Files\LGHUB\lghub_updater.exe` (installed G HUB 2026.6.957899), IDA session `lghub3`, IDB `lghub_updater.exe.i64` (all renames/comments saved)
**Live data captured:** 2026-09-06, from `https://updates.ghub.logitechg.com`, all fixtures under `RE_Work/probes/`

This document is the canonical API reference for the G HUB update pipeline (the
"pipeline" used by `lghub_updater.exe`). The local program in this repository
(`Manager/UpdateManager/`) is **reference material only** — a known-working
implementation of parts of the same protocol — and is never claimed as the
target of analysis.

Every claim is tagged with its evidence tier:

| Tag | Meaning |
|---|---|
| **[LIVE]** | Observed in a live HTTP response captured 2026-09-06 (fixture path given) |
| **[OFFLINE-PROOF]** | Proven by offline computation against saved fixtures (reproducible command given) |
| **[BINARY]** | Observed in `lghub_updater.exe` via IDA (address given; IDB names are saved) |
| **[OWN]** | Behavior of our own program (`Manager/UpdateManager/`), read from source |
| **[HYPOTHESIS]** | Plausible, not yet proven |
| **[UNRESOLVED]** | Open question |
| **[DISPROVEN]** | Previously claimed, later contradicted — listed with the correction |

---

## 1. Hosts

### 1.1 Hosts that serve the pipeline

| Host | Role | Evidence |
|---|---|---|
| `https://updates.ghub.logitechg.com` | Primary production host: manifests, keymaster, depots | [LIVE] all fixtures; [BINARY] in `init_pipeline_hosts` array (0x140021320) |
| `https://2pipeline.s3.amazonaws.com` | S3 origin for v1 manifest depot URLs | [LIVE] v1 `details.json` uses absolute URLs to this host (`resp_pipeline_v1_update_ghub10_win_public_details.json.json`); [BINARY] in `init_pipeline_hosts` array |
| `https://pipeline.logitech.io` | Alternate pipeline host (not live-tested) | [BINARY] in `init_pipeline_hosts` array |
| `https://stg-pipeline.np.logitech.io` | Staging pipeline host (not live-tested) | [BINARY] in `init_pipeline_hosts` array |

**[BINARY] The four-host array.** `init_pipeline_hosts` (0x140021320) is the
static initializer of a 0x80-byte global at 0x1413D8B40 (begin) / 0x1413D8B48
(end): a 4-element array of `std::string`, registered for destruction with
`atexit(sub_140F0DB90)`. Elements, in order:

1. `https://updates.ghub.logitechg.com`
2. `https://2pipeline.s3.amazonaws.com`
3. `https://pipeline.logitech.io`
4. `https://stg-pipeline.np.logitech.io`

The array is consumed by two getters: `sub_140E04680` (returns
`end − begin`, i.e. the count) and `sub_140E047E0` (returns the begin pointer).

### 1.2 Other hosts appearing in the binary (not part of the 4-host array)

Strings in the binary (full list in `RE_Work/samples/lghub_updater_strings.txt`):

- `https://datapipeline.logitech.io`, `https://stg-datapipeline.np.logitech.io`
- CN-region set: `datapipeline.services.logitechg.com.cn`,
  `marketplace.services.logitechg.com.cn`, `dev-store.np.logitechg.com.cn`,
  `device-recommendation.services.logitechg.com.cn`
- `https://gamesapps-assets.ghub.logitechg.com`

[UNRESOLVED] Whether the updater selects among the 4-host array by region/
failure order, and what the CN hosts are used for. No live testing was done
against non-primary hosts (see §17, politeness).

---

## 2. Endpoints

All live probes were `GET`, unauthenticated (bun/fetch defaults), 2026-09-06.
Depot object URLs were fetched with
`User-Agent: LGHUB/2026.6.957899 (Windows NT 10.0; x64)`.
**[CORRECTED 2026-09-07]** the real client sends two identity headers on every
pipeline JSON request (see §2.0.2) — the earlier "no special headers"
statement was wrong for the client (our probes were header-free).

### 2.0 URL construction and request headers [BINARY + LIVE, 2026-09-07]

#### 2.0.1 URL construction — no query parameters, ever

- `build_update_url_base` (0x1402694D0) assembles the base with
  `append_url_segment` (0x14026A280) over exactly 5 segments:
  `[server, "pipeline/v2/update", appId, "win" (hardcoded literal), channel]`.
- Endpoint builders (all = base + fixed suffix, **no query parameters anywhere
  in the binary**):
  | Builder | Address | Suffix |
  |---|---|---|
  | `build_update_json_url` | 0x140269710 | `/update.json` |
  | `build_details_json_url` | 0x140269C90 | `/details.json` |
  | `build_settings_url` | 0x14026A150 | `/settings` (**no `.json`**) |
- **[CORRECTED 2026-09-07]** an earlier version of this section claimed
  `updates.ghub.logitechg.com` is *absent* from the binary — wrong: it is a
  literal (string-table line 22787 of
  `RE_Work/samples/lghub_updater_strings.txt`) and is element 0 of the
  4-host array (§1.1). The **runtime** host used for requests comes from
  the Settings singleton `GetSettingString` (0x140EE2990,
  `lghub\servers.cpp`) or the protobuf IPC field `Channel.pipeline_host`;
  the 4-host array is a static candidate set (default/fallback —
  **[HYPOTHESIS]**, its selection order not yet traced), and the configured
  host is validated against the memcmp allowlist table @0x140E01248.

#### 2.0.2 Request headers: `logi-install-id` + `logi-app-version`

Every pipeline JSON request (update.json, details.json, `/settings`) is built
with `pipeline_build_install_headers` (0x14022BE90), which fills a string map
(nodes 0x50 B; key @+32, value @+48; lookup `install_header_map_get`
0x14022B2A0) with exactly two entries:

| Header | Value source |
|---|---|
| `logi-install-id` | machine identifier string, stored at `pipeline_impl+880` |
| `logi-app-version` | Downloader vfunc 6 (+48) — the app version string |

Callers confirmed: `_check_for_updates` (0x1402142E0, update.json job
sub_1402594E0), `pipeline_impl_on_update_check_result` (0x140228430,
details.json job sub_14025AFC0 — renamed from the earlier misnomer
`create_details_download_job`), `create_settings_download_job` (0x140216840,
`/settings` job sub_14020FF10). Optional extra headers may be merged from
Downloader vfunc 44 (+352).

#### 2.0.3 H6 [CONFIRMED] — machine identifier generation, persistence, and
server-side canary bucketing

**Generation** (`pipeline_impl_builder_createMachineIdentifier` 0x1401FDF50,
called from `pipeline_impl_builder_build` 0x1401FD930):
- canary config type == 1 → `canary_machine_id_derive_name_volume`
  (0x140A386E0): `GetComputerNameW(WCHAR[16])` +
  `GetVolumeInformationW(L"c:\\")` volume serial, then **SHA-256** over the
  full name buffer (UTF-16LE, NUL included; `sha256_update(Buffer, 2*nSize[0])`)
  + 4-byte LE volume serial → 64-char lowercase hex (stringstream, `setw(2)`
  fill `'0'`). SHA-256 confirmed by K constants
  (0x6A09E667…0x5BE0CD19; init 0x140AB88E0 / update 0x140AB8960 /
  final 0x140AB85B0).
- type != 1 → **[CORRECTED 2026-09-07]** `sub_1401FDF50` calls
  `SystemInfo::GetVolumeSerial` (0x140A34940 → logi_platform system_info
  wrapper; the earlier "HDD serial" attribution was wrong) and uses the
  **decimal string** of the C: volume serial as the identifier — on this
  machine `4199704679` (10 ASCII digits, no hash). The transform
  `sub_140A34940` is this decimal-string conversion, now mapped.
- Log lines: `Canary - Generated machine identifier %s`,
  `Canary - Saved machine identifier`.

**Persistence** (load-or-generate): `canary_machine_id_load` 0x14022BE30 /
`canary_machine_id_save` 0x14022C0E0 via logi_platform `SecureStorage`
(container name "Logi Secure Storage"; open 0x140A49110 / read 0x140A497E0 /
write 0x140A496B0 / close 0x140A49360; serializer sub_140A61330; source
`…\secure_storage_impl_win.cpp` per path @0x141048D00). An existing value with
equal length + memcmp skips the write. Local container:
`HKLM\SOFTWARE\Logitech\LGHUB\Data\canary_machine_identifier`, REG_BINARY
(**396 B** as of 2026-09-07; earlier 415-B count superseded):
DWORD 1 @0; GUIDs @4/@0x17; UTF-16 "Logi Secure Storage" @0x2F; length 0x40
@0x148 + 64-byte value @0x14C (opaque on this machine — see §14u).

**Server behavior [LIVE 2026-09-07, `probe_h6_headers.mjs`,
`probe_h6_ids.mjs`, `probe_h6_rest.mjs`]:** the server buckets installs by the
`logi-install-id` header value (deterministic; repeat ⇒ same ETag, ~46 ms):

| `logi-install-id` | ghub10/win `details.json` channel |
|---|---|
| header absent / empty | `public` |
| `0123…` (64-hex) | `canary` |
| `0000…` (64-hex) | `canary` |
| `aaaa…` (64-hex) | `canary` |
| `1111…` (64-hex) | `canary` |
| `ffff…` (64-hex) | `public` |
| `deadbeef…` | `public` |

~4/6 well-formed 64-hex ids bucket to canary ⇒ per-value hash threshold on
the server. For ghub10/win the canary and public buckets serve **identical
build content** (buildId 634218, version 2025.9.814156); bodies are 938,799 B
each and differ only in the `channel` and `lastModified` fields. ETags:
public `W/"93208d1198a5be93a085849b1f128ee9"`, canary
`W/"be3b37fbb5a956a7d212b9daad830317"` (weak validators, CloudFront-cached
per (channel, id-bucket)). `update.json` buckets identically (§3.3).

**Version decision [BINARY]:** `pipeline_impl_has_new_build_to_download`
(0x1402185D0) compares versions by **plain string equality, not semver**:
current==next ⇒ "…same build already installed… Ignoring this build." (0);
old-next==new-next ⇒ "Next build did not change…" (0); else "Found new version
to download…" (1). The updater is **downgrade-capable** (any different string
counts as new).

#### 2.0.4 FeatureCanary is fed by `/settings` [BINARY, 2026-09-07]

RTTI for the `FeatureCanary::start` lambda_1 is
`std::function<void(const logi::protocol::updates::Settings&, int)>` — i.e.
the canary component/flag tree comes from the **`/settings` response**
(`logi.protocol.updater.Settings` proto-JSON), with the `int` = HTTP status.
Since `/settings` 403s on this host, FeatureCanary state ends at 3
("Download failed with error code %d"). `FeatureCanary_get_flag`
(0x140124460) → `FeatureCanary_lookup_flag` (0x1401236B0) does a two-level
tree lookup (component → flag) with **no local machine-id bucketing**;
`FeatureCanary_parse_settings` (0x1401237C0) builds the tree and logs
"Non-object component group found for key %s, skipping…" /
"Flag defaults not set for %s:%s"; catch handler 0x140E6DFB0
"Exception parsing settings JSON %s".

#### 2.0.5 `pipeline_impl` object layout (size 0x3B0) [BINARY]

`pipeline_impl_make_shared` 0x1402262E0 → `pipeline_impl_ctor` 0x140210320
(vftable "logi::pipeline::pipeline_impl::vftable" @0x140F492F0):
+0/+8 double vftable; +56 Downloader sp; +120 = 15000 ms timeout; +128 base
dir (cache root = base+"cache"); +296 keymaster sp; +320 config sp; +336
storage sp; +352 cache-dir obj; +376/+392/+408 = config+432/448/464 sps;
+424 updates::Status vftable; +456 update state (2/3/12); +776 recursive
mutex; +880..+912 **machine identifier string**; +912..+944 **app id** string
(e.g. `ghub10`) — **[CORRECTED 2026-09-07]**, not the channel:
`pipeline_impl_on_update_check_result` (0x140228430) reads a1+912 as the
app id; the channel travels in the urlctx (config+32). Builder: `pipeline_impl_builder_build` (0x1401FD930) —
builder+72 set ⇒ caller-supplied id @builder+56, else
createMachineIdentifier; poll interval default **86400 s** (builder+48
override); update check entry `pipeline_impl::_check_for_updates`
(0x1402142E0): updates-disabled guard (+368), min-OS check vs config+216
(status 13 if too old), then update.json job.

### 2.1 Update manifest endpoints

| Path template | Method | Result 2026-09-06 | Fixture |
|---|---|---|---|
| `/pipeline/v2/update/{appId}/{platform}/{channel}/details.json` | GET | **200** — full depot list + signatures + keymaster group | `resp_pipeline_v2_update_ghub10_win_public_details.json.json` (938,799 B), `..._ghub12_win_public_details.json.json` (992,526 B) |
| `/pipeline/v2/update/{appId}/{platform}/{channel}/update.json` | GET | **200** — manifest pointer (no depots) | `resp_pipeline_v2_update_ghub10_win_public_update.json.json` (197 B) |
| `/pipeline/v2/update/{appId}/{platform}/{channel}/settings` (the client's actual suffix, no `.json`) | GET | **403** — S3 `AccessDenied` XML (263 B public / 243 B canary) | `reprobe_h6_rest_2026-09-07/` (2026-09-07 re-probe) |
| `…/settings.settings` (typo-suffix probed 2026-09-06 — **not** the client's path) | GET | **403** — S3 `AccessDenied` XML (263 B public / 243 B canary) | `resp_pipeline_v2_update_ghub10_win_public_settings.settings` |
| `/pipeline/v1/update/{appId}/{platform}/{channel}/details.json` | GET | **200** — v1 manifest (absolute S3 depot URLs, no signatures/keys) | `resp_pipeline_v1_update_ghub10_win_public_details.json.json` (224,746 B) |

Observed `appId` values: `ghub10` (G HUB 2025.9 line), `ghub12`.
Observed `platform`: `win`. Observed `channel`: `public`, `canary`
(200, plaintext JSON), `tim`, `staging` (200, **opaque** bodies — §2.5);
14 further probed names → 403.
**[LIVE]** The path is a plain S3-style key under a **CloudFront front with
S3 origin** — captured responses carry both `server: AmazonS3` and
CloudFront participation headers (`via: …cloudfront.net (CloudFront)`,
`x-amz-cf-id`, `x-amz-cf-pop`, `x-cache: Hit|Miss from cloudfront`,
`x-amz-server-side-encryption: AES256`); root `/` returns 403 0 B and
unknown keys return S3 `AccessDenied` XML. I.e. the whole thing is object
storage behind a CDN, with an access policy. No API documentation or auth
headers were found in the binary for these paths.

**[LIVE] canary vs public:** `ghub10/win/canary/details.json` and
`ghub10/win/public/details.json` are the same size (938,799 B) but **not
byte-identical** (re-probe 2026-09-07, different ETags
`be3b37fb…` vs `93208d11…`): canary content `lastModified` is
`2025-12-11T12:24:39Z`, public `2025-12-12T05:13:22Z`; the equal size is a
coincidence of the one-day field difference. Same buildId/version for both
(634218 / 2025.9.814156).

### 2.2 Depot object URLs

| Template | Served by | Evidence |
|---|---|---|
| v1: `https://2pipeline.s3.amazonaws.com/depots/{dirUuid}/{name}.depot` (absolute, in manifest) | S3 | [LIVE] v1 manifest |
| v2: `/depots/{dirUuid}/{name}.depot` (relative, in manifest) | `updates.ghub.logitechg.com` — **empirically confirmed by fetching 6 depots from this host** | [LIVE] `RE_Work/probes/fetch_depot.mjs` (UA header), saved `depot_*.bin` |

**[CONFIRMED 2026-09-07, H3]** v2 relative URLs are also readable on the S3
origin: `http://2pipeline.s3.amazonaws.com/depots/780f7572-…/g560_dfu.depot`
→ 200, 632 B, SHA-256 = manifest `mac`
(`RE_Work/probes/reprobe_2026_09_07_h3/h3_s3_origin.depot`).
**[CONFIRMED 2026-09-07; CORRECTED this round]** the
`updates.ghub.logitechg.com` host is a **CloudFront distribution with S3
origin**, not a bare S3 bucket alias: the captured manifest responses
(`reprobe_channel_names_2026-09-07/tim_details.resp.txt`,
`reprobe_channel_brute_2026-09-07/staging_update.resp.txt`, and the
2026-09-06 fixtures) carry **both** `server: AmazonS3` **and** CloudFront
participation headers (`via: …cloudfront.net (CloudFront)`,
`x-amz-cf-id`, `x-amz-cf-pop`, `x-cache: Hit|Miss from cloudfront`).
S3-origin pass-through headers and CDN headers coexist — the earlier
"not a CloudFront distribution" statement is **[DISPROVEN]**.
(Per-object CDN pathing for depot objects is not recorded in the saved
fixtures — the depot GETs stored bodies only — **[UNRESOLVED]**, minor.)
Note: the **HTTP** listener of `updates.ghub.logitechg.com` answers the
same depot path with 403 (protocol-level block; HTTPS works) — always use
HTTPS for that host.

### 2.3 Keymaster / access endpoints

| Path template | Result 2026-09-06 | Fixture |
|---|---|---|
| `/pipeline/v2/access/{accessGroup-UUID}/content.json` | **200** — key list | `probe_access_ghub10_group_content.out` (11,536 B, 71 keys), `probe_access_ghub12_group_content.out` (12,985 B) |
| `/pipeline/v2/access/{accessGroup-UUID}/iat.json` | **200** — `{ "lastModified": 1765286821 }` | `probe_access_ghub10_group_iat.out` (32 B) |
| `/pipeline/v2/access/{name}/content.json` for `logitech`, `ghub`, `logigames`, `ghub10` | **403** — S3 `AccessDenied` XML (243/263 B) | `resp_pipeline_v2_access_*_content.json.json` |
| `/pipeline/v2/access/{name}/iat.json` for `logitech` | **403** | `resp_pipeline_v2_access_logitech_iat.json.json` |

**[LIVE] The access path requires the access-group UUID, not a human
name.** The UUID is obtained from the manifest itself (see §4.1). This is the
complete discovery chain, all confirmed:

```
GET /pipeline/v2/update/{app}/{plat}/{chan}/details.json
  → m["keys"]["accessGroup"] = "<UUID>"
GET /pipeline/v2/access/<UUID>/content.json
  → key list (base64 16-byte AES-128 keys, one per UUID name)
```

Group UUIDs observed: ghub10 → `323e77f5-68c2-43d8-8457-818bbd663938`,
ghub12 → `e37b37e8-a368-477e-8cb9-2cc6cb1d8324`.

### 2.4 Endpoints seen only in the binary (not live-tested)

- `/scarif/keyswap` — string present in the binary. **[UNRESOLVED]**
  semantics, method, auth.
- Depot download progress / resume semantics — no Range behavior was
  tested. (Response cache semantics — ETag/Last-Modified/Cache-Control —
  were captured on the 2026-09-07 re-probe, see §9.)

### 2.5 Channel census and encrypted channels [LIVE + OFFLINE, 2026-09-07]

**Channel names are a free-form path segment.** The client has no
channel whitelist: `pipeline_storage_get_valid_channel` (0x1402005A0)
accepts whatever the config/IPC supplies; the server decides what exists.
Probing 14 additional names (`dev, beta, test, nightly, latest, release,
stable, main, master, trunk, preview, alpha, rc, prod`) returned
**403 S3 `AccessDenied`** for every one — indistinguishable from a
"not provisioned" object; there is no 404 surface.

Live channel table for `ghub10/win` (both suffixes, 2026-09-07; fixtures
in `RE_Work/probes/reprobe_channel_names_2026-09-07/` and
`RE_Work/probes/reprobe_channel_brute_2026-09-07/`):

| Channel | update.json | details.json |
|---|---|---|
| `public` | 200, 197 B, **plaintext JSON** | 200, 938,799 B, **plaintext JSON** (build 634218 / 2025.9.814156) |
| `canary` | 200, 197 B, plaintext | 200, 938,799 B, plaintext, **same build as public but not byte-identical** (different ETags, `lastModified` differs by 1 day — §2.1) |
| `tim` | 200, **207 B opaque** | 200, **809,601 B opaque** |
| `staging` | 200, **198 B opaque** | 200, **938,800 B opaque** |
| 14 other names | 403 S3 XML | 403 S3 XML |

**Opaque bodies — documented, not decrypted.** All four tim/staging
bodies: declared `Content-Type: application/json` but **fail JSON
parse**; no known depot/capsule magic (`0x20170110` / `0x20210506` /
`0x20210521`) at byte 0; high-entropy bytes from offset 0; no framing
header (no key-id/uuid prefix) observed. ⇒ the encoding/protection and
the server-side meaning are **[UNRESOLVED]**; the bytes must not be
interpreted as any specific cipher or format.

**Structural facts** (offline, computed on the saved bodies):

1. In plaintext `public`, `details.json` is a **strict prefix extension**
   of `update.json`: first 195 B identical; `details` continues the same
   leading object by adding `"uuid": …` (and the rest of the manifest)
   after the same seven fields (`appId, platform, channel, buildId,
   version, branch, lastModified`).
2. `tim`: update (207 B) and details (809,601 B) share a **205 B common
   prefix**; `staging`: 196 of 198 / 938,800 B. The same "update.json is
   the leading section of details.json" structure holds, and both
   objects are encoded from byte 0 with the same keystream (equal
   ciphertext prefix over the equal plaintext prefix, per fact 1).
   For encoded channels, `update.json` is therefore the encoded leading
   section — a truncated stream of the same construction. The mode/key
   identity behind this is not established by these observations.
3. Sizes: staging details = public + 1 B — *consistent with* the channel
   value `"staging"` (7 chars) vs `"public"` (6 chars), **not proven**;
   tim details = 809,601 B is 129,198 B smaller than public ⇒ the
   manifest content (depot set) differs; tim's exact build is
   unobservable while the body stays opaque.
4. Per-object metadata (captures): `update.json` objects → **strong**
   32-hex ETag, `accept-ranges: bytes`, no `br`; `details.json` objects →
   **weak** ETag `W/"…32hex"`, `content-encoding: br`, chunked,
   `vary: Accept-Encoding`. Last-Modified: tim 2026-08-13 (details
   10:14:42Z / update 10:14:43Z — separate objects), staging
   2025-12-10T22:39:18Z (both). All four: `x-amz-server-side-encryption:
   AES256`, CloudFront `x-cache` Hit/Miss, S3 origin.

**[DISPROVEN] earlier "recovery" of the staging body.**
`reprobe_channel_brute_2026-09-07/staging_details_RECOVERED.json` is a
**synthetic** artifact: `recover_staging.mjs` computed K = ct XOR
assumedPlaintext and re-applied that same K to the same ciphertext
(K = ct ⊕ A ⇒ ct ⊕ K = A) — it reproduces the *assumption* (the public
JSON with the channel value swapped), not server data. It is excluded
from the evidence about staging's content and must not be cited as a
recovery. (The file remains in the tree, labeled, for audit.)

**[CLOSED experiment] Bounded client key-material search — 0 hits.**
All 71 access-group keys × 6 candidate modes, 1,426 derived key
candidates (PBKDF2/SHA/HMAC combinations of group UUID, channel names,
app ids, SSO client ids) × 6 modes, PBKDF2-IV variants, and an
ECB-repeat-keystream check over 58,675 positions — **0 hits**
(`test_ks_deep_result.json`, `test_ks_modes_result.json`). A failed
candidate set does not disprove any particular algorithm; it only
excludes the tested material. Corroborating: the updater binary contains
**no** code path that decodes manifests (its only AES/GCM usage is the
depot-capsule pipeline, §5.2). **Verdict: the key material for
tim/staging is not in this updater build; the consumer component is
[HYPOTHESIS] (e.g. the software manager) and remains unverified.**

Coverage and open questions: `RE/API_VARIANT_MATRIX.md` (what was
probed) and `RE/API_FINDINGS.md` (findings + unresolved hypotheses).

### 2.6 App / platform / host inventory [LIVE, 2026-09-07 matrix round]

30 live requests (run dirs `api_matrix_run{1,2,3}_2026-09-07/`, full
metadata per request):

**Apps (v2, `win`, `public`):** `ghub10` (197 B / 938,799 B), `ghub12`
(197 B / 992,526 B), **`ghub13` — new, live** (update 190 B: buildId
**824196 / 2026.5.939708**, branch `staging/2026_5`, lastModified
2026-08-11; details **1,106,793 B**). `ghub99` (control) → 403.
`ghub13`'s served build **exactly matches the local install**
(updater 2026.5.939708, depot 824196 in the 2026-08-08 software-manager
log) ⇒ the local machine is on **ghub13** (the factory-default app id,
§2.0.5/§11.19) — **H7 partially closed** (the "local newer than live"
observation was an app-identity artifact; the `2026.6.957899` UA origin
remains open).

**Platforms:** `win` + **`osx` live** (ghub10/osx public update 200:
buildId 625362 / 2025.9.807501); `mac`/`linux` → 403. A Linux token may
exist under another name — untested.

**Channels × app:** `tim` exists on ghub10 but **403 on ghub12**;
`staging` exists on both (ghub12 staging update 198 B, ETag
`7d44412e…`, Last-Modified 2026-03-25 — different object from ghub10's).
Channel sets are **per-app** (matrix §4).

**Hosts:** `updates.ghub.logitechg.com` → CloudFront `d2l2wc59w4vrl3`
(S3 origin); **`pipeline.logitech.io` is internal** — DNS CNAME
`internal-pipeline-prod-alb-1144201675.us-east-1.elb.amazonaws.com` →
**172.30.91.104/172.30.90.118** (private us-east-1 ALB), unreachable from
the public internet (connection-level failure, not HTTP).
`stg-pipeline.np.logitech.io` not probed (constraint).

**HTTP behavior (matrix §6):** 206 Range (manifests **and** depots —
depot range 0-15 = `0x20170110` header), 304 If-Modified-Since /
If-None-Match, trailing-slash and uppercase → 403, **query parameters
ignored** (`?foo=bar&channel=canary` byte-identical to baseline),
**`logi-app-version` has no observable server-side effect** (3 tested
values; bucketing is driven by `logi-install-id` alone). S3
ListObjectsV2 (corrected bucket-root + `prefix` form) → 403 ×3 —
**enumeration disabled**. 403 is a single template; 263 vs 243 B is just
variable `RequestId`/`HostId` length. v1/v2 `update.json` for ghub12 are
the **same object** (identical ETag); no v1 route literal exists in either
client binary — v1 is server-side legacy. Depot objects are CDN-cached
(`x-cache: Hit`, age ≈ 19 h on HEAD; `Miss` on Range requests).

---

## 3. Manifest schemas

### 3.1 `details.json`, protocol v2 (the main manifest)

Top-level keys [LIVE] (ghub10/win/public, 2026-09-06):
`appId, platform, channel, buildId, version, branch, lastModified, uuid,
signatures, depots, keys`

```jsonc
{
  "appId": "ghub10",
  "platform": "win",
  "channel": "public",
  "buildId": 634218,                      // integer build number
  "version": "2025.9.814156",             // CalVer-ish string
  "branch": "staging/2025_9_ghub10",
  "lastModified": "2025-12-12T05:13:22Z", // ISO-8601
  "uuid": "012e1752…5884",                // 64-hex manifest identity
  "signatures": {                         // TOP-LEVEL MANIFEST SIGNATURE
    "version": 2,
    "signatures": ["5be833c8…cfe"]        // raw hex strings (512 bytes each, RSA-4096)
  },
  "depots": [ /* 648 entries, see below */ ],
  "keys": {
    "accessGroup": "323e77f5-68c2-43d8-8457-818bbd663938"  // keymaster group UUID
  }
}
```

Depot entry — all 648 entries carry `signatures` [LIVE] (hex length 1024,
i.e. 512 bytes, for every one):

```jsonc
{
  "name": "core",                         // human name OR UUID (encrypted capsule depots are named by UUID, see §5.5)
  "size": 270677711,                       // exact byte count of the depot object
  "url": "/depots/3b51d0b7-…-ca1d6dea66c8/core.depot",  // relative in v2
  "mac": "5b3f76fa…9798",                  // 64-hex: SHA-256 of the raw depot bytes (CONFIRMED, §5.7)
  "signatures": {
    "version": 1,
    "signatures": [ { "signature": "6ac80c83…381d" } ]   // object-wrapped hex, 512 bytes
  },
  "required": true,                        // present on 7/648 entries
  "dependsOn": ["gl","core_systray_win","core_apps","core_assets","core_runtime_win","core_qt_win","gl_resources"]  // present on 256/648 entries
}
```

Census for ghub10/win/public 2025.9.814156 (buildId 634218): 648 depots,
3,770,380,499 total declared bytes, 648 distinct dir-UUIDs (one per depot),
7 with `required`, 256 with `dependsOn`, 648/648 signed.

**ghub12** (win/public): 992,526 B manifest, same schema;
`buildId` 710935, `version` "2026.2.861817", `branch`
"staging/2026_2_ghub12", `lastModified` "2026-04-13T15:16:53Z" [LIVE
re-probe 2026-09-07]; its `keys.accessGroup` =
`e37b37e8-a368-477e-8cb9-2cc6cb1d8324`.

### 3.2 `details.json`, protocol v1

Same core identity fields (`appId, platform, channel, buildId, version,
branch, lastModified`) and a `depots` array, but **[LIVE]**:

- **no** `uuid`, **no** `signatures`, **no** `keys`
- depot entries add `"cipherSuite": "none"` (all 648) and use **absolute**
  S3 URLs: `https://2pipeline.s3.amazonaws.com/depots/{dirUuid}/{name}.depot`
- same 648 depots, same `mac` values, same sizes as v2 (verified for the
  shared depots)

So v1 and v2 describe the same build with different URL style and a
signature/key envelope in v2.

### 3.3 `update.json`

197 B, the lightweight "what's current" pointer [LIVE]:

```json
{
  "appId": "ghub10",
  "platform": "win",
  "channel": "public",
  "buildId": 634218,
  "version": "2025.9.814156",
  "branch": "staging/2025_9_ghub10",
  "lastModified": "2025-12-12T05:13:22Z"
}
```

**[LIVE 2026-09-07]** `update.json` is bucketed by `logi-install-id` exactly
like `details.json` (same probe set, `probe_h6_rest.mjs`): canary-bucketed id
returns `"channel": "canary"` with the canary `lastModified`; the only fields
that differ between buckets are `channel` and `lastModified` (body stays
197 B).

### 3.4 `settings.settings` / `/settings`

**403** on all live probes (public and canary). Body is S3 `AccessDenied`
XML. The binary's actual path suffix is **`/settings`** (9 B literal, no
`.json`); the expected response is a `logi.protocol.updates.Settings`
proto-JSON (`{"settingsJson": "..."}` per the code comment) and it feeds
FeatureCanary (§2.0.4). On this host the object is not exposed (403 on both
`settings.settings` and the code path's `/settings`), so the schema remains
unobserved live — **[UNRESOLVED]** whether the object is dead here or
restricted by policy.

---

## 4. Keymaster (access) schemas

### 4.1 `content.json`

```jsonc
{
  "accessGroup": "323e77f5-68c2-43d8-8457-818bbd663938",
  "lastModified": 1765286821,              // unix epoch seconds (2025-12-09T18:47:01Z)
  "keys": [
    {
      "name": "c91f9b21-5280-44de-9e67-9c39aaea28ad",  // UUID; also the PBKDF2 salt (§5.6)
      "version": 1,
      "lastModified": 1756990707,
      "key": "nPq32LjaydO0k9bKn5I3/g=="    // base64 of 16 raw bytes → AES-128 key
    }
    /* 71 keys for ghub10, 71+ for ghub12 */
  ]
}
```

**[OFFLINE-PROOF]** `key` always decodes to 16 bytes (`verify-saved-sample.mjs`
asserts this for the key used by the saved depot).

**[BINARY] How the updater finds this file:** the manifest's top-level
`keys.accessGroup` UUID is read by `long_json_download_job::get_details`
(0x140265360; missing/empty → error "Build access group ID is missing or
empty!", code 702/703), and the content URL is built by
`build_content_json_url` (0x1402692a0) = `/pipeline/v2/access/` +
`{accessGroup-uuid}` + `/content.json`.

**[CONFIRMED 2026-09-07] Key-by-name design.** The encrypted depot header
field `key-id` (§5.2) is a **`name` of a key in this very list** — depots
reference protection keys *by UUID name* rather than embedding them, and
the client resolves the name through this endpoint + its local cache
(`keys.json`, §18.3). Live cross-check on the saved sample depot:
`key-id` `b895e0bb-0970-4eb5-a623-ab5abc2fddfd` is present in the ghub10
group content.json (71 entries), entry =
`{ name: b895e0bb-…, version: 1, lastModified: 1652975873 (2022-05-19T22:37:53Z),
key: "L/zJSb8VtDMxwHGPKJRZEw==" (2ffcc949bf15b43331c0718f28945913) }`.
Old-format depots (pre-`0x20210506`) embedded the full key object instead.
Re-probed 2026-09-07: content.json body unchanged (11,536 B, same 71 keys;
`RE_Work/probes/reprobe_access_keys_2026-09-07/`).

### 4.2 `iat.json`

`{ "lastModified": 1765286821 }` [LIVE] — same epoch value as `content.json`
for the same group. Purpose (invalidate-at / index-age tracking?) is
**[UNRESOLVED]**.

---

## 5. Depot file formats

Common envelope for all observed formats [OFFLINE-PROOF]
(`RE_Work/tools/dump_depot_headers.js`):

```
offset 0:  u32 LE  magic
offset 4:  u32 LE  headerJsonLen
offset 8:  headerJsonLen bytes of UTF-8 JSON (header object)
offset 8+headerJsonLen: payload — u32-length-prefixed chunks for every magic
```

### 5.1 Format A — plaintext multi-file depot, magic `0x20170110`

Observed on: `core`, `core_apps`, `gl`, all device-firmware depots
(`g502`, `g403`, `g305`, `driver_audio_*`, …) and on all 5 fully downloaded
samples. Header:

```json
{ "files": [ { "name": "manifest.json", "mode": 33206 }, … ] }
```

Payload (after the header JSON): the same **length-prefixed chunk** layout as
Format B — one `[u32 LE len][data]` chunk per file, in `files[]` order.
[OFFLINE-PROOF, 2026-09-07] `RE_Work/probes/check_plain_framing.mjs` frames all 5 saved
plain depots exactly to EOF (19,392 / 632 / 192,267 / 97,930 / 21 B), matching the
binary: `capsule_write_data_chunk` (0x14020EA20) reads the u32 length prefix before
building the stream chain in every format — the plain depot is the no-key,
no-compression degenerate case of that same code path. Plain file-list entries
carry only `name` + `mode` (no `sha`, no `link`) [OFFLINE-PROOF, all 5
samples]; both are optional in the binary's 104-byte record (§5.4).

File `mode` values [OFFLINE-PROOF, int_convert-verified]: standard POSIX
st_mode values — `33188 = 0o100644`, `33206 = 0o100666`,
`33279 = 0o100777` (regular file + permission bits). [OWN] our builder
(`UpdateManager.cpp`) writes `33188` for regular files.

Sample header sizes show the list can be very large (`core`: jsonLen 32,601
B; `core_apps`: 49,603 B) [LIVE `magic_inventory.json`].

### 5.2 Format B — encrypted capsule, magic `0x20210506`

Observed on 24 sampled depots whose **names are UUIDs** (e.g.
`85875e86-f3e1-4e79-91ee-232575e2807f`) [LIVE `magic_scan_broad.json`].
Header fields [BINARY: `read_capsule_filter_header` 0x14020B2C0 dispatches on
the magic; `parse_capsule_header` 0x14020B5F0 reads them]:

```json
{ "header-sha": "a59dd1b3…122f1", "key-id": "b895e0bb-0970-4eb5-a623-ab5abc2fddfd" }
```

| Field | Required | Purpose |
|---|---|---|
| `header-sha` | **yes** — `json.at(field)` throws nlohmann type_error 302 if missing or non-string | PBKDF2 password (ASCII hex) for chunk 0; must equal `sha256(chunk-0 plaintext)` |
| `key-id` | no | keymaster `find()` → the 16-byte AES key for all chunks |
| `compression` | no | if `"xz"`, wrap the decrypted stream in `DecompressedStream` (xz is the only code — type 2) |

Magic gate [BINARY]: `0x20170110` → empty header (plain, §5.1); `0x20210506` →
header above; any other magic →
`capsule_exception("Capsule magic header mismatch!")`. The literal
`"header-sha"` (0x140F46778) is passed as a **runtime parameter** (SSO stack
string, len 10) — which is why string-table scans of the parser body missed
it. Xrefs of the literal: `read_capsule_filter_header` (0x14020B2C0) and
`sub_14023E920` (§14e). Note also `check_headersha.mjs`: `header-sha` is **not**
`sha256` of the header JSON bytes — it hashes the chunk-0 plaintext.

Payload: **length-prefixed chunks** [OFFLINE-PROOF on the saved 175,132 B
depot; 9 chunks, last ends exactly at EOF]:

```
repeat: u32 LE chunkLen, chunkLen bytes
  chunk[0]: encrypted JSON  { "files": [ {name, mode, sha}, … 8 entries ] }
  chunk[i]: encrypted payload of files[i-1]
```

Decryption — **fully reproduced offline** for the saved sample
(`RE_Work/review/verify-saved-sample.mjs` → PASS):

| Parameter | Value |
|---|---|
| Cipher | AES-128-GCM |
| Key | base64-decode of the keymaster `key` whose `name == header["key-id"]` (16 bytes) |
| IV (per chunk, 32 bytes) | `PBKDF2-HMAC-SHA512(password = <expected SHA as ASCII hex string>, salt = key.name (UUID string), iterations = 1000, dkLen = 32)` |
| Expected SHA, chunk 0 | `header["header-sha"]` |
| Expected SHA, chunk i+1 | `files[i]["sha"]` |
| AAD | none [BINARY `DecryptedStream_ctor` 0x140272FB0 — none is ever set; see tag note] |
| Key struct | `Key` { `name` string @+0x10 (the PBKDF2 salt), base64 key @+0x18 (standard base64 → 16 B), `version` @+0x28 — must be 1, else `capsule_exception("Unsupported key version: %d")` } [BINARY] |

After decryption, `sha256(chunk0 plaintext) == header-sha` and
`sha256(chunk[i] plaintext) == files[i-1].sha` held for all 9 chunks of the
saved sample. **[BINARY, closed 2026-09-07]** the updater itself makes exactly
this call: `DecryptedStream_ctor` (0x140272FB0; its vtable symbol proves the
signature `DecryptedStream(const Key&, std::string const&, shared_ptr<buffered_stream>)`)
calls OpenSSL `PKCS5_PBKDF2_HMAC` (0x140ABA080, renamed) with
`pass = <per-chunk SHA as ASCII hex>`, `salt = Key.name` (Key+0x10),
`iter = 0x3E8 (1000)`, `md = EVP_sha512` (static EVP_MD @0x14104DAA0,
`md_size = 64`), `keylen = 32` → the 32-byte IV; it then standard-base64-decodes
Key+0x18 via `base64_decode` (0x140C139C0, renamed; suffix arg `"=+/"` =
padding + the 62/63 chars, i.e. the standard alphabet) into the 16-byte AES key and initializes the
AES-128-GCM context at `this+0x20`. This is the verified recipe; it matches the C++ reference
([OWN] `UpdateManager.cpp:764-817`, `Utils/Encryption.h` — PBKDF2
`0x3E8 = 1000` iterations, 0x20-byte output, per-file IV at line 817).

**GCM tag: [OFFLINE-PROOF, 2026-09-07] not present in the stream for this
sample.** `RE_Work/probes/decrypt_headersha.mjs` (fixed) tested both tag
layouts — last-16 and first-16 bytes of each chunk — using the WebCrypto
convention (tag is the final 16 bytes of `data`). Both **fail
authentication** for every chunk. The unauthenticated pass (no tag supplied)
recovers all 9 chunks with `sha256(plaintext) == expected SHA` for each.
Conclusion: for the saved `0x20210506` capsule, GCM is used **without a
verifiable tag** (the C++ reference likewise never feeds one in and ignores
`Final` failure). Integrity relies on the per-chunk SHA chain + the depot
`mac`, not on GCM authentication. [HYPOTHESIS] this may hold only for
capsule depots; whether a tag ever appears (e.g. in `0x20210521` single-file
depots) is still open.

### 5.3 Format C — encrypted single-file depot, magic `0x20210521`

**[BINARY, closed 2026-09-07]** A third magic: the **individual-file
capsule**. Layout: identical to Format B — same `[u32 magic][u32 len][JSON
header]` + u32-length-prefixed data chunks, same AES-128-GCM/PBKDF2 recipe —
but the required header SHA field is **`files-sha`** instead of `header-sha`:
`read_indiv_file_first_chunk` (0x14020B4E0) passes it as the runtime field
name to `parse_capsule_header`. `check_indiv_file_magic` (0x14020A0A0)
accepts only `0x20210521` and rejects the two capsule magics with
`capsule_exception("Is a capsule, not an individual file")` (magic cached at
reader +112, flag +120). Write path: `write_indiv_file_to_stream` (0x140092740,
8 MiB reader) — magic `0x20210521` → header +
`capsule_write_data_chunk(pwd = files-sha)`; anything else → raw buffered
copy. Magic values (all `int_convert`-verified): `0x20170110` = 538378512,
`0x20210506` = 539034886, `0x20210521` = 539034913. No live sample captured
(0 of 60 prefixes, 2026-09-06) — sampling one would close the GCM-tag
question for this variant (§14a).


### 5.4 File records and extraction flow [BINARY]

The files-list JSON entries parse into **104-byte records**
(`file_list_from_json` 0x140208F90, renamed in IDB):

```
offset 0:   std::string name   (required)
offset 32:  int       mode     (POSIX st_mode)
offset 40:  std::string link   (optional; non-empty = symlink)
offset 72:  std::string sha    (optional; PBKDF2 password = sha256(plaintext)
                               hex, for this file's data chunk)
```

Extraction (`extract_capsule_to_dir` 0x14020E3E0 → `extract_capsule_files`
0x140208930): create the output directory (error: "Failed to create depot
directory '<p>' (<err>)"), read the filter header, read the files-list chunk
(`read_capsule_json_chunk` 0x14020BE70), parse, then iterate records with a
104-byte stride:

- `link` empty → open `outdir/name` (`std::ofstream`, error: "Failed to open
  file for writing: ") and `capsule_write_data_chunk(reader, stream,
  record.sha)` — for Format A (no key, no `sha`) this degenerates to the plain
  u32-length-prefixed copy;
- `link` non-empty → symlink entry in the directory tree, **no data chunk** in
  the stream.

Intermediate directories are created on demand in a 0x28-byte linked dir map
(dir mode 0x101 = 257; error: "Failed to create folder for extracted file.").

### 5.5 Naming convention observation

[LIVE] All 36 sampled `0x20170110` depots have human-readable names
(`core`, `g915_us`, `driver_audio_apo`); all 24 sampled `0x20210506` depots
are named by UUID. **[HYPOTHESIS]** capsule depots carry app-internal
content (web assets, per the decrypted sample: `front.webp`,
`metadata.json`, `device_presets.json`) and are content-addressed.

### 5.6 Key-identity chain (end to end, all confirmed)

```
manifest depots[i].name "85875e86-…"
  → depot header key-id "b895e0bb-…"
  → /pipeline/v2/access/323e77f5-…/content.json keys[]
     entry name == "b895e0bb-…", key = base64 16 B
  → Key struct { name (PBKDF2 salt), base64 key 16 B (std alphabet, `base64_decode` 0x140C139C0), version 1 }
  → AES-128-GCM key for all chunks of that depot
```

**[DISPROVEN correction]** An earlier hypothesis that keymaster "keys" are
per-feature keys, not per-depot encryption keys, was **wrong**: the depot
header `key-id` matches a keymaster key 1:1 and that key decrypts the depot
[OFFLINE-PROOF].

### 5.7 Depot `mac`

`mac` = **SHA-256 of the raw depot object bytes** — confirmed for 6 depots
(5 plaintext + 1 encrypted capsule) [OFFLINE-PROOF]. It is also the value
the signature is ultimately anchored to (§6.1).

---

### 5.8 CapsuleMetadata — random-access resource reads ("resource_access") [BINARY, 2026-09-07]

The updater does not only extract depots. `lghub_updater.exe` also indexes a
depot in memory and reads individual files out of it by name, decrypting and
decompressing **in place, per read** — no extraction to disk. Both modes exist
and are selected per build; the random-access mode falls back to full
extraction on any failure.

**CapsuleMetadata (`logi::pipeline::CapsuleMetadata`, 96 B)** — built by
`capsule_metadata_create` (0x14023D680) from an open depot stream
(ifstream mode 33):
- `+0`  shared_ptr key context (Key from the keymaster; see §5.6)
- `+16` transformation chain (decrypt/decompress stream factory)
- `+32` `vector<104 B FileRecord {name@0, mode@32, link@40, sha@72}>`
- `+56` offset tree root A, `+64` flag/ptr, `+72` size tree root B
- `+88` u32 magic gate: accepts `0x20170110` and `0x20210506` only
  (the `0x20210521` single-file format has **no** random-access index).
Construction steps: `capsule_metadata_parse_header` (0x14023E920) →
`capsule_metadata_parse_files` (0x14023DB30, names/modes/links/shas from the
header JSON) → `capsule_metadata_scan_chunks` (0x14023D210, physical walk of
the `[u32 len][data]` chunks filling the offset/size trees; the i-th chunk
pairs with the i-th regular `files[]` entry).

**Three public entry points** (each opens the depot path, runs a worker, and
returns a 41-B result struct `{message@0, code@32, status@40}` with
status `0`=ok, `1`=fail, `-1`=unknown):
- `capsule_open_file` (0x140200EF0) → worker
  `capsule_open_resource_worker` (0x14023F5C0)
- `capsule_verify_file` (0x140201890) → `capsule_verify_file_worker`
  (0x14023FED0)
- `capsule_list_resources` (0x1402015F0)

**Worker flow (0x14023F5C0):** key-ctx null ⇒ `"Capsule metadata is null"`;
record lookup by (normalized) name miss ⇒ `"Resource '%s' not found in
capsule"` (followed by the full name listing); symlink target missing ⇒
`"Broken symlink in depot: '%s' -> '%s' (target not found)"`; missing
offset/size ⇒ `"Missing offset or size for resource '%s' in capsule
metadata"`. The record's fields are copied into the stream factories:
`mode` (u32), `link` string, and the **`sha` string**.

**Two stream factories** (per resource):
- plain depots → `capsule_make_buffered_resource_stream` (0x14023F180,
  `CapsuleResourceBufferedStream`)
- capsule depots → `capsule_make_protected_resource_stream`
  (0x14023F220), signature (verified by register trace of the call at
  0x14023FC62 + factory disasm):
  `(out_sp, &transform_sp, &stream_sp, &offset_u64, sha_string, &size_u64)`.
  It allocates a `_Ref_count_obj2` (16-B control block; the object is
  ctrl+0x10, 0xE0 B) and calls the ctor (0x1402721E0).

**`CapsuleResourceProtectedStream` field map** (ctor 0x1402721E0, confirmed
by re-decompile + disasm — note: the decompiler's `(_DWORD)v10 + 16` argument
form is misleading; the real asm passes `lea rcx,[rbx+10h]`):
- `+0x68/0x70/0x78` buffer triple `{start, commit, capacity-end}` — a
  **zeroed `size`-byte buffer** (`capsule_buffer_alloc` 0x140271600)
- `+0x80` shared_ptr to the depot `istream` (shared with all resources)
- `+0x90` **offset** u64 (start of the record's chunk *data*, i.e. after its
  u32 length prefix)
- `+0x98` **size** u64 (the chunk's u32 length, from the size tree)
- `+0xA0` consumed bytes (0), `+0xA8` end = offset+size (recomputed per fill)
- `+0xB0` shared_ptr to the 0xA0 protection context (shared from the
  CapsuleMetadata transformation chain)
- `+0xC0` **the record's `sha` string** (hex) — *not* the display name
  (earlier labeling corrected). Ctor throws `capsule_exception`
  `"Protection data is not provided."` (msg @0x140F4D680) when the
  protection context is missing/invalid.

**underflow** (0x140272920, vtable slot 6 — disassembled fully):
1. g-region non-empty → return `*gptr` (no accounting).
2. g-region empty: `consumed += g-region size`; if `consumed ≥ size`
   recompute `end = offset + size`, `consumed = 0` (offset field itself is
   never advanced).
3. Head reset: `capsule_buffer_set_commit(buf, size)` — sets the commit
   pointer of `{start, commit, cap}` to `start+n` (zero-extends on grow,
   plain move on shrink; the name "reset/commit" fits both call sites).
4. `fpos = offset + consumed`; `stream->seekg(fpos)`; `tellg` → `pos`;
   `pos < offset` or `pos ≥ end` → `setg(NULL)` → EOF (-1).
5. `to_read = end − pos` (the clamp against the buffer-end address is a
   no-op in practice); `istream::read(stream, buf_start, to_read)`;
   `gcount == 0` → EOF.
6. Tail commit: `capsule_buffer_set_commit(buf, gcount)`.
7. **Protection step** (if `+0xB0` set and the sha string is non-empty and
   `ctx+0x58` is not aliased into it): `std::string::assign(ctx+0x58,
   this->sha)` — the 0xA0 protection context's **PASSWORD slot (+0x58) is set
   to the record's sha** — then `apply_capsule_protection_to_buffer`
   (0x140240D40): wraps the buffer in a `SingleBufferStream` (0x1402407A0),
   builds the standard decrypt+decompress chain (`build_capsule_stream_chain`
   0x140240B40 — PBKDF2-HMAC-SHA512(sha, Key.name, 1000, 32 B) IV, AES-128-GCM
   tagless, then xz) and pumps **in place**, shrinking commit to the
   plaintext size.
8. `end = offset + (commit − start)`; `setg(start, start, commit)` → the
   g-region becomes the whole (transformed) buffer; return first byte.

**Consequence (closes the round-2 password question):** the random-access
path decrypts with **password = record.sha — exactly the same password the
extract path uses** (`capsule_write_data_chunk` pwd = record.sha, §5.4). One
shared transform primitive (0x140240D40) serves *all* read paths: extract,
verify, header read, and random-access underflow.

**Vtable** (0x140F4D608; plain variant 0x140F4D578): base streambuf stubs
(`_Lock`/`_Unlock`/`overflow`/`pbackfail`/`showmanyc`/`uflow`/`xsgetn`/
`xsputn` at 0x140DF8xxx/0x140DFF5xx), slot 6 = underflow, slots 10/11 = a
class seek pair (`seek_entry` 0x140272670: mode 0 = absolute, mode 1 =
derived from current g-region; `seek_impl` 0x1402727C0: validates the target
against `[0, size]`, adjusts g-region/consumed, writes the 16-B fpos).
Slots 12–14 contain a shared 3-qword tail (`0x140DFF7FC/F6/F0`) present
identically in every streambuf vtable in the binary — never called, exact
role unverified.

**Mode selection — `build_resource_access` (0x14008E4D0):** builds an access
object (`+40` key ctx: build-usage flag 0→usage 1, 1→usage 2, else throw
`"Unknown build usage, this should never happen"`; `+56` base path; `+64`
cache flag) and uses the depot at `join(join(base, "cache"), depotName)`.
Failure logs `"Couldn't get a list of resources from capsule '%s' since it is
not readable. Reason: %s. Depot state: %s. Falling back to depot approach."`
and invokes the full-extract fallback `depot_extract_fallback`
(0x14008E250 = `extract_capsule_to_dir`). Success lists names via
`capsule_list_resources`, filters them (0x140036820), and serves each file
through `resource_access_get_resource` (0x14008EEF0), which runs
`capsule_open_file` + `capsule_verify_file` (per-file SHA-256 check,
`verify_buffer_sha256` 0x14026F9C0) then streams the bytes into the target
(`write_indiv_file_to_stream`); its errors:
`"Resource '%s' from capsule '%s' (%s) is not readable. Reason: %s. Depot
state: %s"` and `"Error '%s' when accessing resource '%s' from capsule '%s'
(%s). Depot state: %s. Falling back to depot approach."` (event tag
`"resource_access"`).

Random access works because **each chunk is fully independent**: own u32
length, own PBKDF2-derived IV, own xz stream — so any file can be read
directly at `offset` without touching the rest of the depot.

---

## 6. Signature verification

### 6.1 Depot signature scheme — EMPIRICALLY CONFIRMED [OFFLINE-PROOF]

**The depot v1 signature is RSA-4096 PKCS#1 v1.5 over
`SHA-256(SHA-256(raw depot bytes))` — i.e. RSA over the raw 32-byte SHA-256
digest (a "double hash"): the inner SHA-256 is the depot `mac`, the outer
SHA-256 is applied to the 32-byte mac digest itself.**

Proof (reproducible: `node RE_Work/tools/rsa_forensics.js`):

1. For saved depot `85875e86-…` (175,132 B): manifest `mac` =
   `688bc41d…f69` = `sha256(raw bytes)` (directly recomputed).
2. Its 512-byte signature, fed to `crypto.publicDecrypt(sig, {key: 4096-bit
   ghub PEM, padding: RSA_PKCS1_PADDING})`, **succeeds only** with the
   embedded 4096-bit key (2048-bit keys fail with `data greater than mod
   len`).
3. The decrypted 512-byte block is exactly:
   PKCS#1 v1.5 `0x00 0x01 <0xFF…> 0x00` + DigestInfo
   `3031300d0609608648016503040201 05000420` + 32-byte digest
   `46d0dc31…a61`.
4. `46d0dc31…a61 == sha256(the 32-byte mac digest)`, exactly.

### 6.2 Key selection — app-name table [BINARY]

`select_signature_key_v1` (0x140E50900) and `select_signature_key_v2`
(0x140E50A10) select the public key by **`memcmp` of the application name
(`std::string`) against three literals**:

| App name | Length | Bits | v1 PEM address | v2 PEM address |
|---|---|---|---|---|
| `updaterservice` | 14 | 2048 | 0x141340DF5 (`pub_key_pem_updaterservice_v1`) | 0x1413419B5 (`pub_key_pem_updaterservice_v2`) |
| `ghub` | 4 | **4096** | 0x1410E9020 (`pub_key_pem_ghub_v1`) | 0x1413414B0 (verified by bytes; commented in IDB) |
| `optionsplus` | 11 | 2048 | 0x141341195 | 0x1413417E5 |
| anything else | — | — | empty → log "Invalid application key" | — |

Extracted PEMs (first occurrence per fingerprint; `fp` = first 16 hex chars
of `SHA-256(modulus)`):

| File | Bits | fp |
|---|---|---|
| `RE_Work/probes/pipeline_pubkey_1.pem` | 2048 | `b97bd09079792cd4` |
| **`RE_Work/probes/pipeline_pubkey_2.pem`** | **4096 — the ghub key** | **`01f43ddda220be2a`** |
| `RE_Work/probes/pipeline_pubkey_3.pem` | 2048 | `a4600dc4f2c36ded` |

**[OFFLINE-PROOF + BINARY] The updater runs as app `"ghub"`.** All 1,333
live signatures in the two captured manifests (648 depots × 2 + 2 top-level)
are 512 bytes — only the 4096-bit `ghub` slot can produce/verify those — and
the ghub 4096 key verifies the depot signature forensically. The 24-byte
global holding the app name (0x1413D8B28) is zero-initialized in `.data` and
written indirectly (register-based store invisible to xrefs); the static
setter could not be pinned down, but the runtime value is empirically
`"ghub"` as above. ("updaterservice" is the Windows SCM service name from
`startup_shutdown.cpp` context, not the pipeline app name.)

### 6.3 Verification call chain in the binary [BINARY]

```
depot_validators_check_signature (0x1402636E0)
  │  depot-info struct: +104 TBS std::string, +112 sig-vector ptr, +128 version
  │  version 1 → verifier slot +248, version 2 → slot +280, else "Invalid application key"
  │  ANY signature in the vector succeeding → success
  │  all fail → "Depot file does not match any of its signatures"
  ▼
depot_verifier_rsa_verify_depot (0x14027A190)
  │  EVP_MD_CTX_new
  │  EVP_DigestVerifyInit(ctx, NULL, <SHA-256 EVP_MD>, NULL, pkey)
  │  hex_decode_to_string (0x140C0DC10) — sig hex → raw bytes
  │  EVP_DigestVerifyUpdate(ctx, <input>)
  │  EVP_DigestVerify(ctx, sig, siglen)
  │  logs: "Computed digest of depot capsule:",
  │        "Signature successfully verified for depot capsule.",
  │        "Digest verification failed. Signature does not match."
  ▼
SHA-256 EVP_MD descriptor (0x14104D960, `evp_md_sha256`)
     returned by thunk get_evp_md_sha256 (0x140ABA030, lea+ret)
     words: {672, 668, 32, 8, 3 fn-ptrs, NULL, NULL, 64, 120}
     672 = NID_sha256, 668 = NID_sha256WithRSAEncryption (BoringSSL nid.h)
     trailing words 64/120: [UNRESOLVED] field mapping (block size / ctx size?)

key loading:
depot_verifier_rsa_set_public_key (0x140279D70)
  │  PEM string_view → BIO_new_mem_buf → PEM_read_bio_PUBKEY
  │  → EVP_PKEY_CTX_new → EVP_PKEY_verify_init
depot_verifier_rsa_init (0x140278DE0) — object init, vtable sub_140ABE0B0
```

**[BINARY + OFFLINE-PROOF, closed 2026-09-07]** The `+104` TBS std::string
`check_signature` passes to the RSA verifier is the **32-byte mac digest**
(raw, not hex) for version-1 depot signatures — this is the only input
consistent with the offline proof (§6.1) and the "Computed digest of depot
capsule" log. **CONFIRMED** two ways: (1) `RE_Work/tools/tbs_digest_test.mjs`
reproduces the RSA PKCS#1 v1.5 signature by feeding the verifier
`sha256(raw 32-byte mac digest)` under the embedded ghub 4096 key; (2) the
binary's `EVP_DigestVerifyUpdate` input is that raw 32-byte digest (struct
+104 in `check_signature` 0x1402636E0), so the signed value is
`sha256(raw digest)` — exactly the §6.1 scheme. Open item §14d is closed.

### 6.4 Top-level v2 manifest signature

`signatures: {version: 2, signatures: [<1024-hex string>]}` [LIVE].
Forensically the same key and padding: `publicDecrypt` with the ghub 4096
key yields a SHA-256 DigestInfo whose digest is
`3ff742238baf70bb4374ab7ae9361ae11e2b48942f83ffc92e1ce4124035e656`.

**[BINARY, closed 2026-09-07] The updater never verifies the top-level v2
manifest signature at all.** `depository_from_details_json` (0x14026D920,
renamed) accepts exactly `{version, appId, buildId, branch, depots}` and never
reads the top-level `uuid`, `signatures`, or `keys`; `depot_signatures_from_json`
(0x14026EC60, renamed) is the **only** per-depot signature parser and throws
"Invalid signatures version" for any per-depot `signatures.version` > 1 (v1
only); both `check_signature` call sites are depot-level. The TBS is whatever
the pipeline server chooses to sign — the client does not consume it, so the
offline TBS search below is moot by design (kept for the record): raw
`details.json` as fetched;
minified JSON; `\n`/`\r\n` variants; trailing-trimmed; `appId`/`buildId`/
`uuid`/`version` combinations; double-SHA-256 of each of the above;
`details.json` with `signatures` removed; with `depots` removed; with the
top-level signature object removed (inner digest `1299e57baf1541433f64a0c9`);
`update.json` raw (inner `d49209e5aef823d6a1b49d93`);
`settings.settings` raw (inner `6c6ee7d74b6060fc5dc18b62`).
(Open item §14b is closed as moot: no client-side v2 verify call site exists.)

### 6.5 Negative result on purpose [OFFLINE-PROOF]

`node RE_Work/tools/verify_rsa_sigs.js` documents that verifying depot
signatures against the **raw depot bytes** (single hash) with all three
embedded keys fails — the double-hash scheme is what makes the raw-bytes
approach wrong. Kept as a regression marker.

---

## 7. Wire protocol (protobuf) evidence [BINARY]

String evidence for a protobuf package `logi.protocol.updates`
(proto path `logi/updater_ipc/protocol/messages/v1/pipeline.proto`) with
messages/fields (names as they appear in strings; exact field numbers not
recovered):

- `Depot`: `cipher_suite, iv, key, mac, depends_on, files, url, name,
  local_folder`
- `Depository`: `build_id, version, branch, region, access_group,
  lockdown_name`
- `Channel`: `pipeline_host, name, password, access_groups`
- `AccessGroup`, `Signature`, `PeriodicCheck`, `Settings`, `ContentRequest`,
  `Application_PipelineConfiguration` (`default_channel`,
  `default_password`)

[HYPOTHESIS] These describe the IPC wire between the UI and the updater
service and/or the request shapes toward the pipeline, not the JSON manifest
wire format (which is plain JSON as captured). `region` and `lockdown_name`
hint at regional deployment and a lockdown mode, both **[UNRESOLVED]**.

---

## 8. Other updater capabilities seen in the binary [BINARY]

- **xdelta differential depots:** `xdelta_patcher::impl::patch`,
  `download_differential_depot_async`; config key `xdelta_enabled`
  (default 1). **[UNRESOLVED]** the differential depot layout (likely xdelta3
  patch + base-depot reference; no sample captured).
- **Compressed depots:** `capsule_download_group::
  download_compressed_depot_async`; `DecompressedStream_ctor` (0x140272C00,
  renamed). **[RESOLVED 2026-09-07]** algorithm = **xz only** (type code 2,
  literal `xz`); optional per depot via the header `compression` field;
  stream order = decrypt → decompress (`build_capsule_stream_chain`
  0x140240B40, renamed).
- **Capsule extraction:** `extract_capsule_to_dir` (0x14020E3E0) →
  `extract_capsule_files` (0x140208930): create outdir, filter header,
  files-list chunk, then per 104-byte record — regular file →
  `capsule_write_data_chunk(pwd = record.sha)` (degenerates to a plain
  u32-length-prefixed copy for Format A), symlink (`link` non-empty) →
  directory entry with **no data chunk** (§5.4). `pipeline://` URIs resolve to
  these extracted paths.
- **Local cache:** `simple_cache_manager::*` — depots are cached locally
  before install (explains partial downloads; not reverse-engineered).
- **URI scheme:** `pipeline://` with regex
  `pipeline:\/\/([a-zA-Z0-9_-]+)\/([a-zA-Z0-9_\/.-]*)` and
  `pipeline_uri_to_local_url` — maps `pipeline://app/path` to a local
  extracted path.
- **Keymaster client:** `keymaster::*` symbols — the access-group/key
  fetching is a named subsystem (matches §4).
- **Config keys:** `xdelta_enabled` (def 1), `vfs_enabled` (def 1),
  `update_interval_from_config_file_enable` — read by
  `pipeline_configuration_factory` (0x140E02460).
- **Crash report:** sentry-native 0.9.1; build farm path
  `C:\builds\kragle\lego\logi\…`, Conan 1.66.0 (provenance, not protocol).
- **Canary:** string `canary_machine_identifier` — machines can be tagged
  into the canary channel. **[UNRESOLVED]** how the tag is set.

---

## 9. Live probe matrix (2026-09-06)

| Probe | Result |
|---|---|
| `GET /pipeline/v2/update/ghub10/win/public/details.json` | 200, 938,799 B |
| `GET /pipeline/v2/update/ghub10/win/canary/details.json` | 200, 938,799 B |
| `GET /pipeline/v2/update/ghub12/win/public/details.json` | 200, 992,526 B |
| `GET /pipeline/v2/update/{ghub10,ghub12}/win/{public}/update.json` | 200, 197 B |
| `GET /pipeline/v2/update/ghub10/win/public/settings.settings` | **403**, 263 B S3 XML |
| `GET /pipeline/v2/update/ghub10/win/canary/settings.settings` | **403**, 243 B S3 XML |
| `GET /pipeline/v1/update/ghub10/win/public/details.json` | 200, 224,746 B |
| `GET /pipeline/v2/access/323e77f5-…/content.json` (ghub10 group) | 200, 11,536 B |
| `GET /pipeline/v2/access/e37b37e8-…/content.json` (ghub12 group) | 200, 12,985 B |
| `GET /pipeline/v2/access/{group}/iat.json` | 200, 32 B |
| `GET /pipeline/v2/access/{logitech,ghub,logigames,ghub10}/content.json` | **403** (name-based, 4 probes) |
| `GET /pipeline/v2/access/logitech/iat.json` | **403** |
| `GET /` | **403**, 0 B |
| 6 depot objects under `https://updates.ghub.logitechg.com/depots/…` | 200; sizes+SHA-256 match manifest (`verify-saved-sample.mjs`) |
| 60 depot first-512 B prefixes (magic scan) | 36× `0x20170110`, 24× `0x20210506`, 0× `0x20210521` |
| Re-probe 2026-09-07 (fixed script, same 17 paths) | Identical statuses/sizes: 8× 200 (938,799 / 197 / 992,526 / 197 / 938,799 / 197 / 224,746 / 197 B), 8× 403 S3-XML, root 403 0 B `text/html`. New data: `etag` + `Last-Modified` on every 200 (below); v1 `update.json` (ghub12) = 200; ghub12 buildId 710935 / version 2026.2.861817 |
| H3 probe 2026-09-07 (`probe_h3_s3.ps1`, `probe_h3_cf_https.mjs`) | `http://2pipeline.s3.amazonaws.com/depots/780f7572-…/g560_dfu.depot` **200** 632 B, SHA-256 = manifest `mac`; same depot path on `updates.ghub.logitechg.com`: **403 over HTTP**, **200 over HTTPS** (headers not captured for this object; manifest objects on the same host show CloudFront + S3-origin headers — §2.2) |
| H6 headers probe 2026-09-07 (`probe_h6_headers.mjs`, outdir `reprobe_h6_headers_2026-09-07/`) | `details.json` with `logi-install-id: 0123…` (64-hex) → **different body** than header-less: `channel` public→canary, same 938,799 B, different weak ETag (`W/"be3b37fb…"` vs `W/"93208d11…"`) — **server-side identity bucketing proven** |
| H6 ids probe 2026-09-07 (`probe_h6_ids.mjs`, outdir `reprobe_h6_ids_2026-09-07/`) | 7-request channel table: absent/empty → public; `0123/0000/aaaa/1111` → canary; `ffff`/`deadbeef` → public; repeat of one id ⇒ same ETag in 46 ms (deterministic, ~4/6 64-hex ids canary) |
| H6 rest probe 2026-09-07 (`probe_h6_rest.mjs`, outdir `reprobe_h6_rest_2026-09-07/`) | `update.json` ×2 buckets identically to details.json (197 B; only `channel`+`lastModified` differ); `/settings` ×2 → **403** S3 `AccessDenied` XML (code-path suffix, no `.json`) |
| Channel census 2026-09-07 (`probe_channel_names.mjs`, `probe_channel_brute.mjs`; outdirs `reprobe_channel_names_2026-09-07/`, `reprobe_channel_brute_2026-09-07/`) | `staging` 200 ct 938,800 B; `tim` 200 ct 809,601 B; `canary` 200 = public size; 14 other names → **403** S3 XML (§2.5) |
| Access-group re-probe 2026-09-07 (`probe_access_keys.mjs`, outdir `reprobe_access_keys_2026-09-07/`) | content.json 200, 11,536 B, 71 keys (unchanged); iat.json 200 `{ "lastModified": 1765286821 }` (§4.1) |
| v1/v2 cross-check 2026-09-07 (`reprobe_2026_09_07/*`) | ghub12 v1 `update.json` 200 (710935 / 2026.2.861817 / branch `staging/2026_2_ghub12`); ghub10 v1 details 200, 224,746 B = v2's 648 depots with `cipherSuite:"none"` + absolute S3 URLs, no `keys` (§3.2) |
| Variant matrix round 2026-09-07 (`probe_api_matrix{,2,3}.mjs`; outdirs `api_matrix_run{1,2,3}_2026-09-07/`, 30 reqs, 0 tool errors, no 429/503) | **ghub13 live** (update 190 B: 824196/2026.5.939708 = local build ⇒ H7 largely closed; details 1,106,793 B); **osx live** (197 B, 625362/2025.9.807501), mac/linux 403; ghub12×tim **403**, ghub12×staging 200 (198 B, ETag `7d44412e…`); query params **ignored** (byte-identical baseline); `logi-app-version` ×3 values **no observable effect**; Range **206** on manifests+depots, If-Modified-Since/If-None-Match **304**; trailing-slash/uppercase **403** (same template; 263 vs 243 B = RequestId/HostId length); S3 ListObjectsV2 (corrected form) **403** ×3 (listing disabled); depot HEAD `x-cache: Hit` (age ≈ 19 h) — CDN caches depots too; `pipeline.logitech.io` DNS → **private us-east-1 ALB** (172.30.91.104/118) — internal host, fetch failed (connection level); v1/v2 ghub12 `update.json` = same object (identical ETag). Full cell ledger: `RE/API_VARIANT_MATRIX.md` |

**[LIVE] Cache semantics (2026-09-07 re-probe,
`RE_Work/probes/reprobe_2026_09_07/probe_summary.json`):** every 200 carries
a strong S3-style `ETag` (quoted 32-hex, e.g. `"93208d1198a5be93a085849b1f128ee9"`
for ghub10 public details.json), a `Last-Modified: Mon, 08 Jun 2026
17:0x:xx GMT` (object re-upload date, **not** the content `lastModified`
field), and **no `Cache-Control` header**. 403 responses carry no cache
headers. So: conditional revalidation via `If-None-Match` is supported by
the front; there are no directive-based cache TTLs. (Range/resume
behavior still untested.)

**Caveat (historical):** the original `probe_summary.json` written on
2026-09-06 was corrupted by the probe bug (§16) — 8 successful 200s were
recorded as errors. It was **replaced on 2026-09-07** by the clean re-probe
summary (bodies under `RE_Work/probes/reprobe_2026_09_07/`); the 2026-09-06
evidence bodies in `RE_Work/probes/` are untouched.

---

## 10. Evidence index

| Evidence | File |
|---|---|
| v2 manifest ghub10 public (canonical) | `RE_Work/probes/resp_pipeline_v2_update_ghub10_win_public_details.json.json` |
| v2 manifest ghub12 public | `RE_Work/probes/resp_pipeline_v2_update_ghub12_win_public_details.json.json` |
| v2 manifest ghub10 canary | `RE_Work/probes/resp_pipeline_v2_update_ghub10_win_canary_details.json.json` |
| v1 manifest ghub10 | `RE_Work/probes/resp_pipeline_v1_update_ghub10_win_public_details.json.json` |
| update.json ×3 | `RE_Work/probes/resp_pipeline_v2_update_*_update.json.json` |
| settings 403 bodies ×2 | `RE_Work/probes/resp_pipeline_v2_update_*_settings.settings` |
| keymaster content ×2, iat ×2 | `RE_Work/probes/probe_access_*_group_{content,iat}.out` |
| name-based access 403 bodies ×5 | `RE_Work/probes/resp_pipeline_v2_access_*.json.json` |
| full encrypted capsule depot | `RE_Work/probes/depot_full_85875e86-f3e1-4e79-91ee-232575e2807f.bin` |
| 5 plaintext depots | `RE_Work/probes/depot_{applet_slobs,lua_scripting,release_notes,g560_dfu,driver_audio_osx}.bin` |
| 512 B depot prefixes ×2 | `RE_Work/probes/depot_prefix_*.bin` |
| magic censuses | `RE_Work/probes/magic_inventory.json`, `magic_scan_broad.json` |
| clean re-probe summary (2026-09-07) + bodies | `RE_Work/probes/reprobe_2026_09_07/probe_summary.json`, `.../resp_*` |
| embedded PEMs ×3 | `RE_Work/probes/pipeline_pubkey_{1,2,3}.pem` |
| string dump (1.4 MB) | `RE_Work/samples/lghub_updater_strings.txt` |
| offline GCM baseline (PASS×2 + LIMIT) | `RE_Work/review/verify-saved-sample.mjs` |
| review of prior session (bug list) | `RE_Work/review/qwen-session-review.md` |
| signature forensics tool | `RE_Work/tools/rsa_forensics.js` |
| negative raw-bytes verification | `RE_Work/tools/verify_rsa_sigs.js` |
| PE key extractor | `RE_Work/tools/extract_pem_keys.js` |
| schema dumper | `RE_Work/tools/dump_schemas.js` |
| depot header dumper | `RE_Work/tools/dump_depot_headers.js` |
| inner files-list dumper | `RE_Work/tools/dump_inner_files.js` |
| depot fetcher (UA, 400 ms delay) | `RE_Work/probes/fetch_depot.mjs` |
| TBS = raw 32-byte digest proof (§6.3) | `RE_Work/tools/tbs_digest_test.mjs` |
| plain-depot framing verifier, PASS×5 exact EOF (§5.1) | `RE_Work/probes/check_plain_framing.mjs` |
| header-sha ≠ sha256(header bytes); it hashes chunk-0 plaintext (§5.2) | `RE_Work/probes/check_headersha.mjs` |
| manifest v2 depot entries carry no iv/key/cipherSuite (§5.2) | `RE_Work/probes/check_iv_source.mjs` |
| H6 header probe (install-id flips channel) + summary | `RE_Work/probes/probe_h6_headers.mjs`, `RE_Work/probes/reprobe_h6_headers_2026-09-07/` |
| H6 id-bucketing table (7 ids → channel) | `RE_Work/probes/probe_h6_ids.mjs`, `RE_Work/probes/reprobe_h6_ids_2026-09-07/` |
| H6 update.json bucketing + `/settings` 403 | `RE_Work/probes/probe_h6_rest.mjs`, `RE_Work/probes/reprobe_h6_rest_2026-09-07/` |
| SecureStorage blob hex (396 B) + machine-id variant tests | `RE_Work/probes/h6_blob_full.txt`, `RE_Work/probes/h6_variant_test.mjs`, `RE_Work/probes/h6_variant2.mjs` |
| local update log (software manager 2026-08-08, H7) | `C:\ProgramData\Logi\GHUB\Logs\software_manager\lghub_08_08_2026.log` |
| staging/tim opaque bodies (captured, NOT decoded) + synthetic (circular) "recovery" kept for audit | `RE_Work/probes/reprobe_channel_brute_2026-09-07/staging_details.body`, `staging_update.body`, `staging_ks_analysis.json`; `staging_details_RECOVERED.json` = synthetic assumption artifact (§2.5, §13) |
| encrypted-channel key search results (0 hits, §2.5) | `RE_Work/probes/reprobe_channel_brute_2026-09-07/test_ks_modes_result.json`, `test_ks_deep_result.json`, `brute_summary.json` |
| channel census bodies (staging/tim/canary + 403s) | `RE_Work/probes/reprobe_channel_names_2026-09-07/`, `RE_Work/probes/reprobe_channel_brute_2026-09-07/staging_update.body`, `staging_details.body` |
| access-group re-probe (content.json 71 keys, iat.json, SSO ids) | `RE_Work/probes/reprobe_access_keys_2026-09-07/content.json.body`, `iat.json.body`, `sso_client_ids.json` |
| v1/v2 live bodies (2026-09-07) | `RE_Work/probes/reprobe_2026_09_07/*` (v1 update.json ghub12, v1/v2 details) |
| GCM reference vectors (authoritative, §11.27) | `RE_Work/probes/gcm-spec.pdf/.txt`, `RE_Work/probes/sp800-38d.pdf/.txt` |
| IDB (all renames/comments saved) | `C:\Program Files\LGHUB\lghub_updater.exe.i64` |

**STALE, to delete:** `RE_Work/probes/resp_pipeline_v2_update_ghub10_win_public_details.json`
(99,877 B — a truncated first fetch; the canonical 938,799 B body is the
`.json.json` variant).

---

## 11. What is verified

1. **Depot integrity:** `mac` = SHA-256(raw depot bytes) — 6 depots.
2. **Depot signature:** RSA-4096 PKCS#1 v1.5, DigestInfo SHA-256, over
   `sha256(32-byte mac digest)`; key = embedded "ghub" 4096-bit PEM
   (fp `01f43ddda220be2a`); verified by PKCS#1 unpadding + DigestInfo
   decomposition on a live-captured signature.
3. **Updater app identity:** all 1,333 live signatures are 512 B ⇒ app name
   `"ghub"` (4096 slot of the memcmp key table).
4. **Encrypted capsule decryption:** AES-128-GCM, key = base64 keymaster
   key (16 B) selected by `header.key-id`, IV = PBKDF2-HMAC-SHA512(SHA-hex
   string, key.name, 1000) → 32 B, per-chunk SHA chain
   (`header-sha` → `files[i-1].sha`), length-prefixed chunks, chunk0 =
   encrypted files list — all 9 chunks of the saved sample reproduce and
   hash-match.
5. **GCM is unauthenticated for capsule depots:** neither tag-last-16 nor
   tag-first-16 authenticates; every chunk decrypts correctly with no tag
   (`RE_Work/probes/decrypt_headersha.mjs`). Integrity comes from the SHA
   chain + depot `mac`, not GCM.
6. **Endpoint set + 403 behavior** for update/access paths (matrix above),
   including that access requires the group UUID and name-based paths 403.
7. **Keymaster discovery chain** manifest `keys.accessGroup` →
   `/pipeline/v2/access/{uuid}/content.json` — fully exercised.
8. **Manifest v1/v2 schema differences** (URL style, cipherSuite,
   signatures, keys, uuid).
9. **Three depot magics — all three layouts now closed:** `0x20170110`
   (plain, u32-length-prefixed chunks, PASS×5 offline), `0x20210506`
   (encrypted capsule, 9/9 chunks reproduced), `0x20210521` (individual-file
   capsule, `files-sha` variant; binary layout closed, no live sample yet).
10. **Plain-depot framing:** every plain depot is `[u32 magic][u32 len]
    [files-list JSON]` + one `[u32 LE len][content]` chunk per regular file,
    framing exactly to EOF on all 5 samples (`check_plain_framing.mjs`); plain
    entries carry no `sha`/`link` (§5.1).
11. **PBKDF2 recipe in the binary:** `DecryptedStream_ctor` (0x140272FB0)
    calls OpenSSL `PKCS5_PBKDF2_HMAC` (0x140ABA080) with `pass` = per-chunk
    SHA hex, `salt` = `Key.name`, `iter` = 0x3E8 (1000), `md` = `EVP_sha512`
    (md_size 64 @0x14104DAA0), `dkLen` = 32; AES key = standard-base64
    `Key+0x18` → 16 B (`base64_decode` 0x140C139C0); `Key.version` must be 1
    (§5.2).
12. **File records + extraction:** 104-byte record `{name@0, mode@32,
    link@40, sha@72}`; symlink = non-empty `link` with no data chunk
    (`extract_capsule_to_dir`/`extract_capsule_files`); stream order
    decrypt → decompress (xz) (§5.4, §8).
13. **v2 top-level manifest signature is NOT verified client-side**; only
    per-depot v1 signatures are (`depository_from_details_json` /
    `depot_signatures_from_json`) (§6.4).
14. **Depot v1 TBS = raw 32-byte mac digest** — reproduced offline
    (`tbs_digest_test.mjs`) and matched to the binary `+104` input
    (§6.3).
15. **Random-access depot reads ("resource_access")** — `CapsuleMetadata`
    (96 B: key ctx, transformation chain, `vector<FileRecord>`, offset/size
    trees, magic gate {`0x20170110`, `0x20210506`}) + entry points
    `capsule_open_file`/`capsule_verify_file`/`capsule_list_resources` and
    the worker/factory/ctor/underflow chain are fully mapped; per-file
    SHA-256 verify + symlink-target validation; fallback to full extract
    (`depot_extract_fallback`) on failure (§5.8).
16. **Random-access decryption password = record.sha** — the protected
    resource stream's `+0xC0` field is the record's sha string (not the
    display name) and underflow copies it into the 0xA0 protection context's
    password slot (`ctx+0x58`) before every in-place transform — identical to
    the extract path. One shared transform (`apply_capsule_protection_to_buffer`
    0x140240D40) on all read paths (§5.8).
17. **Plain depot header JSON carries the full file list**
    (`{"files":[{"name",…,"mode"},…]}`, no sizes) while `0x20210506` headers
    carry only `{"header-sha","key-id"}` (file list inside encrypted chunk 0);
    the size tree is always the physical `[u32 len]` chunk scan, size = chunk
    u32 length (verified on all 6 local samples via `dump_depot_headers.js`)
    (§5.8).
18. **H3 closed:** v2 relative depot URLs are served by the S3 origin
    (`http://2pipeline.s3.amazonaws.com/depots/…` 200 + mac-match). Infra
    correction (this round): `updates.ghub.logitechg.com` is **CloudFront
    with S3 origin** — manifest captures show both `server: AmazonS3` and
    `via: …cloudfront.net (CloudFront)`/`x-amz-cf-id`/`x-cache`; the earlier
    "same bucket via alias / not CloudFront" phrasing is **[DISPROVEN]**.
    The HTTP listener 403s the depot path (HTTPS works) (§2.2, §9).
19. **Request headers on all pipeline JSON requests** —
    `pipeline_build_install_headers` (0x14022BE90) sends exactly
    `logi-install-id` (machine id @pipeline_impl+880) + `logi-app-version`
    (Downloader vfunc6) on update.json/details.json/`/settings`; URL builders
    are 5-segment base + suffix with **no query parameters in the traced
    client builders**; the runtime host comes from settings/IPC — the
    hostname *is* a binary literal (element 0 of the 4-host array, §1.1)
    (§2.0.1, §2.0.2).
20. **H6 machine identifier chain** — type-1 id = 64-hex SHA-256(computer
    name UTF-16LE incl. NUL + C: volume serial LE32)
    (`canary_machine_id_derive_name_volume` 0x140A386E0); else C: volume
    serial as **decimal string** (e.g. `4199704679`;
    `SystemInfo::GetVolumeSerial` → 0x140A34940, transform semantics
    unmapped); persisted load-or-generate in logi SecureStorage container
    `HKLM\SOFTWARE\Logitech\LGHUB\Data\canary_machine_identifier` (396 B
    REG_BINARY) (§2.0.3).
21. **Server-side canary bucketing** — deterministic per
    `logi-install-id` value (~4/6 64-hex test ids → canary; absent/empty →
    public); canary/public for ghub10/win are the **same build**, differing
    only in `channel`+`lastModified` fields (not byte-identical objects)
    (§2.0.3, §9). This **supersedes** the earlier
    "stateless API, same manifest for everyone" reading.
22. **Version decision = plain string equality** (not semver);
    downgrade-capable (`pipeline_impl_has_new_build_to_download`
    0x1402185D0) (§2.0.3).
23. **FeatureCanary fed by `/settings`** — lambda_1 signature
    `void(const logi::protocol::updates::Settings&, int)`; two-level
    component→flag tree, no local bucketing; `/settings` 403 here ⇒ canary
    download state 3 (§2.0.4). Update-flow entry `_check_for_updates`
    (0x1402142E0) + `pipeline_impl` 0x3B0 layout confirmed (§2.0.5).
24. **Non-public channels exist (tim, staging) with opaque bodies** — 200 on
    both suffixes; bodies fail JSON parse and carry no known magics;
    structural facts: update.json is the leading section of details.json
    (205/207 B and 196/198 B common ciphertext prefixes), staging details =
    public + 1 B, tim details 129,198 B smaller (different manifest
    content). The earlier "staging recovered" claim is **[DISPROVEN]** —
    that file is a circular XOR of an assumption (§2.5). Bounded
    client-side key-material search (71 group keys × 6 modes, 1,426 derived
    candidates × 6 modes, PBKDF2-IV variants, ECB check over 58,675
    positions) = 0 hits; the binary has no manifest-decryption code path ⇒
    key material is not in this updater build; encoding + consumer
    **[UNRESOLVED]** (§2.5, §9, `RE/API_FINDINGS.md`).
25. **Depot key reference is by-name** — sample `key-id`
    `b895e0bb-0970-4eb5-a623-ab5abc2fddfd` ∈ live content.json (71 entries,
    16-B keys, version field must be 1 per binary check) (§4.1, §5.6).
26. **v1 API live and schema-diffed** — ghub12 v1 update.json (710935 /
    2026.2.861817); ghub10 v1 details 224,746 B: per-depot `cipherSuite:"none"`,
    absolute S3 URLs, no `uuid`/`signatures`/`keys` (§3.2, §9).
27. **GCM counter convention (reference)** — for 96-bit IV: `J0 = IV || 0³¹ ||
    1`, first keystream block `E(J0+1) = E(IV || 0x00000002)`; non-96-bit IV:
    `J0 = GHASH_H(IV || 0ˢ || [len(IV)]64)`; all-zeros test vector
    C `0388dace60b6a392f328c2b971b2fe78` / T
    `ab6e47d42cec13bdf53a67b21257bddf` — matches OpenSSL exactly. Source:
    NIST SP 800-38D + McGrew/Viega GCM spec Appendix B (local copies in
    `RE_Work/probes/`); the GF(2¹²⁸) multiply follows the NIST reference
    `gcm.c` (bit-reversed representation, R = 0x87 in last byte). Documented
    for the API format; depot decryption itself is out of project scope
    (closed 2026-09-07, user-directed).
28. **Machine-id type≠1 path** — `SystemInfo::GetVolumeSerial` **decimal
    string** of the C: volume serial (this machine: `4199704679`), not a
    hashed HDD serial (§2.0.3, corrected).

## 12. Active hypotheses

| # | Hypothesis | Status / test |
|---|---|---|
| H1 | Depot v1 TBS is the raw 32-byte mac digest (not hex) | **CONFIRMED (2026-09-07):** offline TBS reproduction (`tbs_digest_test.mjs`) + binary `+104` input buffer (§6.3, §11.14) |
| H2 | v2 manifest TBS is some canonicalization of `details.json` (e.g. protobuf-encoded, or a specific JSON serialization) | **Moot (2026-09-07):** the binary never verifies the top-level v2 signature — there is no client-side TBS to identify (§6.4, §11.13) |
| H3 | v2 relative depot URLs also resolve on `2pipeline.s3.amazonaws.com` | **CONFIRMED (2026-09-07):** S3 origin serves `/depots/{uuid}/{name}.depot` with 200 + mac-match. Infra correction: `updates.ghub.logitechg.com` is **CloudFront + S3 origin** (manifest captures show both header sets) — the earlier "S3 alias, not CloudFront" phrasing is **[DISPROVEN]** — §2.2, §9 |
| H4 | GCM is used without tag authentication (tag dropped), matching the C++ reference ignoring `Final` | **CONFIRMED for capsule depots** (2026-09-07): both tag layouts fail auth, all chunks decrypt tagless — see §5.2 |
| H5 | `0x20210521` depots appear only for newer builds | **Layout resolved in binary** (2026-09-07): `files-sha` single-file capsule, full flow in §5.3 — still no live sample captured |
| H6 | `canary_machine_identifier` gates canary-channel delivery per machine | **CONFIRMED (2026-09-07):** full chain mapped (generation → SecureStorage persistence → `logi-install-id` header) and bucketing proven live (deterministic per id value; ~4/6 64-hex ids → canary) — §2.0.3, §9, §11.20-21. Open detail: server hash/threshold, and byte-level repro of this machine's stored value (§14u) |
| H7 | Local install 2026.6.957899 is newer than served public 2025.9.814156 because the local machine is on a different channel (canary/enterprise) or the public channel was rolled back | **Largely closed (2026-09-07 matrix round):** the live `ghub13/win/public` channel serves buildId 824196 / **2026.5.939708** — exactly the local updater build and the depot id (824196) in the local 2026-08-08 log. The local machine is on app **ghub13** (the updater's factory-default app id, §2.0.5/§2.6), not ghub10/ghub12; the earlier "newer than every channel" observation was an app-identity artifact (wrong app compared). Residual: the captured `User-Agent: 2026.6.957899` origin is still untraced (findings B4) |
| H8 | Brute-forcing channel names reveals extra/hidden builds ("tim" suggests more names exist) | **ANSWERED (2026-09-07):** names are a free-form path segment (no client whitelist); 14 common names → 403, only `staging`/`tim`/`canary` exist beyond `public` for ghub10; the name selects a *manifest*, the depot set always comes from that manifest — there is no name→depot shortcut (§2.5) |
| H9 | Encrypted-channel (staging/tim) keys are client-derivable from visible data (group UUID, names, SSO ids, PBKDF2) | **DISPROVEN for the tested candidate set (2026-09-07):** 0 hits across all candidate families (a failed candidate set excludes only that material); the key material is not in this updater build; "server-side keying" is the working verdict but the actual consumer is unverified (§2.5, §11.24) |
| H10 | The updater verifies the v2 top-level manifest signature somewhere at runtime (e.g. in the software manager) | **Open (this build only):** `lghub_updater.exe` does not (§11.13); the software manager binary is the likely consumer of encrypted channels and of `signatures` — not yet analyzed in IDB (deferred) |

## 13. Disproven / corrected claims

| Claim (earlier) | Correction |
|---|---|
| "All depots are public / the API is fully open" | Only **sampled** depots were fetched (60 prefixes + 6 full); the 403s prove access-policy restrictions exist (name-based access, settings) |
| "keymaster keys are feature keys, not per-depot keys" | Wrong — the header `key-id` matches a keymaster key 1:1 and it decrypts the depot (§5.6) |
| "The client does not verify depot signatures" | Contradicted: full RSA verify path exists and is wired into depot validation (§6.3) |
| "Key A (2048, fp b97bd090…) is the updater's key" | The updater's key is the 4096-bit ghub key (fp `01f43ddda220be2a`); all live sigs are 512 B |
| "EVP_MD descriptor +8 word is the digest size" | Unconfirmed; NID words (672/668) are certain, trailing 64/120 mapping is open (§14m) |
| Depots verified as raw-bytes RSA (single SHA-256) | Fails for every key — the scheme is the double hash over the mac digest (§6.1) |
| "Plain depots (0x20170110) have no per-file length prefixes" | **Wrong** — every chunk in every format is `[u32 LE len][data]`; `check_plain_framing.mjs` frames all 5 plain depots exactly to EOF (19,392 / 632 / 192,267 / 97,930 / 21 B) (§5.1) |
| "The `header-sha` literal is absent from the binary" | **Wrong** — present @0x140F46778; the field name is a runtime parameter passed to `parse_capsule_header` (xrefs 0x14020b331, 0x14023e986) (§5.2) |
| "IV seed comes from the manifest depot entry (iv/key fields)" | **Wrong** — live v2 depot entries carry only name/size/url/mac/signatures (no iv/key/cipherSuite); the IV seed is the per-chunk expected-plaintext SHA hex string (§5.2, `check_iv_source.mjs`) |
| "type≠1 machine id = hashed HDD serial (`systeminfo_get_machine_serial`)" | **Wrong (2026-09-07)** — the path is `SystemInfo::GetVolumeSerial` → **decimal string** of the C: volume serial (`4199704679` here); no hash (§2.0.3) |
| "staging details.json recovered byte-for-byte (= public manifest with `channel:"staging"`)" | **[DISPROVEN — synthetic artifact]** `staging_details_RECOVERED.json` is a circular XOR of an assumed plaintext (K = ct ⊕ A ⇒ ct ⊕ K = A); it proves nothing about staging's content and is excluded from the evidence (§2.5). The body stays opaque |
| "tim details is a distinct build" (as fact) | Downgraded to **inference**: tim details (809,601 B) is 129,198 B smaller than public ⇒ different manifest content; the exact build is unobservable while the body is opaque (§2.5) |
| "`updates.ghub.logitechg.com` is an S3 alias of the `2pipeline` bucket, not CloudFront" | **Wrong (2026-09-07)** — captured responses carry both `server: AmazonS3` and CloudFront headers (`via: …cloudfront.net (CloudFront)`, `x-amz-cf-id`, `x-cache`): CloudFront distribution with S3 origin (§2.2, §11.18) |
| "`updates.ghub.logitechg.com` is absent from the binary" | **Wrong (2026-09-07)** — the hostname is a literal (string-table line 22787; element 0 of the 4-host array @0x1413D8B40). The *runtime* host comes from settings/IPC; the array is a static candidate set (§2.0.1, §1.1) |
| "tim/staging bodies are AES-GCM ciphertext" | Downgraded to **opaque**: no mode/cipher identification from the observations (shared prefix, sizes, entropy); documented as undetermined encoding (§2.5) |
| "no query parameters anywhere" (absolute) | Scoped: no query parameters in the **traced client URL builders**; server-side acceptance of query strings is untested (matrix `RE/API_VARIANT_MATRIX.md`) |
| "`pipeline_impl` +912..+944 = channel" | **Wrong (2026-09-07)** — +912 is the **app id** (`ghub10`); the channel is carried in the urlctx/config (§2.0.5) |
| "GCM first keystream block for a 12-byte IV is `E(IV ‖ 0²⁴ ‖ 1)`" | **Wrong** — per SP 800-38D, `J0 = IV ‖ 0³¹ ‖ 1`, so the first block is `E(IV ‖ 0x00000002)`; memorized non-spec test vectors were also wrong — always verify against the fetched spec text or a reference implementation (§11.27) |
| "staging/tim manifest keys derivable client-side (group keys, SSO ids, PBKDF2 variants)" | **Disproven for the tested candidate set (2026-09-07)** — 0 hits over all candidate families (excludes only that material); key material absent from this updater build; consumer unverified (§2.5) |
| "The protected resource stream's `+0xC0` field is the file's display name" | **Wrong (2026-09-07)** — `+0xC0` holds the record's **sha string**, copied underflow-time into the protection context's PBKDF2 password slot `ctx+0x58`; the display name is never used for decryption (§5.8, §11.16) |
| "The API is stateless — same `details.json`/`update.json` for everyone" | **Wrong (2026-09-07, H6)** — the server buckets by the `logi-install-id` request header: same build but `channel` flips public↔canary (and `lastModified` changes) per id value; deterministic per id. Header-absent requests default to public (§2.0.3, §9, §11.21) |
| "`sub_1400DC1F0` is a 16-byte string hash used to build the header map" | **Wrong (2026-09-07)** — it is `std::vector<uint64_t>::resize(n, fill)`; the map node's `+24` vector is filled with node pointers, not a digest. No hashing in the header path (§2.0.2) |

## 14. Unresolved questions

a. **GCM tag** — **resolved for `0x20210506` capsules (2026-09-07): no
   verifiable tag in last-16 or first-16 layout; decryption is
   unauthenticated, integrity via SHA chain + `mac`** (§5.2). Whether a tag
   appears in `0x20210521` single-file depots remains open.
b. **v2 manifest signature TBS** — **moot (2026-09-07):** the client never
   verifies the top-level v2 signature, so no client-side TBS exists
   (§6.4, §11.13).
c. **`settings.settings`** — schema, auth, or dead.
d. **verify_depot update input** — **resolved (2026-09-07):** the TBS is the raw 32-byte mac digest, reproduced offline (`tbs_digest_test.mjs`) and matched to the binary `+104` input (§6.3, §11.14).
e. **Second `header-sha` caller `sub_14023E920`** — xref 0x14023e986 of the
   `header-sha` literal @0x140F46778 — **resolved (2026-09-07):** this is
   `capsule_metadata_parse_header` (0x14023E920), the header-parsing step of
   `capsule_metadata_create` on the random-access path; not a separate
   capsule-open path (§5.8).
f. **Compression** — **resolved (2026-09-07): xz only** (type code 2, literal `xz`), optional per depot via the header `compression` field; stream order decrypt → decompress (`build_capsule_stream_chain` 0x140240B40, `DecompressedStream_ctor` 0x140272C00) (§8).
g. **xdelta differential depot** layout.
h. **Depot variants** beyond the 3 magics (if any).
i. **Local 2026.6.957899 > live 2025.9.814156** — **partially answered
   (2026-09-07):** local binaries (updater 2026.5.939708, software manager
   2026.5.9708.0 per `lghub_08_08_2026.log`) are newer than every observed
   live channel (ghub10 2025.9.814156 incl. canary; ghub12 2026.2.861817);
   canary==public content on ghub10 rules out a pure channel explanation.
   Open: where does the `2026.6.957899` UA string come from, and which
   appId/channel does this machine actually poll (§14x)?
j. **`/scarif/keyswap`** semantics.
k. **CN/staging hosts** behavior.
l. **Two unexplained embedded base64 blobs** (54 B / 48 B) in the binary.
m. **EVP_MD trailing words 64/120** meaning.
n. **Runtime value of the 24-byte global** @0x1413D8B28 written indirectly
   (empirically the app name "ghub"; the setter is register-based and not
   xref-visible).
o. **HTTP caching** — **resolved for 200s (2026-09-07 re-probe)**: strong
   ETag + Last-Modified, no Cache-Control (§9). Range/resume behavior still
   untested.
p. **`iat.json`** purpose.
q. **Plaintext-depot file framing** — **resolved (2026-09-07):** one `[u32 LE len][data]` chunk per regular file, in `files[]` order, framing exactly to EOF on all 5 samples (`check_plain_framing.mjs`); sizes come from those per-chunk prefixes, not the manifest (§5.1).
r. **streambuf vtable tail** — every streambuf vtable in the binary ends
   (after the class seek pair, slots 10/11) with a shared 3-qword tail
   `0x140DFF7FC/0x140DFF7F6/0x140DFF7F0` (mid-instruction addresses inside
   `is_potentially_valid_image_base`, never called). Role unverified —
   probably a compiler-emitted RTTI tail, not virtual functions (§5.8).
s. **Encrypted depot whose xz plaintext is larger than its chunk** — size
   tree stores the chunk u32 length (ciphertext length; == plaintext size
   when plain); the resource buffer capacity is that `size`, so an expanded
   plaintext larger than the chunk would hit `capsule_buffer_set_commit`'s
   over-capacity path (`sub_14017FDC0`, unmapped). No such sample observed;
   untested.
t. **Post-factory result chain** — `sub_14023F0D0`/`sub_14023F020`
   (post-factory check), `sub_14023ED10` (metadata query), `sub_14026F6C0`
   (41-B result assembly) and the buffer over-capacity handler
   `sub_14017FDC0` remain unmapped (§5.8).
u. **Machine-id byte reproduction / SecureStorage container format** —
    type-1 formula re-computed from this machine's current inputs
    (name `DESKTOP-IFDD7ML` + C: vol serial `0xFA6C8AA7`) does **not** match
    the stored 64-byte value (blob tail @0x14C; nor SHA-512 nor name/vol
    variants — `RE_Work/probes/h6_variant_test.mjs`, `h6_variant2.mjs`).
    The type≠1 path is now identified (decimal C: volume-serial string,
    §2.0.3), but `4199704679` does not appear in the blob either — so the
    container value is transformed/encrypted by SecureStorage, or the id
    predates a rename/reformat (stale inputs). **[2026-09-07 corrected
    container layout]** (`h6_blob_full.txt` + `hexdump_blob.cjs`):
    `[u32 1] [GUID {DF9D8CD0-1501-11D1-8C7A-00C04FC297EB}] [u32 1]
    [GUID {36762ECC-DA28-4D3B-A4C9-8297692B1973}] [u32 4] [u32 40]
    [40 B UTF-16 "Logi Secure Storage"+pad @0x30] [u32 0x6610 @0x58 ?]
    [u32 1] [u32 32] [32 B blob @0x64 `2ede98ed…`] [u32 0] [u32 0x80E]
    [u32 0x200] [u32 32] [32 B = two binary GUIDs @0x94]
    [128 B @0xC4 (8×16 B blocks)] [u32 64 @0x148] [64 B value @0x14C
    `24739d2e…`] — 396 B total; GUID and intermediate field roles unknown.
v. **Server-side bucket algorithm** — which hash/threshold over the id
    string (deterministic, ~4/6 64-hex sample → canary; non-64-hex or
    malformed → public; absent/empty → public).
w. **`logi-app-version` effect** — all probes used the same value; whether
    the server keys anything (compatibility gating, A/B) off it is unknown.
x. **`User-Agent: 2026.6.957899` origin** — raw literal
    `User-Agent: GHubDownloader/1.0\r\n` exists at 0x140F9F080 (code path
    untraced); the UA used by our depot fetches is a probe-side string, and
    the 2026.6.957899 build number's source (a newer canary build? an
    enterprise channel?) is unresolved.
y. **86400 s poll interval purpose** — `pipeline_impl_builder_build`
    default; how it maps to scheduler behavior (and whether `/settings` or
    config can change it) is untraced.
z. **`tim` channel content** — distinct build, 809,601 B ciphertext; the
    plaintext is recoverable in principle only with more known plaintext
    (decryption is out of project scope — API reconstruction only).
aa. **Who consumes the encrypted channels?** — the software manager
    (separate binary) is the prime candidate; it has not been opened in
    IDB yet (deferred). Also: who consumes the v2 top-level `signatures`
    (§6.4, H10)?
ab. **Per-depot storage-namespace UUIDs** (6× core_* + release_notes in
    the v2 manifest) — server-side bucket layout only; no client-side
    reference observed (cosmetic for the client: URLs are opaque).
ac. **`iat.json` semantics** — still open (item p); the updater GETs it in
    the keymaster flow (§18.3) as a change-check, but its server-side
    meaning (invalidate-at?) is undocumented.

## 15. Exact reproduction

From the repository root, no network needed for 1–12:

```powershell
# 1. Offline crypto baseline (5 plaintext depots + encrypted capsule):
node RE_Work/review/verify-saved-sample.mjs
#    → PASS: five plaintext depot sizes and raw SHA-256 values
#    → PASS: encrypted depot size/hash, header, and all eight file SHA-256 values
#    → LIMIT: saved-sample consistency only; GCM tags and manifest signatures not verified

# 2. Depot + manifest signature forensics (proves §6.1/§6.4):
node RE_Work/tools/rsa_forensics.js

# 3. Documented negative: raw-bytes verification with all 3 embedded keys:
node RE_Work/tools/verify_rsa_sigs.js

# 4. Extract the 3 embedded PEMs from the PE (needs C:\Program Files\LGHUB\lghub_updater.exe):
node RE_Work/tools/extract_pem_keys.js

# 5. Schema samples from saved manifests:
node RE_Work/tools/dump_schemas.js
# 6. Depot header dump:
node RE_Work/tools/dump_depot_headers.js
# 7. Decrypted files-list of the saved capsule (uses the verified recipe):
node RE_Work/tools/dump_inner_files.js
# 8. GCM tag-layout experiment (proves §5.2 / §11.5: no tag in stream):
node RE_Work/probes/decrypt_headersha.mjs
#    → tag-last16: auth failed; tag-first16: auth failed;
#      all 9 chunks sha-match unauthenticated
# 9. TBS = raw 32-byte mac digest (proves §6.3 / §11.14; bun or node):
bun RE_Work/tools/tbs_digest_test.mjs
# 10. header-sha hashes chunk-0 plaintext, not the header JSON bytes (§5.2):
node RE_Work/probes/check_headersha.mjs
# 11. live v2 depot entries carry no iv/key/cipherSuite (§5.2):
node RE_Work/probes/check_iv_source.mjs
# 12. plain-depot framing = u32 chunks, exact EOF, 5/5 depots (§5.1 / §11.10):
node RE_Work/probes/check_plain_framing.mjs
```

Live H3 probe (2 depot GETs, one per host; new OutDir; §2.2 / §9):
```powershell
# 13. v2 depot on S3 origin (HTTP) + CloudFront alias (HTTPS) — proves §11.18:
& "RE_Work\probes\probe_h3_s3.ps1" -OutDir "RE_Work\probes\reprobe_2026_09_07_h3"
node RE_Work/probes/probe_h3_cf_https.mjs
#    → s3_origin 200 632 B mac-match=True ; cloudfront HTTP 403 ; HTTPS 200 server: AmazonS3

# 14. LIVE H6: identity headers flip the served channel (§2.0.3, §11.19-22;
#     sequential requests, 400-800 ms apart; new OutDir per run):
node RE_Work/probes/probe_h6_headers.mjs   # details.json ± logi-install-id
node RE_Work/probes/probe_h6_ids.mjs       # 7 ids → channel table
node RE_Work/probes/probe_h6_rest.mjs      # update.json bucketing + /settings 403
# 15. LIVE machine-id derivation cross-check (offline recompute vs stored
#     HKLM SecureStorage blob — expected to MISMATCH on this machine, §14u):
node RE_Work/probes/h6_variant_test.mjs
node RE_Work/probes/h6_variant2.mjs
```

Live re-probe (only if needed; keep it small): see §17.

## 16. Tooling bugs found and fixed

1. **`RE_Work/probes/probe_http.ps1`** (bug + fix, 2026-09-07) — the result
   row `[PSCustomObject]@{…}` lacked `etag`, `lastModified`,
   `cacheControl`, so under `$ErrorActionPreference = 'Stop'` every
   successful 200 threw while populating `etag` and was logged as an error;
   `probe_summary.json` was therefore untrustworthy (8 false errors).
   **Fixed:** all row fields are now initialized at creation; network-level
   failures go to a separate `error` field and no longer overwrite the HTTP
   status; `error` is surfaced in console output. `probe_summary.json` must
   be rebuilt by one live round with the fixed script (same 17 paths,
   §17 etiquette) before use.
2. **`RE_Work/probes/decrypt_headersha.mjs`** (bug + fix, 2026-09-07) —
   (a) the salt loop `for (const [sn, salt] of salts)` destructured each
   salt *string* into characters, collapsing the PBKDF2 salt to one byte;
   (b) WebCrypto `subtle.decrypt(algorithm, key, data)` is three-argument
   and ignores any fourth "tag" argument, so the tag experiments were void —
   the rewrite follows the WebCrypto convention (tag = final 16 bytes of
   `data`) and reassembles the buffer for tag-first layouts; (c) it reused
   the chunk-0 IV for every file — the rewrite derives each file's IV from
   that file's own SHA (`files[i].sha`), as the C++ implementation does.
   Result: see §5.2 (no tag in stream; unauthenticated decryption).
   The working offline baseline remains
   `RE_Work/review/verify-saved-sample.mjs` (node:crypto).
3. **Stale fixture (deleted 2026-09-07)** —
   `RE_Work/probes/resp_pipeline_v2_update_ghub10_win_public_details.json`
   (99,877 B) was a truncated duplicate of the canonical 938,799 B
   `..._details.json.json` body.

## 17. Live probing etiquette (used 2026-09-06, keep for future rounds)

- Tool: `C:\Users\Decode\.bun\bin\bun.exe` (or any JS runtime) — the sandbox
  PowerShell TLS stack is broken; CloudFront serves HTTP, so plain
  `http://updates.ghub.logitechg.com/...` also works.
- Sequential requests only, 400–800 ms delay between them, a few dozen
  requests max per round, `User-Agent: LGHUB/<version> (Windows NT 10.0; x64)`
  for depot objects, default UA for JSON.
- Always save bodies to `RE_Work/probes/` with a descriptive name before
  analyzing; never analyze from memory.
- Do not probe CN/staging hosts without explicit user approval (unknown
  egress/monitoring implications).

---

## 18. Client-side update flow (binary-confirmed, 2026-09-07)

### 18.1 Update check state machine

`pipeline_impl` (0x3B0 B, §2.0.5) drives the check; state word @+456,
transition helper `sub_140221F30`:

| State | Meaning (observed) |
|---|---|
| 2 | IDLE (settled; "already installed. No new keys either") |
| 3 | CHECKING (update.json in flight) — set when state bit 8 requests a check |
| 12 | (transient) → 2 on `_check_for_updates` entry |
| 13 | OS too old — min-OS string compare vs config+216 |

States 4–11/14+ exist in the enum but were not observed on this path.

### 18.2 Request sequence (all URLs from §2.0.1, all JSON GETs)

```
1. _check_for_updates (0x1402142E0)
     guard a1+368 (updates disabled) → return
     state 12 → 2 ; min-OS too old → state 13
     bit8 set → state 3 (CHECKING)
     GET {server}/pipeline/v2/update/{app}/win/{channel}/update.json
       job factory sub_1402594E0 (group @a1+72, url, installed buildId,
       callback, headers from §2.0.2; optional extra headers via
       Downloader vfunc 21/44)
2. pipeline_impl_on_update_check_result (0x140228430)
     parse update.json (buildId, version, …)
     a1+912 = app id string ("ghub10")
     if next build != installed build:
        GET …/details.json  (job factory sub_14025AFC0;
          urlctx {server@+40, channel@+48} via Downloader vfunc 2;
          group @a1+88; headers §2.0.2)
        → §18.3 keymaster flow + depot downloads
     else:
        log "already installed. No new keys either" → state 2 (IDLE)
```

Version comparison = **plain string equality** (no semver; downgrades
possible — §11.22, `pipeline_impl_has_new_build_to_download` 0x1402185D0).
Depot downloads use the conan package **logi_downloader 3.2.0**
(RTTI `logi::downloader::DownloaderImpl` / `DownloadJob` /
`CurlDownloadQuery` / `WinApiDownloadQuery`); per-depot RSA verification
per §6.3 before install.

### 18.3 Keymaster (access-group key) flow [BINARY]

Triggered after details.json arrives:

```
details.json  keys.accessGroup = "<uuid>"
  long_json_download_job::get_details (0x140265360)
    missing/empty → error "Build access group ID is missing or empty!"
                   (codes 702/703)
  keymaster_content_job (ctor 0x140251FA0,
    onKeymasterComplete 0x140F4E190)
    GET /pipeline/v2/access/{uuid}/content.json   (200, §4.1)
    → cache as keys.json (storage fns 0x140231B40 / 0x140233C30 /
      0x140235850)  — absent on a clean machine until first success
    GET /pipeline/v2/access/{uuid}/iat.json       (build_iat_json_url
      0x140269DC0; body {"lastModified": …}) — change-check against the
      cached content
  → notify_protection_keys_used
```

Depot decryption (§5.2) then resolves `header.key-id` against the cached
key table by **name** (§4.1, §5.6). Note: the updater never verifies the
v2 top-level manifest signature (§6.4, §11.13).

## 19. Factory config and identity constants [BINARY]

- Config factory `0x140E02460` builds a 0x250-byte config object with
  defaults: **app id `ghub13`**, **channel `public`**, **min OS
  `10.0.17763`**, **signature key table id `ghub`**. This machine runs
  app id `ghub10` (config/IPC override).
- `pipeline_storage_get_valid_channel` (0x1402005A0): **no client-side
  channel whitelist** — whatever string is configured is used in the URL
  (§2.5 channel census is purely a server-side property).
- Server-app-id allowlist: memcmp table @0x140E01248 (app ids, not hosts).
  The literal `updates.ghub.logitechg.com` **is** in the binary (string
  line 22787; element 0 of the 4-host array @0x1413D8B40, §2.0.1/§11.19) —
  an earlier note claiming its absence was corrected 2026-09-07.
- **LID client UUID** `b7d2e981-0ca6-4806-91e6-0cbb3bdb94d6` = KV key
  `lidClient` (getter 0x140E05780, setter 0x140E03910, writer
  0x140E03D10; config table 0x1410E4F68–0x1410E4FA0) — identity token
  client id, not a pipeline parameter.

## 20. SSO client ids (software manager binary) [BINARY]

Recovered from the software-manager PE (not the updater), file offsets
`0xC586CC`–`0xC587BC`, stride 0x30 (`RE_Work/probes/
reprobe_access_keys_2026-09-07/sso_client_ids.json`):

| Region | Client id (32-hex) |
|---|---|
| CN ×3 | `e5fa04de24153837ce00d64e133a3be2`, `1ad7bf2fb3fa4f2010f9ad9491977cfb`, `b811e77fe061c9daeb13245391c4d9ae` |
| global ×3 | `170d2d1f9153bf5117acbf2dd932c6d4`, `98120664d3c63d9e70fdac56826d95a5`, `c266f32f0fd8d0de16dafc65257bb2b4` |

These are OAuth-style client identifiers for Logitech's SSO endpoints;
they were tested as encrypted-channel key material (§2.5) with 0 hits —
consistent with their purpose being login, not update-channel crypto.

---

*Method note:* conclusions above were derived exclusively from (a) saved
live HTTP fixtures, (b) offline cryptographic verification against those
fixtures, and (c) static analysis of `lghub_updater.exe` in IDA (session
`lghub3`, IDB saved with renames/comments). No claim in §11 rests on
assumption. Base conversions in this document were produced with the
`int_convert` MCP tool.
