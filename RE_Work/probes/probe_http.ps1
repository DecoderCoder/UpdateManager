# Full pipeline API probe over HTTP (sandbox blocks TLS from pwsh; CloudFront serves HTTP)
# ~17 sequential requests, 400ms delay.
# OutDir is parameterized so re-probes do NOT clobber the canonical
# 2026-09-06 evidence fixtures: pass -OutDir <new subfolder>.
param([string]$OutDir = "C:\Users\Decode\source\repos\UpdateManager\RE_Work\probes")
$ErrorActionPreference = 'Stop'
$outDir = $OutDir
if (-not (Test-Path $outDir)) { New-Item -ItemType Directory -Path $outDir | Out-Null }
$base = "http://updates.ghub.logitechg.com"
$ua = "LGHUB/2026.6.957899 (Windows NT 10.0; x64)"

$paths = @(
  "pipeline/v2/update/ghub10/win/public/details.json",
  "pipeline/v2/update/ghub10/win/public/update.json",
  "pipeline/v2/update/ghub10/win/public/settings",
  "pipeline/v2/update/ghub12/win/public/details.json",
  "pipeline/v2/update/ghub12/win/public/update.json",
  "pipeline/v2/update/ghub12/win/public/settings",
  "pipeline/v2/update/ghub10/win/canary/details.json",
  "pipeline/v2/update/ghub10/win/canary/update.json",
  "pipeline/v2/update/ghub10/win/canary/settings",
  "pipeline/v1/update/ghub10/win/public/details.json",
  "pipeline/v1/update/ghub12/win/public/update.json",
  "pipeline/v2/access/logitech/content.json",
  "pipeline/v2/access/logitech/iat.json",
  "pipeline/v2/access/ghub/content.json",
  "pipeline/v2/access/logigames/content.json",
  "pipeline/v2/access/ghub10/content.json",
  ""
)

$summary = @()
$headers = @{ "User-Agent" = $ua }

foreach ($p in $paths) {
  $url = if ([string]::IsNullOrEmpty($p)) { $base + "/" } else { $base + "/" + $p }
  $name = if ([string]::IsNullOrEmpty($p)) { "root" } else { ($p -replace "/", "_") }
  $ext = if ($p -match "json$") { "json" } elseif ($p -match "settings$") { "settings" } else { "bin" }
  $file = Join-Path $outDir ("resp_" + $name + "." + $ext)
  # All fields initialized up front: adding properties to a PSCustomObject
  # after creation throws under $ErrorActionPreference='Stop' (this is what
  # poisoned the original probe_summary.json).
  $row = [PSCustomObject]@{
    path = $p; status = ""; bytes = 0; contentType = ""; head = ""; ms = 0
    etag = ""; lastModified = ""; cacheControl = ""; error = ""
  }
  $sw = [System.Diagnostics.Stopwatch]::StartNew()
  try {
    $resp = Invoke-WebRequest -Uri $url -Headers $headers -UseBasicParsing -TimeoutSec 30
    $row.status = [int]$resp.StatusCode
    $row.contentType = $resp.Headers['Content-Type']
    $ci = $resp.Content
    if ($ci -is [byte[]]) {
      [System.IO.File]::WriteAllBytes($file, $ci)
      $row.bytes = $ci.Length
      $prev = $ci.Length
      $sample = [System.Text.Encoding]::ASCII.GetString($ci, 0, [Math]::Min(300, $ci.Length))
      $row.head = ($sample -replace "[^\x20-\x7e]", ".")
    } else {
      [System.IO.File]::WriteAllText($file, $ci)
      $row.bytes = $ci.Length
      $row.head = ($ci -replace "[\r\n]+", " ").Substring(0, [Math]::Min(250, $ci.Length))
    }
    $row.etag = $resp.Headers['ETag']
    $row.lastModified = $resp.Headers['Last-Modified']
    $row.cacheControl = $resp.Headers['Cache-Control']
  } catch {
    $r = $_.Exception.Response
    if ($r) {
      $row.status = [int]$r.StatusCode
      $row.contentType = $r.ContentType
      try {
        $sr = [System.IO.StreamReader]::new($r.GetResponseStream())
        $body = $sr.ReadToEnd()
        $sr.Close()
        [System.IO.File]::WriteAllText($file, $body)
        $row.bytes = $body.Length
        $row.head = $body.Substring(0, [Math]::Min(250, $body.Length))
      } catch { $row.error = "body-read: " + $_.Exception.Message }
    } else {
      # Network-level failure (no HTTP response at all): keep `status`
      # reserved for real HTTP codes, record the exception separately.
      $row.error = $_.Exception.Message
    }
  }
  $sw.Stop()
  $row.ms = $sw.ElapsedMilliseconds
  $summary += $row
  $err = if ($row.error) { "  [" + $row.error + "]" } else { "" }
  Write-Output ("{0,-70} {1,-5} {2,-10} {3,-8} {4}{5}" -f $p, $row.status, $row.bytes, $row.ms, $row.contentType, $err)
  Start-Sleep -Milliseconds 400
}

$summary | ConvertTo-Json -Depth 3 | Set-Content (Join-Path $outDir "probe_summary.json") -Encoding UTF8
Write-Output "DONE"
