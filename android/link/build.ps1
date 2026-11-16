# Builds and signs "Tomb Raider Link <version>.apk" through WSL.
# The keystore password is asked for once. After a build that worked it is kept encrypted with Windows DPAPI (only this
# Windows user on this PC can read it back) in %LOCALAPPDATA%\TombRaiderLink\keystore.pass, outside the project.
# Usage (PowerShell, from anywhere): & "<project>\android\link\build.ps1" 0.3.1
#        add -Forget to drop the saved password and be asked again.
param([Parameter(Mandatory = $true)][string]$Version, [switch]$Forget)
$here = $PSScriptRoot
$saved = Join-Path $env:LOCALAPPDATA "TombRaiderLink\keystore.pass"
if ($Forget -and (Test-Path $saved)) { Remove-Item $saved; "Saved keystore password removed." }
$sec = $null; $fromSaved = $false
if (Test-Path $saved) {
  # Trim: the file ends in a line break, which ConvertTo-SecureString refuses (it used to fall back to asking every time)
  try { $sec = (Get-Content $saved -Raw).Trim() | ConvertTo-SecureString -ErrorAction Stop; $fromSaved = $true }
  catch { $sec = $null; "The saved keystore password could not be read ($($_.Exception.Message)); asking instead." }
}
if (-not $sec) { $sec = Read-Host "Keystore password" -AsSecureString }
$bstr = [Runtime.InteropServices.Marshal]::SecureStringToBSTR($sec)
$env:TR_KS_PASS = [Runtime.InteropServices.Marshal]::PtrToStringAuto($bstr)
[Runtime.InteropServices.Marshal]::ZeroFreeBSTR($bstr)
$env:WSLENV = "TR_KS_PASS/u"
try {
  $wslDir = (wsl -- wslpath -a ($here -replace '\\', '/')).Trim()
  $apk = Join-Path $here "Tomb Raider Link $Version.apk"
  if (Test-Path $apk) { Remove-Item $apk }
  wsl -- bash -c "cd '$wslDir' && sed -i 's/\r$//' build.sh && bash build.sh $Version"
  if (Test-Path $apk) {
    Copy-Item $apk (Join-Path $here "..\..\Releases\") -Force; "Copied to Releases"
    if (-not $fromSaved) {   # only a password that signed a build is kept
      New-Item -ItemType Directory -Force (Split-Path $saved) | Out-Null
      $sec | ConvertFrom-SecureString | Set-Content $saved -Encoding ascii
      "Keystore password saved for next time (DPAPI, this Windows user only)."
    }
  } elseif ($fromSaved) {
    "No APK. If the saved keystore password is wrong, run again with -Forget."
  }
} finally { Remove-Item Env:TR_KS_PASS, Env:WSLENV -ErrorAction SilentlyContinue }
