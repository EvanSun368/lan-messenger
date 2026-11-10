$ErrorActionPreference = 'Stop'
Set-Location (Split-Path $PSScriptRoot -Parent)
$version = '0.4.0'
$bundle = Join-Path $PWD 'dist\LanLink'
$exe = Join-Path $bundle 'LanLink.exe'
if (!(Test-Path $exe)) { throw 'PyInstaller executable missing' }
$smoke = Join-Path $env:TEMP ('LanLink-build-smoke-' + [guid]::NewGuid())
New-Item -ItemType Directory $smoke | Out-Null
$png = Join-Path $smoke 'startup.png'
# Run the packaged executable, not Python. Demo starts no network services.
$p = Start-Process -FilePath $exe -ArgumentList @('--demo','--data-dir',('"' + $smoke + '"'),'--screenshot',('"' + $png + '"')) -PassThru
if (!$p.WaitForExit(30000)) { $p.Kill(); throw 'Packaged startup timed out' }
if ($p.ExitCode -ne 0 -or !(Test-Path $png) -or (Get-Item $png).Length -lt 1000) {
    throw 'Packaged GUI smoke failed'
}
Copy-Item $png (Join-Path $PWD 'dist\windows-startup-smoke.png')
Copy-Item README.md,FILE_SHARING.md,SCREEN_SHARING.md,UI_REFERENCE.md $bundle
Copy-Item packaging\WINDOWS.md (Join-Path $bundle 'WINDOWS-README.md')
# Preserve the installed Qt binding's license texts in the distribution.
$qt = '.venv-build\Lib\site-packages\PySide6'
if (Test-Path "$qt\LICENSES") { Copy-Item "$qt\LICENSES" (Join-Path $bundle 'Qt-LICENSES') -Recurse -Force }
$zip = Join-Path $PWD "dist\LanLink-$version-windows-x64-portable.zip"
Compress-Archive -Path $bundle -DestinationPath $zip -Force
(Get-FileHash $zip -Algorithm SHA256).Hash | Set-Content "$zip.sha256"
Write-Output "Built $zip; GUI demo startup passed. Network/device QA is still required."
