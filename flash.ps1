param(
    [Parameter(Mandatory=$true)][ValidatePattern('^COM[0-9]+$')][string]$Port,
    [switch]$Build,
    [switch]$CheckOnly,
    [string]$Python = 'D:\MOCE\runtime\python.exe',
    [string]$Compiler = 'D:\MOCEworkplace\tools\st-windows\compiler\bin',
    [string]$LoaderPath = ''
)
$ErrorActionPreference = 'Stop'
if (-not (Test-Path -LiteralPath $Python)) { throw "Python not found: $Python" }
if ($Build) {
    & (Join-Path $PSScriptRoot 'build.ps1') -Python $Python -Compiler $Compiler
}
$flashArgs = @((Join-Path $PSScriptRoot 'tools\windows.py'), 'flash', '--port', $Port)
if ($CheckOnly) { $flashArgs += '--check-only' }
if ($LoaderPath) { $flashArgs += @('--loader-path', $LoaderPath) }
$ErrorActionPreference = 'Continue'
& $Python @flashArgs
if ($LASTEXITCODE -ne 0) { throw "STM32 flash/check failed ($LASTEXITCODE)." }
