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
   forensics); all 1,333 live manifest signatures are 512 B — consistent
   with the 4096-bit "ghub" key being the runtime key (the runtime app-name
   variable is an inference from key selection, not an observed value).
3. Encrypted capsule depots (magic `0x20210506`) decrypt with
   AES-128-GCM, key = base64 keymaster key selected by header `key-id`,
   IV = PBKDF2-HMAC-SHA512(expected-SHA-hex, key name, 1000) per chunk —
   reproduced for all 9 chunks of a saved sample.
4. Full endpoint/keymaster discovery chain exercised live (manifest
   `keys.accessGroup` → `/pipeline/v2/access/{uuid}/content.json`).
   Infrastructure: `updates.ghub.logitechg.com` is a **CloudFront
   distribution with S3 origin** — captures show both `server: AmazonS3`
   and `via: …cloudfront.net (CloudFront)` / `x-amz-cf-id` / `x-cache`
   (the earlier "S3 alias, not CloudFront" claim was wrong); v2 relative
   depot URLs also resolve on the S3 origin `2pipeline.s3.amazonaws.com`
   (H3 confirmed 2026-09-07).
5. Random-access depot reads ("resource_access") fully mapped in the binary:
   `CapsuleMetadata` indexes a depot (offset/size trees over the `[u32 len]`
   chunk scan) and `capsule_open_file`/`capsule_verify_file`/
   `capsule_list_resources` read individual files by name, decrypting +
   decompressing in place with **password = record.sha** (same as the
   extract path); on any failure it falls back to full extraction
   (§5.8 of the reference).
6. **URL construction + request headers (2026-09-07):** all pipeline JSON
   URLs are a 5-segment base (`server, pipeline/v2/update, appId, win,
   channel`) + a fixed suffix (`/update.json`, `/details.json`, `/settings` —
   the client's actual settings suffix has no `.json`), with **no query
   parameters in the traced client builders**; the runtime host comes from
   settings/IPC (the hostname *is* a binary literal: element 0 of the
   4-host array @0x1413D8B40 — an earlier "absent" statement was wrong).
   Every such request carries exactly two headers — `logi-install-id`
   (per-machine identifier) and `logi-app-version` — built by
   `pipeline_build_install_headers` (0x14022BE90) (§2.0 of the reference).
7. **H6 closed (2026-09-07):** the machine identifier (type-1 = 64-hex
   SHA-256 of computer-name UTF-16LE + C: volume serial; else the C: volume
   serial as a **decimal string**) is persisted in the
   `HKLM\SOFTWARE\Logitech\LGHUB\Data\canary_machine_identifier`
   SecureStorage container and sent as `logi-install-id`. The **server
   buckets installs by that header deterministically** — absent/empty →
   `public`, most well-formed 64-hex ids → `canary` (~4/6 tested) — so the
   earlier "stateless, same manifest for everyone" reading was wrong. For
   ghub10/win, canary and public serve the **same build** (differing only
   in `channel` + `lastModified`; not byte-identical objects). Version
   selection is plain string equality (downgrade-capable), and FeatureCanary
   is fed by the `/settings` response (403 on this host ⇒ canary disabled
   here).
8. **Channel census (2026-09-07):** channel is a free-form path segment
   (no client whitelist). For ghub10/win: `public`/`canary` → 200 plaintext
   JSON; `tim` (update 207 B / details 809,601 B) and `staging` (update
   198 B / details 938,800 B) → 200 with **opaque bodies** (fail JSON
   parse, no known magics — encoding + consumer unresolved, *not*
   decrypted); 14 further probed names → 403 S3 XML. Structural fact:
   `update.json` is the leading section of `details.json` (strict prefix
   in public; 205/207 and 196/198 B common prefixes on the encoded
   channels). An earlier "staging recovered" claim was **disproven** —
   that file is a circular XOR of an assumption (§2.5, §13 of the
   reference).
9. **Local install data (H7, 2026-09-07):** local updater 2026.5.939708 and
   software manager 2026.5.9708.0 (log of 2026-08-08: depot 824196, live
   self-update SUCCESS). **Largely closed by the matrix round:** live
   `ghub13/win/public` serves buildId 824196 / 2026.5.939708 — exactly the
   local build; the local machine is on app **ghub13** (the updater's
   factory-default app id), not ghub10/ghub12.
10. **Variant matrix round (2026-09-07, 30 live requests):** apps
   **ghub10/ghub12/ghub13** live (ghub99 control → 403); platform tokens
   **win + osx** live (mac/linux → 403); channel sets are per-app
   (ghub12 has staging but **not** tim); **query parameters ignored**;
   **`logi-app-version` has no observable server-side effect** (bucketing
   is `logi-install-id`-driven); Range → 206, conditionals → 304,
   case/slash variants → 403 (single template); S3 **ListObjects disabled**
   (403, corrected test); depots CDN-cached (`x-cache: Hit` + `age`);
   `pipeline.logitech.io` = **internal private us-east-1 ALB** (unreachable
   publicly); v1/v2 `update.json` = same object for ghub12 (v1 is
   server-side legacy — no v1 literal in either client binary).

Companion deliverables (this round):

- **[`RE/API_VARIANT_MATRIX.md`](API_VARIANT_MATRIX.md)** — tested route
  × app × platform × channel × suffix × HTTP-behavior combinations,
  results, and the exact coverage achieved (not a claim of exhaustiveness).
- **[`RE/API_FINDINGS.md`](API_FINDINGS.md)** — evidence-backed
  discoveries plus the unresolved hypotheses, each tied to fixtures.

Known open items: GCM tag placement in `0x20210521`, `/settings` schema
(403 on this host), the SecureStorage container format / machine-id byte
repro, the server-side bucket hash, xdelta depot layout, `/scarif/keyswap`
semantics, the `2026.6.957899` UA origin, the post-factory result chain,
the encoding + consumer of the tim/staging opaque channel bodies, ghub13
manifest content (details not yet fetched — would supply a `0x20210521`
candidate), and methods beyond GET/HEAD — see §14 of the reference and
`RE/API_FINDINGS.md`.
