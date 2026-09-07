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

Known open items: GCM tag placement in `0x20210521`, `settings.settings`
behavior, xdelta depot layout, `/scarif/keyswap` semantics, and the
post-factory result chain — see §14 of the reference.
