[CmdletBinding()]
param(
    [string]$WwMixPath,
    [string]$InstallPath,
    [string]$Configuration = 'Release',
    [switch]$KeepArtifacts
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$sharedParameters = @{
    Configuration = $Configuration
}

if (-not [string]::IsNullOrWhiteSpace($WwMixPath))
{
    $sharedParameters.WwMixPath = $WwMixPath
}
if (-not [string]::IsNullOrWhiteSpace($InstallPath))
{
    $sharedParameters.InstallPath = $InstallPath
}

& (Join-Path -Path $PSScriptRoot -ChildPath 'Test-WwMix.ReadOnly.ps1') @sharedParameters
if ($LASTEXITCODE -ne 0)
{
    exit $LASTEXITCODE
}

if ($KeepArtifacts.IsPresent)
{
    $sharedParameters.KeepArtifacts = $true
}

& (Join-Path -Path $PSScriptRoot -ChildPath 'Test-WwMix.Functional.ps1') @sharedParameters
exit $LASTEXITCODE
