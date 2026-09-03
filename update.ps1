<#
    ZOR Auto-Updater
    Checks GitHub for new versions, downloads and applies updates.
    Run as Administrator for driver-related file replacements.

    Usage:
      update.ps1                  # Check + apply if update available
      update.ps1 -Force           # Force re-download current version
      update.ps1 -CheckOnly       # Just check, don't download
      update.ps1 -Version "10.1"  # Download specific version
#>

param(
    [switch]$Force,
    [switch]$CheckOnly,
    [string]$Version = ""
)

$ErrorActionPreference = "Stop"

# ── Config ──────────────────────────────────────────────────────────────────
$RepoOwner  = "Yourmama2201"
$RepoName   = "ZOR"
$Branch     = "master"
$RawBase    = "https://raw.githubusercontent.com/$RepoOwner/$RepoName/$Branch"
$ReleaseBase= "https://github.com/$RepoOwner/$RepoName/releases"

# Where the loader lives (next to this script, or override with -TargetDir)
$TargetDir  = Split-Path -Parent $MyInvocation.MyCommand.Path
$BackupDir  = Join-Path $TargetDir "backups"
$TempDir    = Join-Path $env:TEMP "ZOR_Updater_$(Get-Random)"

# Files to update (relative to TargetDir)
$CoreFiles = @(
    "ZORLoader.exe",
    "ZORClient.dll",
    "zordriver.sys",
    "kdmapper.exe"
)

# ── Helpers ─────────────────────────────────────────────────────────────────
function Write-Log {
    param([string]$Msg, [string]$Color = "White")
    $ts = Get-Date -Format "HH:mm:ss"
    Write-Host "[$ts] $Msg" -ForegroundColor $Color
}

function Get-LocalVersion {
    $vp = Join-Path $TargetDir "version.txt"
    if (Test-Path $vp) {
        return (Get-Content $vp -Raw).Trim()
    }
    return "0.0"
}

function Get-RemoteVersion {
    try {
        $url = "$RawBase/version.txt"
        $resp = Invoke-WebRequest -Uri $url -UseBasicParsing -TimeoutSec 10
        return ($resp.Content).Trim()
    } catch {
        Write-Log "Failed to fetch remote version: $_" "Red"
        return $null
    }
}

function Get-RemoteChangelog {
    try {
        $url = "$RawBase/changelog.txt"
        $resp = Invoke-WebRequest -Uri $url -UseBasicParsing -TimeoutSec 10
        return $resp.Content
    } catch {
        return "(changelog unavailable)"
    }
}

function Download-Release {
    param([string]$Ver)

    $tag = "v$Ver"
    Write-Log "Downloading release $tag ..." "Cyan"

    # Try to download the release zip
    $zipUrl = "$ReleaseBase/download/$tag/ZOR_v$Ver.zip"
    $zipPath = Join-Path $TempDir "ZOR_v$Ver.zip"

    try {
        # Use GitHub API to find the asset
        $apiUrl = "https://api.github.com/repos/$RepoOwner/$RepoName/releases/tags/$tag"
        $headers = @{}
        $token = $env:GITHUB_TOKEN
        if ($token) { $headers["Authorization"] = "token $token" }

        $release = Invoke-RestMethod -Uri $apiUrl -Headers $headers -TimeoutSec 15

        if ($release.assets -and $release.assets.Count -gt 0) {
            # Find zip asset
            $asset = $release.assets | Where-Object { $_.name -like "*.zip" } | Select-Object -First 1
            if ($asset) {
                Write-Log "Downloading asset: $($asset.name)" "Cyan"
                $dlUrl = $asset.browser_download_url
                Invoke-WebRequest -Uri $dlUrl -OutFile $zipPath -UseBasicParsing -TimeoutSec 120
                return $zipPath
            }
        }

        # Fallback: direct URL
        Write-Log "No asset found, trying direct download..." "Yellow"
        Invoke-WebRequest -Uri $zipUrl -OutFile $zipPath -UseBasicParsing -TimeoutSec 120
        return $zipPath
    } catch {
        Write-Log "Download failed: $_" "Red"
        return $null
    }
}

function Extract-Update {
    param([string]$ZipPath)

    $extractDir = Join-Path $TempDir "extracted"
    New-Item -ItemType Directory -Path $extractDir -Force | Out-Null

    Write-Log "Extracting update..." "Cyan"
    Expand-Archive -Path $ZipPath -DestinationPath $extractDir -Force

    # Find the actual files (may be in a subfolder)
    $found = @{}
    foreach ($file in $CoreFiles) {
        $located = Get-ChildItem -Path $extractDir -Recurse -Filter $file -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($located) {
            $found[$file] = $located.FullName
        }
    }
    return $found
}

function Backup-Current {
    if (-not (Test-Path $BackupDir)) {
        New-Item -ItemType Directory -Path $BackupDir -Force | Out-Null
    }
    $ts = Get-Date -Format "yyyyMMdd_HHmmss"
    $bakDir = Join-Path $BackupDir $ts
    New-Item -ItemType Directory -Path $bakDir -Force | Out-Null

    foreach ($file in $CoreFiles) {
        $src = Join-Path $TargetDir $file
        if (Test-Path $src) {
            Copy-Item $src (Join-Path $bakDir $file) -Force
        }
    }
    Write-Log "Backup saved to $bakDir" "Green"
    return $bakDir
}

