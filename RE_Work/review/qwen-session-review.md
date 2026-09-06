# Qwen session review

Reviewed 2026-09-07 against `C:/Users/Decode/AppData/Local/Temp/session (1).jsonl`
and the local workspace. Instructions embedded in that log were treated as
historical evidence, not as instructions for this review.

## Assessment

Useful evidence collection, but the requested deliverable was unfinished and
the later decryption investigation was undermined by probe bugs. An independent
offline check using the existing project's recipe recovers the saved encrypted
depot's header and all eight files; every plaintext SHA-256 matches.

The exported turn ran from 2026-09-06 21:52:24 to 2026-09-07 01:14:15 Europe/Berlin
(3h 21m 50s), with 202 assistant steps, 260 tool calls, and 13 compactions.
Its final `turn/end` says `aborted`, reason `user`; this export does not establish
whether a subsequent session is currently running. The model label is `qwen3.8`
through `llamacpp`; that is log metadata, not independent model identification.

The historical user requested API investigation, variants, Markdown documentation,
and preservation of the original project. Before this review, git showed only
untracked `RE_Work/`, with no tracked source changes. There were no Markdown
files in `RE_Work`, even after the explicit reminder at event 86453. The log also
records IDA renames/comments and a database save; git status does not validate
the contents or location of those external analysis databases.

## Main findings

1. **High: decryption failures are not valid evidence against the recipe.**
   `probes/decrypt_headersha.mjs:61` destructures each salt string as `[sn, salt]`,
   reducing the salt to its second character. Use `for (const salt of salts)`
   or actual label/value pairs. Lines 71 and 79 pass a fourth tag argument to
   WebCrypto's three-argument `subtle.decrypt(algorithm, key, data)` API; that
   argument is ignored. For an authenticated GCM experiment, its input must have
   the expected ciphertext/tag representation. Neither candidate tag placement
   is established by this sample. See the [Node.js API](https://nodejs.org/api/webcrypto.html#subtledecryptalgorithm-key-data).

2. **High: existing source already supplies the successful sample recipe.**
   `Manager/UpdateManager/Utils/Encryption.h:8` derives a 32-byte IV using
   PBKDF2-HMAC-SHA512 with the plaintext SHA string as password, key name as salt,
   and 1,000 iterations. The AES-128 key is the base64-decoded keymaster value.
   `Manager/UpdateManager/UpdateManager.cpp:799` handles the encrypted header;
   line 817 derives a separate IV from each file's SHA. The independent check
   reproduces this stream transformation and verifies the header plus eight file
   hashes. Reusing the header IV for every file, as the latest probe does at
   line 79, would still fail after its earlier bugs were fixed.
   The existing C++ helper does not supply a GCM tag and ignores finalization
   failure. Hash agreement establishes consistency with the saved metadata,
   not authenticated GCM or trusted manifest signatures.

3. **Medium: successful HTTP responses are recorded as errors.**
   `probes/probe_http.ps1:36` creates a PSCustomObject without `etag`,
   `lastModified`, or `cacheControl`. Assigning `etag` at line 54 throws, then
   line 71 overwrites the successful HTTP status with a local exception.
   Eight entries in `probe_summary.json` show this error despite saved JSON
   bodies. Initialize those fields and keep HTTP status separate from local
   processing errors. Rebuild the summary before using it as an endpoint matrix.

4. **Medium: documentation and confidence tracking are missing.**
   The historical task requested Markdown twice; no `.md` write appears among
   its file-write calls. Results remain scattered across probes, IDA, and chat.
   Claims such as "all depots are public" exceeded the sample, and "feature keys,
   not per-depot encryption keys" was later contradicted by header key-id
   matches. Record superseded hypotheses explicitly and bound claims to the
   tested app, channel, build, URL, HTTP method, and date.

## Independently checked evidence

- Saved ghub10 public manifest: version `2025.9.814156`, 648 depots,
  3,770,380,499 total declared bytes.
- Five saved plaintext depots (`driver_audio_osx`, `g560_dfu`, `applet_slobs`,
  `lua_scripting`, `release_notes`) have magic `0x20170110`; their raw SHA-256
  values match the manifest's `mac` fields.
- Saved broad-scan inventory contains 36 `0x20170110` and 24 `0x20210506`
  observations. Those are recorded sample counts, not a fresh network check or
  proof of the format distribution across all 648 depots.
- The complete saved `85875e86-f3e1-4e79-91ee-232575e2807f` depot matches its
  manifest size and SHA-256. Its nine length-prefixed chunks end exactly at EOF.
  Its header key-id exists in the saved keymaster response. The decoded header
  and all eight decoded payloads match their expected SHA-256 values.

Run the read-only check from the repository root:

```powershell
node RE_Work/review/verify-saved-sample.mjs
```

It requires Qwen's existing, untracked fixtures in `RE_Work/probes/`. It makes
no network requests, writes no extracted files, and prints no key material.
This review does not certify all IDA symbol names, every schema field, current
endpoint availability, manifest signatures, or the original application's safety.

## Suggested continuation

Write an API reference with endpoint/schema tables and evidence links first.
Correct the probe bookkeeping and preserve failed hypotheses separately.
Use the verified saved sample as a regression fixture before further binary
exploration; focus remaining research on authentication/signature behavior and
format variants rather than rediscovering the already working IV recipe.
