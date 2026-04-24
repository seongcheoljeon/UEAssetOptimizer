#Requires -Version 7.0
<#
.SYNOPSIS
    Fetch and build all third-party dependencies for UEAssetOptimizer.

.DESCRIPTION
    Populates Source/ThirdParty/{CGAL,CGALBoost,GMP,MPFR,meshoptimizer}/ with the
    headers and prebuilt libraries UBT expects.

    Steps:
      1. Download source archives into Scripts/_downloads/ (cached).
      2. Extract into Scripts/_build/ (per-library workdirs).
      3. Copy/build outputs into Source/ThirdParty/<lib>/{include,lib,bin}/.

    Idempotent: skips any lib whose target headers already exist unless -Force.

    Requires: Visual Studio 2022 (MSVC), CMake 3.25+, PowerShell 7+.
#>
[CmdletBinding()]
param(
    [switch]$Force,
    [string]$CGALVersion       = '6.0.1',
    [string]$BoostVersion      = '1.85.0',
    [string]$MeshOptVersion    = '0.22',
    [string]$GmpVersion        = '6.3.0',
    [string]$MpfrVersion       = '4.2.1'
)

$ErrorActionPreference = 'Stop'
$RepoRoot    = Split-Path -Parent $PSScriptRoot
$ThirdParty  = Join-Path $RepoRoot 'Source/ThirdParty'
$Downloads   = Join-Path $PSScriptRoot '_downloads'
$BuildRoot   = Join-Path $PSScriptRoot '_build'

New-Item -ItemType Directory -Path $Downloads, $BuildRoot -Force | Out-Null

function Get-Archive {
    param([string]$Url, [string]$OutFile)
    if (Test-Path $OutFile) {
        Write-Host "  [cache] $OutFile"
        return
    }
    Write-Host "  [download] $Url"
    Invoke-WebRequest -Uri $Url -OutFile $OutFile
}

function Expand-TarArchive {
    param([string]$Archive, [string]$DestDir)
    New-Item -ItemType Directory -Path $DestDir -Force | Out-Null
    tar -xf $Archive -C $DestDir
}

# ---------------------------------------------------------------------------
# meshoptimizer (MIT, source-compiled)
# ---------------------------------------------------------------------------
function Install-MeshOptimizer {
    $target = Join-Path $ThirdParty 'meshoptimizer'
    $incDir = Join-Path $target 'include'
    $srcDir = Join-Path $target 'src'
    if (-not $Force -and (Test-Path (Join-Path $incDir 'meshoptimizer.h'))) {
        Write-Host "[meshoptimizer] already present, skipping."
        return
    }
    Write-Host "[meshoptimizer] installing v$MeshOptVersion"
    $archive = Join-Path $Downloads "meshoptimizer-$MeshOptVersion.tar.gz"
    Get-Archive "https://github.com/zeux/meshoptimizer/archive/refs/tags/v$MeshOptVersion.tar.gz" $archive
    $work = Join-Path $BuildRoot 'meshoptimizer'
    Remove-Item $work -Recurse -Force -ErrorAction SilentlyContinue
    Expand-TarArchive $archive $work

    $root = Get-ChildItem $work -Directory | Select-Object -First 1
    New-Item -ItemType Directory -Path $incDir, $srcDir -Force | Out-Null
    Copy-Item -Force (Join-Path $root 'src/*.h')   $incDir
    Copy-Item -Force (Join-Path $root 'src/*.cpp') $srcDir
}

# ---------------------------------------------------------------------------
# Boost (header-only extraction)
# ---------------------------------------------------------------------------
function Install-CGALBoost {
    $target = Join-Path $ThirdParty 'CGALBoost/include/boost'
    if (-not $Force -and (Test-Path $target)) {
        Write-Host "[CGALBoost] already present, skipping."
        return
    }
    Write-Host "[CGALBoost] installing Boost v$BoostVersion (headers only)"
    $versionUnderscored = $BoostVersion.Replace('.', '_')
    # Use .zip on Windows: .tar.gz contains symlinks that Windows tar.exe cannot
    # create, producing "empty or unreadable filename" errors and partial extractions.
    $archive = Join-Path $Downloads "boost_$versionUnderscored.zip"
    Get-Archive "https://archives.boost.io/release/$BoostVersion/source/boost_$versionUnderscored.zip" $archive
    $work = Join-Path $BuildRoot 'CGALBoost'
    Remove-Item $work -Recurse -Force -ErrorAction SilentlyContinue
    New-Item -ItemType Directory -Path $work -Force | Out-Null
    Write-Host "  [extract] $archive (this can take a few minutes)"
    Expand-Archive -Path $archive -DestinationPath $work -Force
    $root = Join-Path $work "boost_$versionUnderscored"
    $dest = Join-Path $ThirdParty 'CGALBoost/include'
    New-Item -ItemType Directory -Path $dest -Force | Out-Null
    Copy-Item -Recurse -Force (Join-Path $root 'boost') $dest
}