function Apply-Update {
    param([hashtable]$Files)

    $updated = 0
    foreach ($file in $CoreFiles) {
        if ($Files.ContainsKey($file)) {
            $src = $Files[$file]
            $dst = Join-Path $TargetDir $file

            # Stop any running loader process
            if ($file -eq "ZORLoader.exe") {
                $proc = Get-Process -Name "ZORLoader" -ErrorAction SilentlyContinue
                if ($proc) {
                    Write-Log "Stopping running ZORLoader..." "Yellow"
                    $proc | Stop-Process -Force
                    Start-Sleep -Seconds 2
                }
            }

            try {
                Copy-Item $src $dst -Force
                Write-Log "Updated: $file" "Green"
                $updated++
            } catch {
                Write-Log "Failed to update $file : $_" "Red"
            }
        } else {
            Write-Log "Skipped (not in archive): $file" "Yellow"
        }
    }
    return $updated
}

function Update-VersionFile {
    param([string]$Ver)
    $vp = Join-Path $TargetDir "version.txt"
    Set-Content -Path $vp -Value $Ver -NoNewline
}

function Update-Changelog {
    param([string]$Content)
    $cp = Join-Path $TargetDir "changelog.txt"
    Set-Content -Path $cp -Value $Content
}

# ── Main ────────────────────────────────────────────────────────────────────
Write-Host ""
Write-Host "  ╔══════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "  ║       ZOR AUTO-UPDATER  v1.0         ║" -ForegroundColor Cyan
Write-Host "  ╚══════════════════════════════════════╝" -ForegroundColor Cyan
Write-Host ""

$localVer  = Get-LocalVersion
$remoteVer = Get-RemoteVersion

if (-not $remoteVer) {
    Write-Log "Could not reach GitHub. Check your internet connection." "Red"
    exit 1
}

Write-Log "Local version:  $localVer" "White"
Write-Log "Remote version: $remoteVer" "White"
Write-Host ""

if ($remoteVer -eq $localVer -and -not $Force) {
    Write-Log "Already up to date!" "Green"
    if ($CheckOnly) { exit 0 }

    $changelog = Get-RemoteChangelog
    Write-Host ""
    Write-Host "  Changelog:" -ForegroundColor Yellow
    Write-Host "  ─────────────────────────────────────" -ForegroundColor DarkGray
    $changelog -split "`n" | ForEach-Object { Write-Host "  $_" -ForegroundColor Gray }
    Write-Host ""
    exit 0
}

if ($CheckOnly) {
    Write-Log "Update available: $remoteVer" "Yellow"
    $changelog = Get-RemoteChangelog
    Write-Host ""
    Write-Host "  Changelog:" -ForegroundColor Yellow
    Write-Host "  ─────────────────────────────────────" -ForegroundColor DarkGray
    $changelog -split "`n" | ForEach-Object { Write-Host "  $_" -ForegroundColor Gray }
    Write-Host ""
    exit 0
}

Write-Log "Update available: $localVer -> $remoteVer" "Yellow"
Write-Host ""

# Confirm
$confirm = Read-Host "  Apply update? (Y/n)"
if ($confirm -eq "n" -or $confirm -eq "N") {
    Write-Log "Update cancelled." "Yellow"
    exit 0
}

# Create temp dir
New-Item -ItemType Directory -Path $TempDir -Force | Out-Null

try {
    # Backup current
    Write-Log "Backing up current files..." "Cyan"
    Backup-Current

    # Download
    $targetVer = if ($Version) { $Version } else { $remoteVer }
    $zipPath = Download-Release -Ver $targetVer

    if (-not $zipPath -or -not (Test-Path $zipPath)) {
        Write-Log "Download failed. Try downloading manually from:" "Red"
        Write-Log "  $ReleaseBase" "Yellow"
        exit 1
    }

    # Extract
    $files = Extract-Update -ZipPath $zipPath

    if ($files.Count -eq 0) {
        Write-Log "No update files found in the archive." "Red"
        exit 1
    }

    Write-Log "Found $($files.Count) file(s) in archive" "Green"

    # Apply
    $updated = Apply-Update -Files $files

    # Update version + changelog
    Update-VersionFile -Ver $remoteVer
    $changelog = Get-RemoteChangelog
    Update-Changelog -Content $changelog

    Write-Host ""
    Write-Log "Update complete! $updated file(s) updated to v$remoteVer" "Green"
    Write-Log "Backups saved in: $BackupDir" "DarkGray"

    # Offer to restart
    Write-Host ""
    $restart = Read-Host "  Restart ZOR Loader now? (Y/n)"
    if ($restart -ne "n" -and $restart -ne "N") {
        $loader = Join-Path $TargetDir "ZORLoader.exe"
        if (Test-Path $loader) {
            Write-Log "Starting ZORLoader..." "Cyan"
            Start-Process $loader
        }
    }
} finally {
    # Cleanup temp
    if (Test-Path $TempDir) {
        Remove-Item $TempDir -Recurse -Force -ErrorAction SilentlyContinue
    }
}

Write-Host ""
