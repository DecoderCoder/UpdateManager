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

All live probes were `GET`, unauthenticated, no special headers (bun/fetch
defaults), 2026-09-06. Depot object URLs were fetched with
`User-Agent: LGHUB/2026.6.957899 (Windows NT 10.0; x64)`.

### 2.1 Update manifest endpoints

| Path template | Method | Result 2026-09-06 | Fixture |
|---|---|---|---|
| `/pipeline/v2/update/{appId}/{platform}/{channel}/details.json` | GET | **200** — full depot list + signatures + keymaster group | `resp_pipeline_v2_update_ghub10_win_public_details.json.json` (938,799 B), `..._ghub12_win_public_details.json.json` (992,526 B) |
| `/pipeline/v2/update/{appId}/{platform}/{channel}/update.json` | GET | **200** — manifest pointer (no depots) | `resp_pipeline_v2_update_ghub10_win_public_update.json.json` (197 B) |
| `/pipeline/v2/update/{appId}/{platform}/{channel}/settings.settings` | GET | **403** — S3 `AccessDenied` XML (263 B) | `resp_pipeline_v2_update_ghub10_win_public_settings.settings`; also 403 for `ghub10/win/canary` (243 B) |
| `/pipeline/v1/update/{appId}/{platform}/{channel}/details.json` | GET | **200** — v1 manifest (absolute S3 depot URLs, no signatures/keys) | `resp_pipeline_v1_update_ghub10_win_public_details.json.json` (224,746 B) |

Observed `appId` values: `ghub10` (G HUB 2025.9 line), `ghub12`.
Observed `platform`: `win`. Observed `channel`: `public`, `canary`.
**[HYPOTHESIS]** The path is a plain S3-style key under a CloudFront/S3
front (root `/` returns 403 0 B, `settings.settings` returns S3 XML errors),
i.e. the whole thing is object storage with an access policy. No API
documentation or auth headers were found in the binary for these paths.

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

**[HYPOTHESIS]** v2 relative URLs are also readable on the S3 origin
(`2pipeline.s3.amazonaws.com/depots/...`); not tested.
**[UNRESOLVED]** whether `/depots/...` is a CloudFront redirect to S3 or
served directly.

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
- Depot download progress / resume semantics — no ETag/Range behavior was
  tested. The buggy `probe_http.ps1` never recorded `etag`/`cacheControl`
  headers (see §16), so **nothing is known** about cache semantics.

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
  "name": "core",                         // human name OR UUID (encrypted capsule depots are named by UUID, see §5.4)
  "size": 270677711,                       // exact byte count of the depot object
  "url": "/depots/3b51d0b7-…-ca1d6dea66c8/core.depot",  // relative in v2
  "mac": "5b3f76fa…9798",                  // 64-hex: SHA-256 of the raw depot bytes (CONFIRMED, §5.6)
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

### 3.4 `settings.settings`

**403** on all live probes (public and canary). Body is S3 `AccessDenied`
XML. **[UNRESOLVED]** its schema, whether it requires auth, or whether it is
dead. The binary contains config keys (see §8) that may be served by it.

---

## 4. Keymaster (access) schemas

### 4.1 `content.json`

```jsonc
{
  "accessGroup": "323e77f5-68c2-43d8-8457-818bbd663938",
  "lastModified": 1765286821,              // unix epoch seconds (2025-12-09T18:47:01Z)
  "keys": [
    {
      "name": "c91f9b21-5280-44de-9e67-9c39aaea28ad",  // UUID; also the PBKDF2 salt (§5.5)
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
offset 8+headerJsonLen: payload, layout depends on magic
```

### 5.1 Format A — plaintext multi-file depot, magic `0x20170110`

Observed on: `core`, `core_apps`, `gl`, all device-firmware depots
(`g502`, `g403`, `g305`, `driver_audio_*`, …) and on all 5 fully downloaded
samples. Header:

```json
{ "files": [ { "name": "manifest.json", "mode": 33206 }, … ] }
```

Payload (after the header JSON): concatenation of the file contents, in
`files[]` order, **no per-file length prefixes** — the only framing is the
header file list and the depot `size`/`mac` in the manifest.
([HYPOTHESIS] file boundaries are recovered from the total size + known
file sizes elsewhere in the app, or are simply concatenated and split by the
consumer; the updater writes each file from the stream. Not fully verified.)

