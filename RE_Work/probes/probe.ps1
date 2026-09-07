# Probe script for Logitech G HUB pipeline update API (Invoke-WebRequest version)
# Polite probing: ~17 sequential GET requests, 500ms delay between.
$ErrorActionPreference = 'Stop'
[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12

$outDir = "C:\Users\Decode\source\repos\UpdateManager\RE_Work\probes"
$base = "https://updates.ghub.logitechg.com"
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
  $ext = if ($p -match "json$") { "json" } else { "bin" }
  $file = Join-Path $outDir ("resp_" + $name + "." + $ext)
  $row = [PSCustomObject]@{ path = $p; status = ""; contentLength = ""; contentType = "" }
  try {
    $resp = Invoke-WebRequest -Uri $url -Headers $headers -UseBasicParsing -TimeoutSec 20 -OutFile $file
    $row.status = [int]$resp.StatusCode
    $row.contentType = $resp.Headers['Content-Type']
    $fi = Get-Item $file
    $row.contentLength = $fi.Length
    if ($fi.Length -gt 0 -and $fi.Length -lt 200000) {
      $head = (Get-Content $file -Raw -Encoding UTF8) -replace "[\r\n]+", " "
      $row.head = $head.Substring(0, [Math]::Min(250, $head.Length))
    }
    if ($resp.Headers['ETag']) { $row.etag = $resp.Headers['ETag'] }
    if ($resp.Headers['Last-Modified']) { $row.lastModified = $resp.Headers['Last-Modified'] }
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
        $row.contentLength = $body.Length
        $row.head = $body.Substring(0, [Math]::Min(250, $body.Length)) -replace "[\r\n]+", " "
      } catch {}
    } else {
      $row.status = "ERR: " + $_.Exception.Message
    }
  }
  $summary += $row
  Write-Output ("{0,-70} {1,-8} {2,-10} {3}" -f $p, $row.status, $row.contentLength, $row.contentType)
  Start-Sleep -Milliseconds 500
}

$summary | ConvertTo-Json -Depth 3 | Set-Content (Join-Path $outDir "probe_summary.json") -Encoding UTF8
Write-Output "DONE. Summary written to probe_summary.json"