# ---------------------------------------------------------------------------
# CGAL (header-only)
# ---------------------------------------------------------------------------
function Install-CGAL {
    $target = Join-Path $ThirdParty 'CGAL/include/CGAL'
    if (-not $Force -and (Test-Path $target)) {
        Write-Host "[CGAL] already present, skipping."
        return
    }
    Write-Host "[CGAL] installing v$CGALVersion (headers only)"
    $archive = Join-Path $Downloads "CGAL-$CGALVersion-library.tar.xz"
    Get-Archive "https://github.com/CGAL/cgal/releases/download/v$CGALVersion/CGAL-$CGALVersion-library.tar.xz" $archive
    $work = Join-Path $BuildRoot 'CGAL'
    Remove-Item $work -Recurse -Force -ErrorAction SilentlyContinue
    Expand-TarArchive $archive $work
    $root = Get-ChildItem $work -Directory | Select-Object -First 1
    $dest = Join-Path $ThirdParty 'CGAL/include'
    New-Item -ItemType Directory -Path $dest -Force | Out-Null
    Copy-Item -Recurse -Force (Join-Path $root 'include/CGAL') $dest
}

# ---------------------------------------------------------------------------
# GMP + MPFR (binary libs)
#
# Windows-friendly options:
#   a) vcpkg (recommended):
#        vcpkg install gmp:x64-windows-static mpfr:x64-windows-static
#      then copy include/ + lib/ into our ThirdParty.
#   b) MSYS2 cross-build (complex).
#   c) Prebuilt from the CGAL Windows auxiliary installer.
#
# For v0.1 we use vcpkg. Abort with a helpful message if vcpkg isn't on PATH.
# ---------------------------------------------------------------------------
function Install-GmpMpfr {
    $gmpHeader  = Join-Path $ThirdParty 'GMP/include/gmp.h'
    $mpfrHeader = Join-Path $ThirdParty 'MPFR/include/mpfr.h'
    if (-not $Force -and (Test-Path $gmpHeader) -and (Test-Path $mpfrHeader)) {
        Write-Host "[GMP/MPFR] already present, skipping."
        return
    }
    Write-Host "[GMP/MPFR] installing via vcpkg"

    $vcpkg = Get-Command vcpkg -ErrorAction SilentlyContinue
    if (-not $vcpkg) {
        throw @"
vcpkg not found on PATH.

Install vcpkg (pick any directory, e.g. C:\dev\vcpkg):
    git clone https://github.com/microsoft/vcpkg <INSTALL_DIR>
    <INSTALL_DIR>\bootstrap-vcpkg.bat
    [Environment]::SetEnvironmentVariable('PATH',
        ([Environment]::GetEnvironmentVariable('PATH','User') + ';<INSTALL_DIR>'),
        'User')

Open a new PowerShell window so the PATH update is visible, then re-run this script.
"@
    }

    & vcpkg install gmp:x64-windows mpfr:x64-windows | Out-Host
    if ($LASTEXITCODE -ne 0) { throw "vcpkg install failed." }

    # Resolve vcpkg install prefix. The reliable source is the directory
    # containing vcpkg.exe (e.g. C:\dev\vcpkg). Older versions embedded the
    # root in `vcpkg integrate install` output, but the format has drifted.
    $vcpkgRoot    = Split-Path -Parent $vcpkg.Source
    $vcpkgInstall = Join-Path $vcpkgRoot 'installed/x64-windows'
    if (-not (Test-Path $vcpkgInstall)) {
        throw "vcpkg install prefix not found: $vcpkgInstall"
    }
    Write-Host "  [vcpkg] install prefix = $vcpkgInstall"

    # GMP -- keep vcpkg original filenames; the .lib's import records reference
    # the DLL by its exact name "gmp-10.dll", so renaming breaks load-time resolution.
    Copy-ItemEnsure "$vcpkgInstall/include/gmp.h"       (Join-Path $ThirdParty 'GMP/include/gmp.h')
    Copy-ItemEnsure "$vcpkgInstall/include/gmpxx.h"     (Join-Path $ThirdParty 'GMP/include/gmpxx.h') -Optional
    Copy-ItemEnsure "$vcpkgInstall/lib/gmp.lib"         (Join-Path $ThirdParty 'GMP/lib/Win64/gmp.lib')
    Copy-ItemEnsure "$vcpkgInstall/bin/gmp-10.dll"      (Join-Path $ThirdParty 'GMP/bin/Win64/gmp-10.dll') -Optional

    # MPFR -- same rule: do not rename mpfr-6.dll.
    Copy-ItemEnsure "$vcpkgInstall/include/mpfr.h"      (Join-Path $ThirdParty 'MPFR/include/mpfr.h')
    Copy-ItemEnsure "$vcpkgInstall/include/mpf2mpfr.h"  (Join-Path $ThirdParty 'MPFR/include/mpf2mpfr.h') -Optional
    Copy-ItemEnsure "$vcpkgInstall/lib/mpfr.lib"        (Join-Path $ThirdParty 'MPFR/lib/Win64/mpfr.lib')
    Copy-ItemEnsure "$vcpkgInstall/bin/mpfr-6.dll"      (Join-Path $ThirdParty 'MPFR/bin/Win64/mpfr-6.dll') -Optional
}

function Copy-ItemEnsure {
    param(
        [Parameter(Mandatory, Position=0)][string]$Src,
        [Parameter(Mandatory, Position=1)][string]$Dst,
        [switch]$Optional
    )
    if (-not (Test-Path $Src)) {
        if ($Optional) { return }
        throw "Missing source file: $Src"
    }
    $dstDir = Split-Path -Parent $Dst
    New-Item -ItemType Directory -Path $dstDir -Force | Out-Null
    Copy-Item -Force $Src $Dst
}

# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------
Write-Host "=== UEAssetOptimizer ThirdParty prebuild ===`n"

Install-MeshOptimizer
Install-CGALBoost
Install-CGAL
Install-GmpMpfr

Write-Host "`n=== Done. Open your .uproject; UE will rebuild the plugin. ==="
