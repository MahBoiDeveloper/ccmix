# ccmix

Command-line tools for reading, creating, and editing Westwood Studios `.mix` archives.

`ccmix` started from the `tsunmix` codebase and has grown into a small toolkit for classic Command & Conquer archive workflows. The repository currently ships three executables:

- `wwmix` - the unified front end for archive operations, global mix database editing, and encrypted-header key inspection
- `ccmix` - the legacy archive-focused command-line tool
- `gmdedit` - the small compatibility utility for batch-appending entries to `global mix database.dat`

For new workflows, `wwmix` is the recommended entry point.

## About

This project is focused on the MIX archive formats used by classic Westwood games, especially the Tiberian Dawn, Red Alert, Tiberian Sun, and Red Alert 2 generations.

It can:

- list archive contents
- extract files by name or ID
- create new archives from directories
- add or delete archive entries
- write optional encrypted headers
- write optional SHA-1 checksums
- include or inspect XCC-style MIX database data

The repository also includes the `wwmix gmd` and `wwmix key` workflows, which fold the older `gmdedit` and `mixkey` functionality into the newer unified executable.

## Building Manually

Clone the repository with submodules so the vendored Crypto++ dependency is available:

```powershell
git clone --recursive <repository-url>
cd ccmix
```

### Recommended Windows Build

The easiest way to build on Windows is the provided Visual Studio 2022 helper script:

```powershell
.\build_vs2022_msvc.bat Release x64
```

Build outputs are placed in `build\bin\Release`.

### Manual CMake Build

If you want to drive the build yourself, use CMake directly:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

## Requirements

- CMake 3.10 or newer
- A C++23-capable compiler
- Visual Studio 2022 with MSVC for the documented Windows workflow
- Crypto++ available either through the vendored submodule in `libs/cryptopp` or your own external setup

## Main Project Features

- `wwmix` provides a single executable for archive commands, GMD editing, and encrypted-header inspection.
- `ccmix` remains available for the original switch-based archive workflow.
- `gmdedit` remains available as a minimal compatibility tool for batch-appending entries to `global mix database.dat`.
- MIX archives can be created with optional local mix databases, encrypted headers, and checksums where the format supports them.
- The repository keeps a standalone `src/wwmix` implementation, so the unified tool is built independently from the older `ccmix` sources.

## Quick Examples

List an archive:

```powershell
wwmix mix l CONQUER.MIX
```

Extract an archive into a folder:

```powershell
wwmix mix x CONQUER.MIX -oout
```

Create a Red Alert 2 style archive with a local database and checksum:

```powershell
wwmix mix c custom.mix data -gra2 -lmd -checksum
```

Append entries to a global mix database from a CSV-style additions file:

```powershell
wwmix gmd --input "global mix database.dat" --additions additions.csv --output updated.dat --game td
```

Inspect the encryption-related key data for a MIX file:

```powershell
wwmix key --mix CACHE.MIX --game ra
```

Legacy compatibility examples:

```powershell
ccmix --list --mix CONQUER.MIX
gmdedit "global mix database.dat" additions.csv updated.dat
```

## Credits

- `tsunmix` by ivosh-l for the original foundation this project was built on
- `ccmix` by OmniBlade for updating `tsunmix`
- Olaf van der Spek for years of Westwood file format reverse-engineering and XCC tooling
- The Crypto++ authors for the cryptographic primitives used by the project

## Legal

This is an unofficial fan-made utility project. It is not affiliated with or endorsed by Electronic Arts, Westwood Studios, Petroglyph, or any other rights holder connected to Command & Conquer.

Any game data, trademarks, archive contents, and related assets remain the property of their respective owners.

## License

This project is licensed under the GNU General Public License v3.0. See [LICENSE.md](LICENSE.md) for the full text.
