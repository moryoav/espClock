$ErrorActionPreference = "Stop"
$Destination = Join-Path $PSScriptRoot "lib\p5.min.js"
$Url = "https://cdn.jsdelivr.net/npm/p5@1.11.11/lib/p5.min.js"

Write-Host "Downloading official p5.js 1.11.11..."
Invoke-WebRequest -Uri $Url -OutFile $Destination
Write-Host "Saved to $Destination"
Write-Host "Open index-local-p5.html to use the local official library."
