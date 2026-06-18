# ChronoSync

High-performance native Windows folder synchronizer with differential sync, preview, pruning with undo, and a dark-themed GUI.

## Features

- **Differential sync** — copies only new or changed files (timestamp or SHA256 comparison)
- **Atomic block-compare copy** — for same-size files, compares 4MB blocks and rebuilds the destination atomically via `.chrono_tmp`; `deltaBytesWritten` counts only changed block data (unchanged blocks are copied from the existing destination into the temp file)
- **SHA256 verification** — optional content-hash compare mode and verify-after-copy
- **Atomic file replacement** — writes via `.chrono_tmp` then renames with `MOVEFILE_WRITE_THROUGH`
- **Prune with undo** — removed files archived to `.chrono_backups/<timestamp>/` (or legacy `.chrono_trash`)
- **Versioned backups** — keeps the last N prune snapshots (default: 5)
- **Multi-job queue** — queue multiple source→destination jobs, save/load `.chronoqueue` files, run sequentially
- **Scheduled syncs** — create daily/weekly Windows Task Scheduler jobs from the GUI or CLI
- **Network share support** — UNC path detection, connection retry, and copy retries on transient errors
- **Symlink & junction preservation** — reparse points are recreated at the destination
- **Preview dialog** — virtual ListView with sort, search/filter, CSV export, and right-click **Show in File Explorer**
- **Include/exclude filters** — glob patterns (default excludes: `*.pkl`, `node_modules`, `*.zip`)
- **Sync profiles** — save/load source, destination, prune, filters, and compare options (`.chronosync` JSON)

## Build

Requires CMake 3.15+, a C++20 compiler (MSVC or MinGW), and Windows.

```bat
build.bat
```

Or with CMake directly:

```bat
cmake -B build -G "Visual Studio 17 2022"
cmake --build build --config Release
```

Outputs: `ChronoSync.exe`, `ChronoSyncTests.exe`

## Run tests

```bat
ChronoSyncTests.exe
```

## CLI (headless)

ChronoSync can run without the GUI for automation and scheduled tasks:

```bat
ChronoSync.exe --sync profile.chronosync
ChronoSync.exe --queue jobs.chronoqueue
ChronoSync.exe --schedule-create profile.chronosync --daily --time 02:00 --name NightlyBackup
ChronoSync.exe --schedule-create profile.chronosync --weekly --day MON --time 03:30 --name WeeklyBackup
ChronoSync.exe --schedule-remove NightlyBackup
ChronoSync.exe --help
```

Scheduled tasks invoke `ChronoSync.exe --sync <profile>` at the configured time.

## Profile format

Profiles are JSON files with extension `.chronosync`:

```json
{
  "version": 1,
  "name": "My Backup",
  "source": "C:\\Projects\\MyApp",
  "destination": "D:\\Backups\\MyApp",
  "prune": true,
  "sha256Compare": false,
  "verifyAfterCopy": false,
  "versionedBackups": true,
  "maxBackupVersions": 5,
  "deltaBlockCopy": false,
  "includePatterns": [],
  "excludePatterns": ["*.pkl", "node_modules", "*.zip"]
}
```

`deltaBlockCopy` enables atomic block-compare copy (see feature list above). The JSON key name is historical; the operation always rebuilds via `.chrono_tmp` rather than patching blocks in place.

## Queue format

Job queues use `.chronoqueue` JSON files with a `jobs` array. Each job mirrors the profile fields above.

## License

MIT (see repository for details).
