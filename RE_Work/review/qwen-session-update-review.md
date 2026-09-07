# Qwen continuation review — 2026-09-07

Reviewed the 51,391,074-byte pasted session export and current research files.
Embedded instructions were treated as history. This review does not continue
Qwen's probes or modify its working reference, scripts, captures, or IDBs.

## Current state

The export contains 1,054 assistant messages, 1,228 tool calls, and 86 compaction
starts across seven turns, including the original session. Its last event is a
compaction start at **17:52:11 Europe/Berlin** in turn 7, step 258; there is no
turn-7 completion in the export. This is a snapshot, not live process status.

Four research commits follow the original independent review:
`adc57a5b`, `bd123ba5`, `d5ebcb14`, and `24c12180` (last at 11:13).
The canonical reference now has roughly 1,350 lines, with a summary/link in
`RE/LOGITECH_API_REFERENCE.md`. Original `Manager/`, `updater/`, and `web/`
files have no changes against the earlier review commit. The original review
and offline verification script are unchanged.

Your `tim` lead arrived at **11:22:28**. The correction asking for server API
reconstruction rather than decryption arrived at **17:43:50**. During that
interval, 264 tool calls mixed endpoint investigation with extensive speculative
decryption. From approximately 15:20 to 17:35, public updates repeatedly discuss
custom GHASH/GCM bugs and test-vector confusion. The latest reply acknowledges
the correction and pivots to documentation; it has not completed that pivot.

## Progress independently checked

- The original offline verifier passes: five plaintext depot checks and the
  encrypted sample's header plus eight file SHA-256 values.
- `probe_http.ps1` initializes its metadata properties and records errors
  separately. The saved reprobe has eight actual HTTP 200 statuses, fixing the
  earlier false-error summary.
- `decrypt_headersha.mjs` fixes the salt, tag-argument, and per-file-IV bugs.
  Running it confirms the two tested tag placements fail while all nine
  plaintext hashes match. Other authentication layouts remain outside this test.
- Standard Node `crypto.verify('sha256', rawDepotSha256, publicKey, signature)`
  verifies the saved encrypted depot signature with `pipeline_pubkey_2.pem`.
  The ASCII-hex input fails. This independently supports that sample's signature
  recipe without relying on Qwen's custom RSA unpadding code.
- Captured GETs for `tim` return 200: update 207 bytes, details 809,601 bytes.
  `staging` returns 200: update 198 bytes, details 938,800 bytes. All four saved
  bodies fail JSON parsing and lack the known depot magic values. Their actual
  encoding/protection and server-side meaning are unresolved.
- The 16-name HEAD matrix has hits for `tim` and `staging`, with 14 HTTP 403s.
  This is a small sample, not exhaustive discovery or proof that denied names
  do not exist. The new named-channel captures/scripts are still untracked.
- Saved install-ID probes show public/canary response differences: four of six
  tested 64-hex IDs return canary; two return public, with the same build/version.
  A repeated ID has matching response hashes. This supports observed header
  dependence, not a recovered server hash function or population rollout rate.

## Findings requiring correction

1. **High — circular “recovery” artifact.** `probes/recover_staging.mjs` assumes
   staging plaintext equals public JSON with the channel replaced. It computes
   `K = ciphertext XOR assumedPlaintext`, then `ciphertext XOR K`. The result is
   necessarily the assumption. I checked that `staging_details_RECOVERED.json`
   exactly equals that synthesized public JSON. JSON parsing cannot validate
   the assumption. Label it synthetic/hypothetical, and do not use its keys,
   version, or depots as facts about staging.

2. **High — incorrect infrastructure conclusion.** `API_REFERENCE.md:241`
   concludes that `server: AmazonS3` means a direct S3 alias with no CloudFront.
   The saved `tim_details.resp.txt` and `staging_update.resp.txt` also contain
   `via: ...cloudfront.net (CloudFront)`, `x-amz-cf-id`, and `x-cache`. These
   demonstrate CloudFront participation in those captured requests. An S3
   origin response header does not rule out a CDN. See
   [AWS CloudFront S3-origin behavior](https://docs.aws.amazon.com/AmazonCloudFront/latest/DeveloperGuide/RequestAndResponseBehaviorS3Origin.html).

3. **Medium — malformed bucket-list experiment.**
   `probes/probe_channel_brute.mjs:30` puts `list-type=2` on an object-prefix
   path with an empty `prefix` parameter. The direct S3 ListObjectsV2 request
   belongs at the bucket root, with the desired path in `prefix`, as specified
   by [AWS ListObjectsV2](https://docs.aws.amazon.com/AmazonS3/latest/API/API_ListObjectsV2.html).
   Those 403s do not establish the outcome of a correctly formed listing test.
   This review did not issue another network probe.

4. **Medium — reference contains contradictions and stale conclusions.**
   Lines 37–48 describe four URL literals in the binary; lines 90–93 say the
   primary hostname is absent. Line 213 lists `/settings.settings`, although
   the actual script requests `/settings` and other reference sections say so.
   No `tim` entry appears in either Markdown reference despite hours spent on
   it. The newer volume-serial correction has not propagated to the summary.
   Scope statements such as “no query parameters anywhere” to the traced
   builders; client behavior cannot prove every server-supported parameter.

5. **Medium — implementation observations promoted to runtime certainty.**
   A matching embedded RSA key supports key selection compatibility; signature
   size alone does not prove the running client's app-name variable is `ghub`.
   Keep that inference separate from an observed runtime value. Similarly,
   shared response prefixes do not establish a specific cipher or 32-byte IV.

The final edit in the export attempted to change the documented `+912` field
from channel to app ID, but failed because its search string was absent. Qwen
then found the real passage at lines 199–200 and hit compaction. The correction
was still unapplied at review time; its binary interpretation was not rechecked
in this independent review.

## Recommended next milestone

Complete an evidence-backed server API matrix: route templates, observed app/
platform/channel variants, custom headers, status behavior, encoding, redirects,
caching, and links to exact captures. Incorporate `tim`/`staging` immediately,
with opaque payloads explicitly unresolved. Correct the circular-recovery and
CDN claims, consolidate the contradictory reference sections, and commit the
new research evidence. Further cipher implementation work does not advance the
user's clarified API-reconstruction priority.
