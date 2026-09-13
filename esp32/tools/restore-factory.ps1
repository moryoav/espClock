param(
    [Parameter(Mandatory = $true)]
    [string]$BackupFile,
    [string]$Port = "COM3",
    [switch]$IUnderstandThisOverwritesTheClock
)

$ErrorActionPreference = "Stop"

function Invoke-Esptool {
    param([string[]]$Arguments)

    $Tool = Get-Command esptool -ErrorAction SilentlyContinue
    if ($Tool) {
        & $Tool.Source @Arguments
    }
    else {
        $Py = Get-Command py -ErrorAction SilentlyContinue
        if (-not $Py) {
            throw "esptool was not found. Install it with: py -m pip install esptool"
        }
        & $Py.Source -m esptool @Arguments
    }

    if ($LASTEXITCODE -ne 0) {
        throw "esptool exited with code $LASTEXITCODE."
    }
}

if (-not $IUnderstandThisOverwritesTheClock) {
    throw "Re-run with -IUnderstandThisOverwritesTheClock after checking that BackupFile is your verified 16 MB factory dump."
}

$Resolved = (Resolve-Path $BackupFile).Path
$Length = (Get-Item $Resolved).Length
if ($Length -ne 16777216) {
    throw "Factory image must be exactly 16,777,216 bytes; actual size is $Length."
}

Write-Host "Restoring the verified full factory flash image to $Port..."
Invoke-Esptool -Arguments @(
    "--chip", "esp32s3",
    "--port", $Port,
    "--before", "usb-reset",
    "--after", "hard-reset",
    "--no-stub",
    "write-flash",
    "--flash-mode", "keep",
    "--flash-freq", "keep",
    "--flash-size", "keep",
    "0x0000", $Resolved
)
Write-Host "Restore complete. If necessary, unplug and reconnect USB."
