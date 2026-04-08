# Tests

PowerShell-based integration tests for `wwmix`.

These scripts:

- find the Red Alert 2 installation path from the Windows registry by default
- enumerate every `.mix` and `.mmx` archive in that installation for read-only checks
- exercise the main `wwmix` workflows in a temporary sandbox without modifying the original game files

## Scripts

- `Run-WwMixTests.ps1` - runs the full suite
- `Test-WwMix.ReadOnly.ps1` - runs `list`, `info`, and `key` checks against installed RA2 archives
- `Test-WwMix.Functional.ps1` - runs `guess`, `create`, `extract`, `add`, `delete`, `checksum`, `gmd`, `lmd`, and `key` tests in a temporary workspace
- `WwMix.TestHelpers.ps1` - shared helpers

## Usage

From the repository root:

```powershell
powershell -ExecutionPolicy Bypass -File .\tests\Run-WwMixTests.ps1
```

Or use the batch wrapper that builds `wwmix` when needed and then runs the tests:

```bat
.\test_vs2022_msvc.bat
```

Use a specific build:

```powershell
powershell -ExecutionPolicy Bypass -File .\tests\Run-WwMixTests.ps1 -Configuration Release
```

Override the detected paths if needed:

```powershell
powershell -ExecutionPolicy Bypass -File .\tests\Run-WwMixTests.ps1 `
  -WwMixPath .\build\bin\Release\wwmix.exe `
  -InstallPath "D:\Programs\SteamedHams\steamapps\common\Command & Conquer Red Alert II"
```

Keep temporary artifacts for inspection:

```powershell
powershell -ExecutionPolicy Bypass -File .\tests\Run-WwMixTests.ps1 -KeepArtifacts
```
