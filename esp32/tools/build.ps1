param(
    [string]$Environment = "jc3248w535"
)

$ErrorActionPreference = "Stop"
$ProjectRoot = Resolve-Path (Join-Path $PSScriptRoot "..")

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

Push-Location $ProjectRoot
try {
    Invoke-PlatformIO -Arguments @("run", "-e", $Environment)
    Write-Host "Build complete: .pio/build/$Environment/firmware.bin"
}
finally {
    Pop-Location
}
