# ChronoSync

High-performance native Windows folder synchronizer with differential sync, preview, pruning with undo, and a dark-themed GUI.

## Features

- **Differential sync** — copies only new or changed files (size + timestamp comparison)
- **Atomic file replacement** — writes via `.chrono_tmp` then renames with `MOVEFILE_WRITE_THROUGH`
- **Prune with undo** — removed files go to `.chrono_trash` and can be restored
- **Symlink & junction preservation** — reparse points are recreated at the destination
- **Preview dialog** — virtual ListView with sort, search/filter, and CSV export
- **Include/exclude filters** — glob patterns (default excludes: `*.pkl`, `node_modules`, `*.zip`)
- **Sync profiles** — save/load source, destination, prune, and filter settings (`.chronosync` JSON)

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

## Profile format

Profiles are JSON files with extension `.chronosync`:

```json
{
  "version": 1,
  "name": "My Backup",
  "source": "C:\\Projects\\MyApp",
  "destination": "D:\\Backups\\MyApp",
  "prune": true,
  "includePatterns": [],
  "excludePatterns": ["*.pkl", "node_modules", "*.zip"]
}
```

## Roadmap

| Priority | Feature |
|----------|---------|
| Medium | SHA256 verification mode |
| Medium | Versioned backups |
| Medium | Multi-job queue |
| Low | Scheduled syncs |
| Low | Network share support |
| Low | Delta block-copying |

## License

MIT (see repository for details).
