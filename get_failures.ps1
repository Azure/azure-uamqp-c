param([string]$BuildId)
$ErrorActionPreference = 'Stop'
$file = "timeline_$BuildId.json"
$env:PYTHONIOENCODING = 'utf-8'
az rest --method get --uri "https://dev.azure.com/azure-iot-sdks/azure-iot-sdks/_apis/build/builds/$BuildId/timeline?api-version=7.0" --resource "499b84ac-1321-427f-aa17-267ca6975798" --output-file $file 2>&1 | Out-Null
$tl = Get-Content $file -Raw | ConvertFrom-Json
$failed = $tl.records | Where-Object { $_.result -eq 'failed' }
foreach ($r in $failed | Sort-Object order) {
  $parent = ($tl.records | Where-Object { $_.id -eq $r.parentId }).name
  "[{0}] {1} :: {2}  (errors={3}) log={4}" -f $r.type, $parent, $r.name, $r.errorCount, $r.log.url
}
