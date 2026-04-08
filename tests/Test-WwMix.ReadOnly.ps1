[CmdletBinding()]
param(
    [string]$WwMixPath,
    [string]$InstallPath,
    [string]$Configuration = 'Release'
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

. (Join-Path -Path $PSScriptRoot -ChildPath 'WwMix.TestHelpers.ps1')

$wwmixExe = Resolve-WwMixExecutable -WwMixPath $WwMixPath -Configuration $Configuration
$ra2InstallPath = Get-RedAlert2InstallPath -InstallPath $InstallPath
$archives = Get-Ra2ArchiveFiles -InstallPath $ra2InstallPath

Write-TestSection "Read-Only Archive Audit"
Write-Host "wwmix:      $wwmixExe"
Write-Host "RA2 folder: $ra2InstallPath"
Write-Host "Archives:   $($archives.Count)"

Assert-True ($archives.Count -gt 0) 'No MIX or MMX archives were found in the Red Alert 2 installation.'

$listChecks = 0
$infoChecks = 0
$keyChecks = 0

foreach ($archive in $archives)
{
    $infoResult = Invoke-WwMix -WwMixPath $wwmixExe -Arguments @('i', $archive.FullName) -Label "info :: $($archive.Name)"
    Assert-ExitCodeZero -Result $infoResult
    Assert-Contains -Text $infoResult.Output -Expected 'Archive:' -Message "Info output for $($archive.FullName) must include archive metadata."
    Assert-Match -Text $infoResult.Output -Pattern 'File count:\s+\d+' -Message "Info output for $($archive.FullName) must include a file count."
    $infoChecks++

    $listResult = Invoke-WwMix -WwMixPath $wwmixExe -Arguments @('l', $archive.FullName) -Label "list :: $($archive.Name)"
    Assert-ExitCodeZero -Result $listResult
    Assert-Contains -Text $listResult.Output -Expected 'Name' -Message "List output for $($archive.FullName) must include the table header."
    $entries = Parse-WwMixListOutput -Output $listResult.Output
    Assert-True ($entries.Count -gt 0) "List output for $($archive.FullName) must contain at least one entry."
    $listChecks++

    $isEncrypted = $archive.Extension.Equals('.mmx', [System.StringComparison]::OrdinalIgnoreCase) -or
                   ($infoResult.Output -match 'Encrypted:\s+yes')
    if ($isEncrypted)
    {
        $keyResult = Invoke-WwMix -WwMixPath $wwmixExe -Arguments @('key', '--mix', $archive.FullName, '--game', 'ra2') -Label "key :: $($archive.Name)"
        Assert-ExitCodeZero -Result $keyResult
        Assert-Contains -Text $keyResult.Output -Expected 'Blowfish key:' -Message "Key output for $($archive.FullName) must include the Blowfish key."
        Assert-Contains -Text $keyResult.Output -Expected 'Key source:' -Message "Key output for $($archive.FullName) must include the key source."
        $keyChecks++
    }
}

Write-Host ""
Write-Host "Read-only audit completed successfully."
Write-Host "Info checks: $infoChecks"
Write-Host "List checks: $listChecks"
Write-Host "Key checks:  $keyChecks"
