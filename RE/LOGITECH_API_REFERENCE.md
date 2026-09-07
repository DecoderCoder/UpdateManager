# Logitech G HUB Update Pipeline — Analysis

This project's reverse-engineering deliverables live in [`../RE_Work/`](../RE_Work/):

- **Canonical API reference:** [`RE_Work/API_REFERENCE.md`](../RE_Work/API_REFERENCE.md) —
  endpoints + 403 matrix, manifest/keymaster/depot schemas, the AES-GCM
  depot crypto recipe, the empirically confirmed RSA signature scheme
  (RSA-4096 PKCS#1 v1.5 over double SHA-256 of the raw depot `mac`),
  embedded key table, binary evidence with addresses, confirmed vs
  hypothesis vs unresolved sections, and exact reproduction commands.
- **Tools and fixtures:** `RE_Work/tools/`, `RE_Work/probes/`, `RE_Work/samples/`
- **Independent offline verification:** `RE_Work/review/verify-saved-sample.mjs`
  (run: `node RE_Work/review/verify-saved-sample.mjs`) and its session
  review `RE_Work/review/qwen-session-review.md`.

Quick summary of what is **verified** (full detail in the reference):

1. `mac` = SHA-256 of raw depot bytes (6 depots checked).
2. Depot signatures verify with the embedded 4096-bit "ghub" public key
   (SHA-256-of-mac-digest scheme, proven by PKCS#1 unpadding + DigestInfo
   forensics); all 1,333 live manifest signatures are 512 B ⇒ the updater
   runs as app `"ghub"`.
3. Encrypted capsule depots (magic `0x20210506`) decrypt with
   AES-128-GCM, key = base64 keymaster key selected by header `key-id`,
   IV = PBKDF2-HMAC-SHA512(expected-SHA-hex, key name, 1000) per chunk —
   reproduced for all 9 chunks of a saved sample.
4. Full endpoint/keymaster discovery chain exercised live (manifest
   `keys.accessGroup` → `/pipeline/v2/access/{uuid}/content.json`); depot
   objects are served directly by S3 — `updates.ghub.logitechg.com` is an S3
   alias of the `2pipeline` bucket (`server: AmazonS3`), and v2 relative
   depot URLs also resolve on the S3 origin (H3 confirmed 2026-09-07).
5. Random-access depot reads ("resource_access") fully mapped in the binary:
   `CapsuleMetadata` indexes a depot (offset/size trees over the `[u32 len]`
   chunk scan) and `capsule_open_file`/`capsule_verify_file`/
   `capsule_list_resources` read individual files by name, decrypting +
   decompressing in place with **password = record.sha** (same as the
   extract path); on any failure it falls back to full extraction
   (§5.8 of the reference).
6. **URL construction + request headers (2026-09-07):** all pipeline JSON
   URLs are a 5-segment base (`server, pipeline/v2/update, appId, win,
   channel`) + a fixed suffix (`/update.json`, `/details.json`, `/settings`),
   with **no query parameters anywhere**; the host comes from settings/IPC,
   not a binary literal. Every such request carries exactly two headers —
   `logi-install-id` (per-machine identifier) and `logi-app-version` —
   built by `pipeline_build_install_headers` (0x14022BE90) (§2.0 of the
   reference).
7. **H6 closed (2026-09-07):** the machine identifier (type-1 = 64-hex
   SHA-256 of computer-name UTF-16LE + C: volume serial; else HDD serial) is
   persisted in the `HKLM\SOFTWARE\Logitech\LGHUB\Data\canary_machine_identifier`
   SecureStorage container and sent as `logi-install-id`. The **server
   buckets installs by that header deterministically** — absent/empty →
   `public`, most well-formed 64-hex ids → `canary` (~4/6 tested) — so the
   earlier "stateless, same manifest for everyone" reading was wrong. For
   ghub10/win, canary and public currently serve identical build content
   (only `channel` + `lastModified` differ). Version selection is plain
   string equality (downgrade-capable), and FeatureCanary is fed by the
   `/settings` response (403 on this host ⇒ canary disabled here).
8. **Local install data (H7, 2026-09-07):** local updater 2026.5.939708 and
   software manager 2026.5.9708.0 (log of 2026-08-08: depot 824196, live
   self-update SUCCESS) are newer than every observed live channel
   (ghub10 2025.9.814156 incl. canary; ghub12 2026.2.861817), so channel
   alone doesn't explain the local-newer-than-live state.

Known open items: GCM tag placement in `0x20210521`, `/settings` schema
(403 on this host), the SecureStorage container format / machine-id byte
repro, the server-side bucket hash, xdelta depot layout, `/scarif/keyswap`
semantics, the `2026.6.957899` UA origin, and the post-factory result chain
— see §14 of the reference.
