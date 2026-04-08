Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Get-WwMixRepoRoot
{
    return Split-Path -Path $PSScriptRoot -Parent
}

function Resolve-WwMixExecutable
{
    param(
        [string]$WwMixPath,
        [string]$Configuration = 'Release'
    )

    if (-not [string]::IsNullOrWhiteSpace($WwMixPath))
    {
        if (-not (Test-Path -LiteralPath $WwMixPath -PathType Leaf))
        {
            throw "Specified wwmix executable was not found: $WwMixPath"
        }

        return (Resolve-Path -LiteralPath $WwMixPath).Path
    }

    $repoRoot = Get-WwMixRepoRoot
    $candidates = @(
        (Join-Path -Path $repoRoot -ChildPath "build\bin\$Configuration\wwmix.exe"),
        (Join-Path -Path $repoRoot -ChildPath "build\bin\Release\wwmix.exe"),
        (Join-Path -Path $repoRoot -ChildPath "bin\wwmix.exe")
    )

    foreach ($candidate in $candidates)
    {
        if (Test-Path -LiteralPath $candidate -PathType Leaf)
        {
            return (Resolve-Path -LiteralPath $candidate).Path
        }
    }

    throw "Could not locate wwmix.exe. Build the project first or pass -WwMixPath."
}

function Resolve-InstallDirectory
{
    param(
        [AllowNull()]
        [AllowEmptyString()]
        [string]$PathValue
    )

    if ([string]::IsNullOrWhiteSpace($PathValue))
    {
        return $null
    }

    if (Test-Path -LiteralPath $PathValue -PathType Container)
    {
        return (Resolve-Path -LiteralPath $PathValue).Path
    }

    if (Test-Path -LiteralPath $PathValue -PathType Leaf)
    {
        return (Resolve-Path -LiteralPath (Split-Path -Path $PathValue -Parent)).Path
    }

    if ($PathValue.EndsWith('.exe', [System.StringComparison]::OrdinalIgnoreCase))
    {
        $parent = Split-Path -Path $PathValue -Parent
        if (Test-Path -LiteralPath $parent -PathType Container)
        {
            return (Resolve-Path -LiteralPath $parent).Path
        }
    }

    return $null
}

function Get-RedAlert2InstallPath
{
    param(
        [string]$InstallPath
    )

    if (-not [string]::IsNullOrWhiteSpace($InstallPath))
    {
        $resolvedPath = Resolve-InstallDirectory -PathValue $InstallPath
        if ($null -eq $resolvedPath)
        {
            throw "Specified Red Alert 2 install path was not found: $InstallPath"
        }

        return $resolvedPath
    }

    $registryCandidates = @(
        @{ Key = 'HKLM:\Software\WOW6432Node\Westwood\Red Alert 2'; Values = @('InstallPath', 'Path', 'FolderPath') },
        @{ Key = 'HKLM:\Software\Westwood\Red Alert 2'; Values = @('InstallPath', 'Path', 'FolderPath') },
        @{ Key = 'HKCU:\Software\Westwood\Red Alert 2'; Values = @('InstallPath', 'Path', 'FolderPath') },
        @{ Key = 'HKLM:\Software\WOW6432Node\Electronic Arts\EA Games\Command and Conquer Red Alert II'; Values = @('InstallPath', 'Path', 'FolderPath') },
        @{ Key = 'HKLM:\Software\Electronic Arts\EA Games\Command and Conquer Red Alert II'; Values = @('InstallPath', 'Path', 'FolderPath') },
        @{ Key = 'HKCU:\Software\Electronic Arts\EA Games\Command and Conquer Red Alert II'; Values = @('InstallPath', 'Path', 'FolderPath') }
    )

    foreach ($candidate in $registryCandidates)
    {
        if (-not (Test-Path -LiteralPath $candidate.Key))
        {
            continue
        }

        $properties = Get-ItemProperty -LiteralPath $candidate.Key
        foreach ($valueName in $candidate.Values)
        {
            if (-not ($properties.PSObject.Properties.Name -contains $valueName))
            {
                continue
            }

            $resolvedPath = Resolve-InstallDirectory -PathValue ([string]$properties.$valueName)
            if ($null -ne $resolvedPath)
            {
                return $resolvedPath
            }
        }
    }

    throw 'Could not locate Command & Conquer: Red Alert 2 in the registry. Pass -InstallPath explicitly.'
}

function Get-Ra2ArchiveFiles
{
    param(
        [string]$InstallPath
    )

    $archives = Get-ChildItem -LiteralPath $InstallPath -Recurse -File |
        Where-Object {
            $_.Extension.Equals('.mix', [System.StringComparison]::OrdinalIgnoreCase) -or
            $_.Extension.Equals('.mmx', [System.StringComparison]::OrdinalIgnoreCase)
        } |
        Sort-Object -Property FullName

    return @($archives)
}

function New-TestArtifactDirectory
{
    param(
        [string]$Name
    )

    $repoRoot = Get-WwMixRepoRoot
    $artifactsRoot = Join-Path -Path $repoRoot -ChildPath 'tests\.artifacts'
    New-Item -ItemType Directory -Force -Path $artifactsRoot | Out-Null

    $timestamp = Get-Date -Format 'yyyyMMdd-HHmmss'
    $directory = Join-Path -Path $artifactsRoot -ChildPath "$Name-$timestamp-$PID"
    New-Item -ItemType Directory -Force -Path $directory | Out-Null
    return $directory
}

