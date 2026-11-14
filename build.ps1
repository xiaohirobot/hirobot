param(
    [string]$Python = 'D:\MOCE\runtime\python.exe',
    [string]$Compiler = 'D:\MOCEworkplace\tools\st-windows\compiler\bin'
)
$ErrorActionPreference = 'Stop'
if (-not (Test-Path -LiteralPath $Python)) { throw "Python not found: $Python" }
$ErrorActionPreference = 'Continue' # Native stderr may contain compiler diagnostics.
& $Python (Join-Path $PSScriptRoot 'tools\windows.py') build --compiler $Compiler
if ($LASTEXITCODE -ne 0) { throw "STM32 build failed ($LASTEXITCODE); do not flash." }