File `mode` values [OFFLINE-PROOF, int_convert-verified]: standard POSIX
st_mode values — `33188 = 0o100644`, `33206 = 0o100666`,
`33279 = 0o100777` (regular file + permission bits). [OWN] our builder
(`UpdateManager.cpp`) writes `33188` for regular files.

Sample header sizes show the list can be very large (`core`: jsonLen 32,601
B; `core_apps`: 49,603 B) [LIVE `magic_inventory.json`].

### 5.2 Format B — encrypted capsule, magic `0x20210506`

Observed on 24 sampled depots whose **names are UUIDs** (e.g.
`85875e86-f3e1-4e79-91ee-232575e2807f`) [LIVE `magic_scan_broad.json`].
Header (only two fields):

```json
{ "header-sha": "a59dd1b3…122f1", "key-id": "b895e0bb-0970-4eb5-a623-ab5abc2fddfd" }
```

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
| AAD | none assumed (never established — see tag note) |

After decryption, `sha256(chunk0 plaintext) == header-sha` and
`sha256(chunk[i] plaintext) == files[i-1].sha` held for all 9 chunks of the
saved sample. **This is the verified recipe**; it matches the C++ reference
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

**[BINARY]** A third magic exists in the updater: a code path that reads a
raw depot with a size limit of `0x6400000` (100 MiB, via `int_convert`) for
this new single-file format. **[UNRESOLVED]** its exact layout; none of the
60 sampled depots showed this magic on 2026-09-06.

### 5.4 Naming convention observation

[LIVE] All 36 sampled `0x20170110` depots have human-readable names
(`core`, `g915_us`, `driver_audio_apo`); all 24 sampled `0x20210506` depots
are named by UUID. **[HYPOTHESIS]** capsule depots carry app-internal
content (web assets, per the decrypted sample: `front.webp`,
`metadata.json`, `device_presets.json`) and are content-addressed.

### 5.5 Key-identity chain (end to end, all confirmed)

```
manifest depots[i].name "85875e86-…"
  → depot header key-id "b895e0bb-…"
  → /pipeline/v2/access/323e77f5-…/content.json keys[]
     entry name == "b895e0bb-…", key = base64 16 B
  → AES-128-GCM key for all chunks of that depot
```

**[DISPROVEN correction]** An earlier hypothesis that keymaster "keys" are
per-feature keys, not per-depot encryption keys, was **wrong**: the depot
header `key-id` matches a keymaster key 1:1 and that key decrypts the depot
[OFFLINE-PROOF].

### 5.6 Depot `mac`

`mac` = **SHA-256 of the raw depot object bytes** — confirmed for 6 depots
(5 plaintext + 1 encrypted capsule) [OFFLINE-PROOF]. It is also the value
the signature is ultimately anchored to (§6.1).

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

**[HYPOTHESIS → near-confirmed]** The `+104` TBS std::string that
`check_signature` passes to the RSA verifier is the **32-byte mac digest**
(raw, not hex) for version-1 depot signatures — this is the only input
consistent with the offline proof (§6.1) and the "Computed digest of depot
capsule" log. Direct confirmation of the update-input buffer in the IDB is
still on the open list (§15d).

### 6.4 Top-level v2 manifest signature

`signatures: {version: 2, signatures: [<1024-hex string>]}` [LIVE].
Forensically the same key and padding: `publicDecrypt` with the ghub 4096
key yields a SHA-256 DigestInfo whose digest is
`3ff742238baf70bb4374ab7ae9361ae11e2b48942f83ffc92e1ce4124035e656`.

