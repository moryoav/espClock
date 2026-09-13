param(
    [string]$Port = "COM3",
    [string]$Environment = "jc3248w535"
)

$ErrorActionPreference = "Stop"
$ProjectRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
$BuildDir = Join-Path $ProjectRoot ".pio/build/$Environment"

function Invoke-PlatformIO {
    param([string[]]$Arguments)

    $Pio = Get-Command pio -ErrorAction SilentlyContinue
    if ($Pio) {
        & $Pio.Source @Arguments
    }
    else {
        $Py = Get-Command py -ErrorAction SilentlyContinue
        if (-not $Py) {
            throw "PlatformIO Core was not found. Install the PlatformIO VS Code extension or run: py -m pip install platformio"
        }
        & $Py.Source -m platformio @Arguments
    }

    if ($LASTEXITCODE -ne 0) {
        throw "PlatformIO exited with code $LASTEXITCODE."
    }
}

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

Push-Location $ProjectRoot
try {
    Invoke-PlatformIO -Arguments @("run", "-e", $Environment)

    $Bootloader = Join-Path $BuildDir "bootloader.bin"
    $Partitions = Join-Path $BuildDir "partitions.bin"
    $Firmware = Join-Path $BuildDir "firmware.bin"
    foreach ($File in @($Bootloader, $Partitions, $Firmware)) {
        if (-not (Test-Path $File)) { throw "Expected build file not found: $File" }
    }

    Write-Host "Flashing through the ESP32-S3 ROM loader (--no-stub), matching the reliable factory-backup method..."
    Invoke-Esptool -Arguments @(
        "--chip", "esp32s3",
        "--port", $Port,
        "--before", "usb-reset",
        "--after", "hard-reset",
        "--no-stub",
        "write-flash",
        "--flash-mode", "qio",
        "--flash-freq", "80m",
        "--flash-size", "16MB",
        "0x0000", $Bootloader,
        "0x8000", $Partitions,
        "0x10000", $Firmware
    )

    Write-Host "Flash complete. If the board does not restart automatically, unplug and reconnect USB once."
}
finally {
    Pop-Location
}
