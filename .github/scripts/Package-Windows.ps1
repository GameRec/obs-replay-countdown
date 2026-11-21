[CmdletBinding()]
param(
    [ValidateSet('x64')]
    [string] $Target = 'x64',
    [ValidateSet('Debug', 'RelWithDebInfo', 'Release', 'MinSizeRel')]
    [string] $Configuration = 'RelWithDebInfo'
)

$ErrorActionPreference = 'Stop'

if ( $DebugPreference -eq 'Continue' ) {
    $VerbosePreference = 'Continue'
    $InformationPreference = 'Continue'
}

if ( $env:CI -eq $null ) {
    throw "Package-Windows.ps1 requires CI environment"
}

if ( ! ( [System.Environment]::Is64BitOperatingSystem ) ) {
    throw "Packaging script requires a 64-bit system to build and run."
}

if ( $PSVersionTable.PSVersion -lt '7.2.0' ) {
    Write-Warning 'The packaging script requires PowerShell Core 7. Install or upgrade your PowerShell version: https://aka.ms/pscore6'
    exit 2
}

function Package {
    trap {
        Write-Error $_
        exit 2
    }

    $ScriptHome = $PSScriptRoot
    $ProjectRoot = Resolve-Path -Path "$PSScriptRoot/../.."
    $BuildSpecFile = "${ProjectRoot}/buildspec.json"

    $UtilityFunctions = Get-ChildItem -Path $PSScriptRoot/utils.pwsh/*.ps1 -Recurse

    foreach( $Utility in $UtilityFunctions ) {
        Write-Debug "Loading $($Utility.FullName)"
        . $Utility.FullName
    }

    $BuildSpec = Get-Content -Path ${BuildSpecFile} -Raw | ConvertFrom-Json
    $ProductName = $BuildSpec.name
    $DisplayName = if ($BuildSpec.displayName) { $BuildSpec.displayName } else { $ProductName }
    $ProductVersion = $BuildSpec.version

    $OutputName = "${ProductName}-${ProductVersion}-windows-${Target}"
    $InstallerName = "${OutputName}-Installer"
    $ReleaseStageDir = "${ProjectRoot}/release/${Configuration}"
    $ReleaseOutDir = "${ProjectRoot}/release"

    $RemoveArgs = @{
        ErrorAction = 'SilentlyContinue'
        Path = @(
            "${ReleaseOutDir}/${ProductName}-*-windows-*.zip",
            "${ReleaseOutDir}/${ProductName}-*-windows-*.exe"
        )
    }

    Remove-Item @RemoveArgs

    Log-Group "Archiving ${ProductName} ZIP..."
    $CompressArgs = @{
        Path = (Get-ChildItem -Path "${ReleaseStageDir}" -Exclude "${OutputName}*.*")
        CompressionLevel = 'Optimal'
        DestinationPath = "${ReleaseOutDir}/${OutputName}.zip"
        Verbose = ($Env:CI -ne $null)
    }
    Compress-Archive -Force @CompressArgs
    Log-Group

    $IssScript = "${ProjectRoot}/installer/installer.iss"
    if (-not (Test-Path $IssScript)) {
        throw "Brak pliku installer/installer.iss w repozytorium!"
    }

    $IsccCmd = Get-Command "ISCC.exe" -ErrorAction SilentlyContinue
    $IsccExe = if ($IsccCmd) { $IsccCmd.Source } else { "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe" }

    if (-not (Test-Path $IsccExe)) {
        Log-Group "Installing Inno Setup..."
        choco install innosetup -y --no-progress
        $IsccExe = "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe"
        Log-Group
    }

    Log-Group "Building Windows Installer (${InstallerName}.exe)..."
    & $IsccExe `
        "/DMyAppName=${DisplayName}" `
        "/DMyAppVersion=${ProductVersion}" `
        "/DMySourceDir=${ReleaseStageDir}" `
        "/DMyOutputDir=${ReleaseOutDir}" `
        "/DMyOutputBaseFilename=${InstallerName}" `
        "${IssScript}"
    if ($LASTEXITCODE -ne 0) {
        throw "Inno Setup compiler failed with exit code ${LASTEXITCODE}"
    }
    Log-Group
}

Package