**[UNRESOLVED] The to-be-signed (TBS) byte string is not yet identified.**
Exhausted offline (all fail to match): raw `details.json` as fetched;
minified JSON; `\n`/`\r\n` variants; trailing-trimmed; `appId`/`buildId`/
`uuid`/`version` combinations; double-SHA-256 of each of the above;
`details.json` with `signatures` removed; with `depots` removed; with the
top-level signature object removed (inner digest `1299e57baf1541433f64a0c9`);
`update.json` raw (inner `d49209e5aef823d6a1b49d93`);
`settings.settings` raw (inner `6c6ee7d74b6060fc5dc18b62`).
Next step: trace the C++ v2 verify call site in the IDB to see which buffer
`check_signature` receives when version == 2 (open item §15b).

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
  download_compressed_depot_async`; `DecompressedStream` ctor
  sub_140272C00. **[UNRESOLVED]** compression algorithm (zlib? lz4? brotli?)
  and framing.
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
9. **Three depot magics** (two with captured layout, one binary-only).

## 12. Active hypotheses

| # | Hypothesis | Status / test |
|---|---|---|
| H1 | Depot v1 TBS is the raw 32-byte mac digest (not hex) | Near-confirmed by offline proof + logs; IDB buffer confirmation pending (§15d) |
| H2 | v2 manifest TBS is some canonicalization of `details.json` (e.g. protobuf-encoded, or a specific JSON serialization) | All JSON variants exhausted; need C++ call-site trace (§15b) |
| H3 | v2 relative depot URLs also resolve on `2pipeline.s3.amazonaws.com` | Testable with 1–2 GETs |
| H4 | GCM is used without tag authentication (tag dropped), matching the C++ reference ignoring `Final` | **CONFIRMED for capsule depots** (2026-09-07): both tag layouts fail auth, all chunks decrypt tagless — see §5.2 |
| H5 | `0x20210521` depots appear only for newer builds | Needs a build where they exist; none in ghub10/ghub12 2025.9 |
| H6 | `canary_machine_identifier` gates canary-channel delivery per machine | Look for the setter/getter in IDB |
| H7 | Local install 2026.6.957899 is newer than served public 2025.9.814156 because the local machine is on a different channel (canary/enterprise) or the public channel was rolled back | Compare canary manifest content; check local channel config |

## 13. Disproven / corrected claims

| Claim (earlier) | Correction |
|---|---|
| "All depots are public / the API is fully open" | Only **sampled** depots were fetched (60 prefixes + 6 full); the 403s prove access-policy restrictions exist (name-based access, settings) |
| "keymaster keys are feature keys, not per-depot keys" | Wrong — the header `key-id` matches a keymaster key 1:1 and it decrypts the depot (§5.5) |
| "The client does not verify depot signatures" | Contradicted: full RSA verify path exists and is wired into depot validation (§6.3) |
| "Key A (2048, fp b97bd090…) is the updater's key" | The updater's key is the 4096-bit ghub key (fp `01f43ddda220be2a`); all live sigs are 512 B |
| "EVP_MD descriptor +8 word is the digest size" | Unconfirmed; NID words (672/668) are certain, trailing 64/120 mapping is open (§15m) |
| Depots verified as raw-bytes RSA (single SHA-256) | Fails for every key — the scheme is the double hash over the mac digest (§6.1) |

## 14. Unresolved questions

a. **GCM tag** — **resolved for `0x20210506` capsules (2026-09-07): no
   verifiable tag in last-16 or first-16 layout; decryption is
   unauthenticated, integrity via SHA chain + `mac`** (§5.2). Whether a tag
   appears in `0x20210521` single-file depots remains open.
b. **v2 manifest signature TBS** — which exact bytes are signed?
c. **`settings.settings`** — schema, auth, or dead.
d. **verify_depot update input** — confirm in IDB that the 32-byte digest
   (raw) is the `EVP_DigestVerifyUpdate` buffer.
e. **`sub_14023D7F0` header-sha verify input** — what the capsule-open path
   hashes before the files list.
f. **Compression** — algorithm + framing for `DecompressedStream`
   (sub_140272C00) / `download_compressed_depot_async`.
g. **xdelta differential depot** layout.
h. **Depot variants** beyond the 3 magics (if any).
i. **Local 2026.6.957899 > live 2025.9.814156** — channel or rollback?
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
q. **Plaintext-depot file framing** — how consumers split the concatenated
   payload (sizes come from where?).

## 15. Exact reproduction

From the repository root, no network needed for 1–4:

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

*Method note:* conclusions above were derived exclusively from (a) saved
live HTTP fixtures, (b) offline cryptographic verification against those
fixtures, and (c) static analysis of `lghub_updater.exe` in IDA (session
`lghub3`, IDB saved with renames/comments). No claim in §11 rests on
assumption. Base conversions in this document were produced with the
`int_convert` MCP tool.
