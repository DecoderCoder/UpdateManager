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
   `keys.accessGroup` → `/pipeline/v2/access/{uuid}/content.json`).

Known open items: GCM tag placement, v2 manifest signature TBS bytes,
`settings.settings` behavior, compressed/xdelta depot layouts, and
`/scarif/keyswap` semantics — see §14 of the reference.