function Remove-TestArtifactDirectory
{
    param(
        [string]$Path
    )

    if (-not [string]::IsNullOrWhiteSpace($Path) -and
        (Test-Path -LiteralPath $Path -PathType Container))
    {
        Remove-Item -LiteralPath $Path -Recurse -Force
    }
}

function Format-WwMixCommand
{
    param(
        [string]$WwMixPath,
        [string[]]$Arguments
    )

    $parts = @($WwMixPath) + $Arguments
    $escaped = foreach ($part in $parts)
    {
        if ($part -match '[\s"]')
        {
            '"' + ($part -replace '"', '\"') + '"'
        }
        else
        {
            $part
        }
    }

    return ($escaped -join ' ')
}

function Invoke-WwMix
{
    param(
        [string]$WwMixPath,
        [string[]]$Arguments,
        [string]$Label
    )

    Write-Host ""
    Write-Host "==> $Label"
    Write-Host (Format-WwMixCommand -WwMixPath $WwMixPath -Arguments $Arguments)

    $output = (& $WwMixPath @Arguments 2>&1 | Out-String).TrimEnd()
    $exitCode = $LASTEXITCODE

    if (-not [string]::IsNullOrWhiteSpace($output))
    {
        Write-Host $output
    }

    return [pscustomobject]@{
        Label = $Label
        Arguments = @($Arguments)
        ExitCode = $exitCode
        Output = $output
    }
}

function Assert-True
{
    param(
        [bool]$Condition,
        [string]$Message
    )

    if (-not $Condition)
    {
        throw "Assertion failed: $Message"
    }
}

function Assert-ExitCodeZero
{
    param(
        $Result
    )

    if ($Result.ExitCode -ne 0)
    {
        throw "Command failed with exit code $($Result.ExitCode): $($Result.Label)`n$($Result.Output)"
    }
}

function Assert-Contains
{
    param(
        [string]$Text,
        [string]$Expected,
        [string]$Message
    )

    if (-not $Text.Contains($Expected))
    {
        throw "Assertion failed: $Message`nExpected to find: $Expected`nActual output:`n$Text"
    }
}

function Assert-Match
{
    param(
        [string]$Text,
        [string]$Pattern,
        [string]$Message
    )

    if ($Text -notmatch $Pattern)
    {
        throw "Assertion failed: $Message`nExpected pattern: $Pattern`nActual output:`n$Text"
    }
}

function Assert-FileExists
{
    param(
        [string]$Path
    )

    if (-not (Test-Path -LiteralPath $Path -PathType Leaf))
    {
        throw "Expected file was not found: $Path"
    }
}

function Assert-DirectoryExists
{
    param(
        [string]$Path
    )

    if (-not (Test-Path -LiteralPath $Path -PathType Container))
    {
        throw "Expected directory was not found: $Path"
    }
}

function Assert-FileTextEquals
{
    param(
        [string]$ExpectedPath,
        [string]$ActualPath,
        [string]$Message
    )

    $expected = Get-Content -LiteralPath $ExpectedPath -Raw
    $actual = Get-Content -LiteralPath $ActualPath -Raw
    if ($expected -ne $actual)
    {
        throw "Assertion failed: $Message`nExpected file: $ExpectedPath`nActual file: $ActualPath"
    }
}

function Assert-FileBytesEqual
{
    param(
        [string]$ExpectedPath,
        [string]$ActualPath,
        [string]$Message
    )

    $expectedHash = (Get-FileHash -LiteralPath $ExpectedPath -Algorithm SHA256).Hash
    $actualHash = (Get-FileHash -LiteralPath $ActualPath -Algorithm SHA256).Hash
    if ($expectedHash -ne $actualHash)
    {
        throw "Assertion failed: $Message`nExpected file: $ExpectedPath`nActual file: $ActualPath"
    }
}

function Parse-WwMixListOutput
{
    param(
        [string]$Output
    )

    $entries = New-Object System.Collections.Generic.List[object]
    foreach ($line in ($Output -split "\r?\n"))
    {
        if ($line -match '^\s*(Name|----)\b')
        {
            continue
        }

        if ($line -match '^\s*(?<name>.+?)\s+(?<id>[0-9a-fA-F]{8})\s+(?<offset>\d+)\s+(?<size>\d+)\s*$')
        {
            $entries.Add([pscustomobject]@{
                Name = $Matches.name.Trim()
                Id = $Matches.id.ToLowerInvariant()
                Offset = [uint32]$Matches.offset
                Size = [uint32]$Matches.size
            })
        }
    }

    return @($entries.ToArray())
}

function Read-JsonFile
{
    param(
        [string]$Path
    )

    return (Get-Content -LiteralPath $Path -Raw | ConvertFrom-Json)
}

function Write-TestSection
{
    param(
        [string]$Title
    )

    Write-Host ""
    Write-Host "==== $Title ===="
}
