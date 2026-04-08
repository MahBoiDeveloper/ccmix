[CmdletBinding()]
param(
    [string]$WwMixPath,
    [string]$InstallPath,
    [string]$Configuration = 'Release',
    [switch]$KeepArtifacts
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

. (Join-Path -Path $PSScriptRoot -ChildPath 'WwMix.TestHelpers.ps1')

function Get-FirstMapPackGuessTarget
{
    param(
        [string]$WwMixExe,
        [object[]]$Archives
    )

    foreach ($archive in $Archives | Where-Object { $_.Extension.Equals('.mmx', [System.StringComparison]::OrdinalIgnoreCase) })
    {
        $listResult = Invoke-WwMix -WwMixPath $WwMixExe -Arguments @('l', $archive.FullName) -Label "guess probe :: $($archive.Name)"
        Assert-ExitCodeZero -Result $listResult
        $entries = Parse-WwMixListOutput -Output $listResult.Output
        $pktEntry = $entries | Where-Object { $_.Name -like '*.pkt' } | Select-Object -First 1
        if ($null -ne $pktEntry)
        {
            return [pscustomobject]@{
                Archive = $archive
                Entry = $pktEntry
                Stem = [System.IO.Path]::GetFileNameWithoutExtension($archive.Name)
            }
        }
    }

    throw 'Could not find an MMX archive with a resolved .pkt entry for the guess test.'
}

$wwmixExe = Resolve-WwMixExecutable -WwMixPath $WwMixPath -Configuration $Configuration
$ra2InstallPath = Get-RedAlert2InstallPath -InstallPath $InstallPath
$archives = Get-Ra2ArchiveFiles -InstallPath $ra2InstallPath
$repoRoot = Get-WwMixRepoRoot
$artifactRoot = New-TestArtifactDirectory -Name 'functional'
$keepArtifacts = $KeepArtifacts.IsPresent
$completed = $false

try
{
    Write-TestSection "Functional WwMix Tests"
    Write-Host "wwmix:      $wwmixExe"
    Write-Host "RA2 folder: $ra2InstallPath"
    Write-Host "Artifacts:  $artifactRoot"

    Assert-True ($archives.Count -gt 0) 'No MIX or MMX archives were found for the functional tests.'

    Write-TestSection "Guess"
    $guessTarget = Get-FirstMapPackGuessTarget -WwMixExe $wwmixExe -Archives $archives
    $guessResult = Invoke-WwMix -WwMixPath $wwmixExe -Arguments @(
        'guess',
        '--id', $guessTarget.Entry.Id,
        '--game', 'ra2',
        '--ext', '.pkt',
        '--prefix', $guessTarget.Stem,
        '--min', '0',
        '--max', '0'
    ) -Label "guess :: $($guessTarget.Archive.Name)"
    Assert-ExitCodeZero -Result $guessResult
    Assert-Contains -Text $guessResult.Output -Expected $guessTarget.Entry.Name -Message 'Guess output must resolve the expected .pkt name.'

    Write-TestSection "Create / List / Info / Extract"
    $createInputDir = Join-Path -Path $artifactRoot -ChildPath 'create-input'
    $extractAllDir = Join-Path -Path $artifactRoot -ChildPath 'extract-all'
    $extractByIdDir = Join-Path -Path $artifactRoot -ChildPath 'extract-by-id'
    $extractLmdDir = Join-Path -Path $artifactRoot -ChildPath 'extract-lmd'
    New-Item -ItemType Directory -Force -Path $createInputDir, $extractAllDir, $extractByIdDir, $extractLmdDir | Out-Null

    $alphaPath = Join-Path -Path $createInputDir -ChildPath 'alpha.txt'
    $betaPath = Join-Path -Path $createInputDir -ChildPath 'beta.ini'
    Set-Content -LiteralPath $alphaPath -Value @('alpha line 1', 'alpha line 2') -Encoding ascii
    Set-Content -LiteralPath $betaPath -Value @('[section]', 'value=42') -Encoding ascii

    $createdMixPath = Join-Path -Path $artifactRoot -ChildPath 'sample.mix'
    $createResult = Invoke-WwMix -WwMixPath $wwmixExe -Arguments @(
        'c',
        $createdMixPath,
        $createInputDir,
        '--game', 'ra2',
        '-lmd'
    ) -Label 'create :: sample.mix'
    Assert-ExitCodeZero -Result $createResult
    Assert-FileExists -Path $createdMixPath

    $createdInfo = Invoke-WwMix -WwMixPath $wwmixExe -Arguments @('i', $createdMixPath) -Label 'info :: sample.mix'
    Assert-ExitCodeZero -Result $createdInfo
    Assert-Match -Text $createdInfo.Output -Pattern 'Game format:\s+RA2' -Message 'Created archive must use the RA2 format.'
    Assert-Match -Text $createdInfo.Output -Pattern 'Has checksum:\s+no' -Message 'Created archive should start without a checksum.'

    $createdList = Invoke-WwMix -WwMixPath $wwmixExe -Arguments @('l', $createdMixPath) -Label 'list :: sample.mix'
    Assert-ExitCodeZero -Result $createdList
    $createdEntries = Parse-WwMixListOutput -Output $createdList.Output
    Assert-True (@($createdEntries | Where-Object { $_.Name -eq 'alpha.txt' }).Count -eq 1) 'Created archive must contain alpha.txt.'
    Assert-True (@($createdEntries | Where-Object { $_.Name -eq 'beta.ini' }).Count -eq 1) 'Created archive must contain beta.ini.'
    Assert-True (@($createdEntries | Where-Object { $_.Name -eq 'local mix database.dat' }).Count -eq 1) 'Created archive must contain the local mix database.'

    $extractAllResult = Invoke-WwMix -WwMixPath $wwmixExe -Arguments @(
        'x',
        $createdMixPath,
        "-o$extractAllDir"
    ) -Label 'extract all :: sample.mix'
    Assert-ExitCodeZero -Result $extractAllResult
    Assert-FileTextEquals -ExpectedPath $alphaPath -ActualPath (Join-Path -Path $extractAllDir -ChildPath 'alpha.txt') -Message 'Extracted alpha.txt must match the source file.'
    Assert-FileTextEquals -ExpectedPath $betaPath -ActualPath (Join-Path -Path $extractAllDir -ChildPath 'beta.ini') -Message 'Extracted beta.ini must match the source file.'

    $alphaEntry = $createdEntries | Where-Object { $_.Name -eq 'alpha.txt' } | Select-Object -First 1
    Assert-True ($null -ne $alphaEntry) 'alpha.txt entry must be available for extract-by-id.'
    $extractByIdResult = Invoke-WwMix -WwMixPath $wwmixExe -Arguments @(
        'x',
        $createdMixPath,
        'alpha.txt',
        '--id', $alphaEntry.Id,
        "-o$extractByIdDir"
    ) -Label 'extract by id :: alpha.txt'
    Assert-ExitCodeZero -Result $extractByIdResult
    Assert-FileTextEquals -ExpectedPath $alphaPath -ActualPath (Join-Path -Path $extractByIdDir -ChildPath 'alpha.txt') -Message 'Extract-by-id must reproduce alpha.txt exactly.'

    $extractLmdResult = Invoke-WwMix -WwMixPath $wwmixExe -Arguments @(
        'x',
        $createdMixPath,
        'local mix database.dat',
        "-o$extractLmdDir"
    ) -Label 'extract :: local mix database.dat'
    Assert-ExitCodeZero -Result $extractLmdResult
    $lmdBinaryPath = Join-Path -Path $extractLmdDir -ChildPath 'local mix database.dat'
    Assert-FileExists -Path $lmdBinaryPath

    Write-TestSection "LMD Convert"
    $lmdJsonPath = Join-Path -Path $artifactRoot -ChildPath 'local mix database.json'
    $lmdRoundtripPath = Join-Path -Path $artifactRoot -ChildPath 'local mix database.roundtrip.dat'
    $lmdToJsonResult = Invoke-WwMix -WwMixPath $wwmixExe -Arguments @(
        'lmd',
        '--to-json',
        '--input', $lmdBinaryPath,
        '--output', $lmdJsonPath
    ) -Label 'lmd :: to-json'
    Assert-ExitCodeZero -Result $lmdToJsonResult
    Assert-FileExists -Path $lmdJsonPath

    $lmdJson = Read-JsonFile -Path $lmdJsonPath
    Assert-True ($lmdJson.game -eq 'ra2') 'LMD JSON must report the RA2 game type.'
    $lmdNames = @($lmdJson.entries | ForEach-Object { $_.name })
    Assert-True ($lmdNames -contains 'alpha.txt') 'LMD JSON must contain alpha.txt.'
    Assert-True ($lmdNames -contains 'beta.ini') 'LMD JSON must contain beta.ini.'

    $lmdToBinResult = Invoke-WwMix -WwMixPath $wwmixExe -Arguments @(
        'lmd',
        '--to-bin',
        '--input', $lmdJsonPath,
        '--output', $lmdRoundtripPath
    ) -Label 'lmd :: to-bin'
    Assert-ExitCodeZero -Result $lmdToBinResult
    Assert-FileExists -Path $lmdRoundtripPath

    Write-TestSection "Add / Delete / Checksum"
    $gammaPath = Join-Path -Path $artifactRoot -ChildPath 'gamma.bin'
    [System.IO.File]::WriteAllBytes($gammaPath, [byte[]](1..16))

    $addResult = Invoke-WwMix -WwMixPath $wwmixExe -Arguments @(
        'a',
        $createdMixPath,
        $gammaPath
    ) -Label 'add :: gamma.bin'
    Assert-ExitCodeZero -Result $addResult

    $listAfterAdd = Invoke-WwMix -WwMixPath $wwmixExe -Arguments @('l', $createdMixPath) -Label 'list :: sample.mix after add'
    Assert-ExitCodeZero -Result $listAfterAdd
    $entriesAfterAdd = Parse-WwMixListOutput -Output $listAfterAdd.Output
    Assert-True (@($entriesAfterAdd | Where-Object { $_.Name -eq 'gamma.bin' }).Count -eq 1) 'Added archive must contain gamma.bin.'

    $checksumAddResult = Invoke-WwMix -WwMixPath $wwmixExe -Arguments @(
        'a',
        $createdMixPath,
        '-checksum'
    ) -Label 'add checksum :: sample.mix'
    Assert-ExitCodeZero -Result $checksumAddResult

    $infoAfterChecksum = Invoke-WwMix -WwMixPath $wwmixExe -Arguments @('i', $createdMixPath) -Label 'info :: sample.mix after checksum'
    Assert-ExitCodeZero -Result $infoAfterChecksum
    Assert-Match -Text $infoAfterChecksum.Output -Pattern 'Has checksum:\s+yes' -Message 'Archive must report a checksum after adding one.'

    $deleteResult = Invoke-WwMix -WwMixPath $wwmixExe -Arguments @(
        'd',
        $createdMixPath,
        'gamma.bin'
    ) -Label 'delete :: gamma.bin'
    Assert-ExitCodeZero -Result $deleteResult

    $listAfterDelete = Invoke-WwMix -WwMixPath $wwmixExe -Arguments @('l', $createdMixPath) -Label 'list :: sample.mix after delete'
    Assert-ExitCodeZero -Result $listAfterDelete
    $entriesAfterDelete = Parse-WwMixListOutput -Output $listAfterDelete.Output
    Assert-True (@($entriesAfterDelete | Where-Object { $_.Name -eq 'gamma.bin' }).Count -eq 0) 'Deleted archive must no longer contain gamma.bin.'

    $checksumRemoveResult = Invoke-WwMix -WwMixPath $wwmixExe -Arguments @(
        'd',
        $createdMixPath,
        '-checksum'
    ) -Label 'remove checksum :: sample.mix'
    Assert-ExitCodeZero -Result $checksumRemoveResult

    $infoAfterChecksumRemoval = Invoke-WwMix -WwMixPath $wwmixExe -Arguments @('i', $createdMixPath) -Label 'info :: sample.mix after checksum removal'
    Assert-ExitCodeZero -Result $infoAfterChecksumRemoval
    Assert-Match -Text $infoAfterChecksumRemoval.Output -Pattern 'Has checksum:\s+no' -Message 'Archive must report no checksum after removal.'

    Write-TestSection "Encrypted Archive / Key"
    $secureInputDir = Join-Path -Path $artifactRoot -ChildPath 'secure-input'
    New-Item -ItemType Directory -Force -Path $secureInputDir | Out-Null
    $secureFilePath = Join-Path -Path $secureInputDir -ChildPath 'secure.txt'
    Set-Content -LiteralPath $secureFilePath -Value 'encrypted test payload' -Encoding ascii

    $encryptedMixPath = Join-Path -Path $artifactRoot -ChildPath 'encrypted.mix'
    $encryptedCreateResult = Invoke-WwMix -WwMixPath $wwmixExe -Arguments @(
        'c',
        $encryptedMixPath,
        $secureInputDir,
        '--game', 'ra2',
        '-encrypt',
        '-checksum'
    ) -Label 'create :: encrypted.mix'
    Assert-ExitCodeZero -Result $encryptedCreateResult
    Assert-FileExists -Path $encryptedMixPath

    $encryptedInfo = Invoke-WwMix -WwMixPath $wwmixExe -Arguments @('i', $encryptedMixPath) -Label 'info :: encrypted.mix'
    Assert-ExitCodeZero -Result $encryptedInfo
    Assert-Match -Text $encryptedInfo.Output -Pattern 'Encrypted:\s+yes' -Message 'Encrypted archive must report encrypted=yes.'

    $keyResult = Invoke-WwMix -WwMixPath $wwmixExe -Arguments @(
        'key',
        '--mix', $encryptedMixPath,
        '--game', 'ra2'
    ) -Label 'key :: encrypted.mix'
    Assert-ExitCodeZero -Result $keyResult
    Assert-Contains -Text $keyResult.Output -Expected 'Blowfish key:' -Message 'Key command must print the Blowfish key.'
    Assert-Contains -Text $keyResult.Output -Expected 'First decrypted block:' -Message 'Key command must print the first decrypted block.'

    Write-TestSection "GMD Convert / Edit"
    $sourceGmdPath = Join-Path -Path $repoRoot -ChildPath 'test_files\global mix database.dat'
    Assert-FileExists -Path $sourceGmdPath

    $gmdJsonPath = Join-Path -Path $artifactRoot -ChildPath 'global mix database.json'
    $gmdRoundtripPath = Join-Path -Path $artifactRoot -ChildPath 'global mix database.roundtrip.dat'
    $gmdRoundtripJsonPath = Join-Path -Path $artifactRoot -ChildPath 'global mix database.roundtrip.json'

    $gmdToJsonResult = Invoke-WwMix -WwMixPath $wwmixExe -Arguments @(
        'gmd',
        '--to-json',
        '--input', $sourceGmdPath,
        '--output', $gmdJsonPath
    ) -Label 'gmd :: to-json'
    Assert-ExitCodeZero -Result $gmdToJsonResult
    Assert-FileExists -Path $gmdJsonPath

    $gmdToBinResult = Invoke-WwMix -WwMixPath $wwmixExe -Arguments @(
        'gmd',
        '--to-bin',
        '--input', $gmdJsonPath,
        '--output', $gmdRoundtripPath
    ) -Label 'gmd :: to-bin'
    Assert-ExitCodeZero -Result $gmdToBinResult
    Assert-FileExists -Path $gmdRoundtripPath

    $gmdRoundtripToJsonResult = Invoke-WwMix -WwMixPath $wwmixExe -Arguments @(
        'gmd',
        '--to-json',
        '--input', $gmdRoundtripPath,
        '--output', $gmdRoundtripJsonPath
    ) -Label 'gmd :: roundtrip to-json'
    Assert-ExitCodeZero -Result $gmdRoundtripToJsonResult
    Assert-FileExists -Path $gmdRoundtripJsonPath

    $gmdJsonText = Get-Content -LiteralPath $gmdJsonPath -Raw
    $gmdRoundtripJsonText = Get-Content -LiteralPath $gmdRoundtripJsonPath -Raw
    Assert-True ($gmdJsonText -eq $gmdRoundtripJsonText) 'GMD JSON roundtrip must remain stable.'

    $additionsPath = Join-Path -Path $artifactRoot -ChildPath 'gmd-additions.csv'
    Set-Content -LiteralPath $additionsPath -Value 'zz_test_entry.ini,added by automated tests' -Encoding ascii
    $editedGmdPath = Join-Path -Path $artifactRoot -ChildPath 'global mix database.edited.dat'
    $editedGmdJsonPath = Join-Path -Path $artifactRoot -ChildPath 'global mix database.edited.json'

    $gmdEditResult = Invoke-WwMix -WwMixPath $wwmixExe -Arguments @(
        'gmd',
        '--input', $sourceGmdPath,
        '--additions', $additionsPath,
        '--output', $editedGmdPath,
        '--game', 'ra2'
    ) -Label 'gmd :: edit'
    Assert-ExitCodeZero -Result $gmdEditResult
    Assert-FileExists -Path $editedGmdPath

    $editedGmdToJsonResult = Invoke-WwMix -WwMixPath $wwmixExe -Arguments @(
        'gmd',
        '--to-json',
        '--input', $editedGmdPath,
        '--output', $editedGmdJsonPath
    ) -Label 'gmd :: edited to-json'
    Assert-ExitCodeZero -Result $editedGmdToJsonResult
    $editedGmdJsonText = Get-Content -LiteralPath $editedGmdJsonPath -Raw
    Assert-Contains -Text $editedGmdJsonText -Expected 'zz_test_entry.ini' -Message 'Edited GMD must contain the injected test entry.'

    $completed = $true
    Write-Host ""
    Write-Host 'Functional tests completed successfully.'
}
finally
{
    if ($completed -and -not $keepArtifacts)
    {
        Remove-TestArtifactDirectory -Path $artifactRoot
    }
    else
    {
        Write-Host ""
        Write-Host "Keeping test artifacts in $artifactRoot"
    }
}
