# H3 test: do v2 relative depot URLs resolve on the S3 origin
# (2pipeline.s3.amazonaws.com), or only on the CloudFront host?
# Two requests, 632 B each, 700 ms apart. New OutDir per run (etiquette).
param([string]$OutDir = "C:\Users\Decode\source\repos\UpdateManager\RE_Work\probes\reprobe_2026_09_07_h3")
$ErrorActionPreference = 'Stop'
if (-not (Test-Path $OutDir)) { New-Item -ItemType Directory -Path $OutDir | Out-Null }
$ua = "LGHUB/2026.6.957899 (Windows NT 10.0; x64)"
$depoRel = "depots/780f7572-689c-45d5-894e-706f02c8f13e/g560_dfu.depot"
$expectMac = (node -e "const m=JSON.parse(require('fs').readFileSync('C:/Users/Decode/source/repos/UpdateManager/RE_Work/probes/reprobe_2026_09_07/resp_pipeline_v2_update_ghub10_win_public_details.json.json','utf8')); console.log(m.depots.find(d=>d.name==='g560_dfu').mac)")
$targets = @(
  @{ host = "http://2pipeline.s3.amazonaws.com"; tag = "s3_origin" },
  @{ host = "http://updates.ghub.logitechg.com"; tag = "cloudfront" }
)
$rows = @()
foreach ($t in $targets) {
  $url = $t.host + "/" + $depoRel
  $file = Join-Path $OutDir ("h3_" + $t.tag + ".depot")
  $row = [PSCustomObject]@{ host = $t.host; status = ""; bytes = 0; mac = ""; match = ""; error = "" }
  try {
    $resp = Invoke-WebRequest -Uri $url -Headers @{ "User-Agent" = $ua } -UseBasicParsing -TimeoutSec 30
    $row.status = [int]$resp.StatusCode
    $ci = $resp.Content
    if ($ci -is [byte[]]) { [System.IO.File]::WriteAllBytes($file, $ci) } else { [System.IO.File]::WriteAllText($file, $ci) }
    $row.bytes = if ($ci -is [byte[]]) { $ci.Length } else { $ci.Length }
  } catch {
    $r = $_.Exception.Response
    if ($r) { $row.status = [int]$r.StatusCode; $row.error = $r.ContentType } else { $row.error = $_.Exception.Message }
  }
  if (Test-Path $file) {
    $mac = (Get-FileHash -Path $file -Algorithm SHA256).Hash.ToLower()
    $row.mac = $mac
    $row.match = ($mac -eq $expectMac)
  }
  $rows += $row
  Write-Output ("{0,-14} {1,-5} {2,-8} mac-match={3}  {4}" -f $t.tag, $row.status, $row.bytes, $row.match, $row.error)
  Start-Sleep -Milliseconds 700
}
$rows | ConvertTo-Json -Depth 3 | Set-Content (Join-Path $OutDir "h3_summary.json") -Encoding UTF8
Write-Output "DONE"
