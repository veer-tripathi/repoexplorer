# repoexplorer

A terminal-based **repository structure and visualization explorer**, written
entirely in modern C++ (C++20). It scans any code repository, analyzes its
structure, builds dependency and call graphs, and renders everything in your
terminal — plus exports reports and diagrams.

No Python, JavaScript, or external services. Just C++ and the standard library
(ncurses is optional, only for the interactive TUI).

---

## Features

- **Repository scanner** — recursive `std::filesystem` walk; detects files,
  directories, symlinks and hidden files; collects name, extension, size,
  modification time and relative path.
- **Directory tree visualization** — `├──`/`└──` tree with depth limits, ignore
  patterns and colored output.
- **Language detection** — C, C++, Python, Java, JavaScript, TypeScript, Rust,
  Go, Kotlin, C#, PHP, Shell, Markdown — with per-language file/line stats
  (total, blank, comment, code).
- **File content analysis** — extracts includes/imports, namespaces, classes,
  structs, functions/methods, enums and templates via regex-based static
  analysis.
- **Dependency graph** — file-to-file adjacency lists with incoming/outgoing
  queries and circular-dependency detection.
- **Module analysis** — most connected files, largest files/directories,
  circular dependencies, orphan files, unused headers.
- **Call graph approximation** — links defined functions to the functions they
  call.
- **Repository metrics** — totals, average file size, largest file/directory,
  language distribution.
- **Terminal visualizations** — ASCII trees, percentage bars and directory-size
  charts.
- **Interactive TUI** (ncurses) — Tree / File details / Dependencies /
  Statistics / Search views with keyboard navigation.
- **Search engine** — by file name, function, class or include.
- **Report generation** — `report.txt`, `report.md`, `report.json`.
- **Mermaid & Graphviz export** — `.mmd` and `.dot` for dependency and call
  graphs.

---

## Requirements

| | Required | Optional |
|---|---|---|
| **Compiler** | C++20: GCC 11+ / Clang 13+ / MinGW-w64 GCC 11+ | — |
| **Build tool** | none (single `g++` command works) | CMake 3.16+ |
| **Libraries** | C++ standard library only | ncurses (for the interactive TUI) |
| **Tests** | C++20 compiler (no framework) | — |

The tool and its tests depend on **nothing but the standard library** — no
downloads, no package managers. CMake and ncurses are only conveniences (a
nicer build, and the interactive TUI).

---

## Installing the toolchain

### Linux

```sh
# Arch Linux
sudo pacman -S base-devel cmake ncurses

# Debian / Ubuntu
sudo apt update && sudo apt install build-essential cmake libncurses-dev

# Fedora
sudo dnf install gcc-c++ cmake ncurses-devel
```

### macOS

```sh
xcode-select --install          # Clang toolchain
brew install cmake ncurses      # optional (Homebrew)
```

### Windows

Install a MinGW-w64 GCC toolchain. The simplest is
[**w64devkit**](https://github.com/skeeto/w64devkit): download, unzip, and add
its `bin/` folder to your `PATH` (or run commands from the w64devkit shell).
Verify with `g++ --version`.

CMake (optional) via winget: `winget install Kitware.CMake` — then restart the
shell. ncurses is not readily available on Windows, so the TUI falls back to a
message there; every other command works fully.

---

## Build

### Linux / macOS — CMake (recommended)

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

Binary: `build/repoexplorer`.

### Linux / macOS — single command (no CMake)

```sh
g++ -std=c++20 -O2 -Isrc $(find src -name '*.cpp') -o repoexplorer
```

### Windows — PowerShell, direct g++ (no CMake)

```powershell
$files = (Get-ChildItem -Recurse -Filter *.cpp -Path src).FullName
g++ -std=c++20 -O2 -Wall -Isrc $files -o repoexplorer.exe
```

Produces `repoexplorer.exe` in the project root.

### Windows — CMake + MinGW

```powershell
cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

If CMake cannot find the compiler, point it at one explicitly:
`-DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++`.

---

## Running the tests

The tests are a single dependency-free program (no framework, no downloads) —
it just runs checks and returns non-zero on failure.

With CMake:

```sh
cmake -S . -B build
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Or compile and run it directly (Linux/macOS):

```sh
g++ -std=c++20 -Isrc test/test.cpp \
    src/utils.cpp src/scanner.cpp src/parser.cpp src/graph.cpp \
    src/analyzer.cpp src/view.cpp src/exporter.cpp src/tui.cpp \
    -o tests && ./tests
```

On Windows PowerShell:

```powershell
$mods = "utils","scanner","parser","graph","analyzer","view","exporter","tui"
$src = $mods | ForEach-Object { "src/$_.cpp" }
g++ -std=c++20 -Isrc test/test.cpp $src -o tests.exe; .\tests.exe
```

---

## Usage

```sh
repoexplorer scan   <path>           # quick summary
repoexplorer tree   <path>           # directory tree
repoexplorer stats  <path>           # metrics + language distribution
repoexplorer deps   <path>           # dependency graph
repoexplorer graph  <path>           # call graph
repoexplorer search <path> <query>   # search symbols
repoexplorer export <path> --out out # write reports + diagrams
repoexplorer tui    <path>           # interactive explorer
```

Options: `--depth N`, `--out DIR`, `--no-color`.

On Windows, invoke the binary as `.\repoexplorer.exe` (e.g.
`.\repoexplorer.exe stats .`). Paths accept both `\` and `/` separators. If your
console shows raw ANSI codes, add `--no-color` (Windows Terminal renders colors
without it).

### Examples

```sh
repoexplorer tree .
repoexplorer stats .
repoexplorer deps .
repoexplorer export . --out ./reports
```

---

## Project layout

One file pair (`.hpp` + `.cpp`) per module — about 1,600 lines total, no
external dependencies.

```
src/
├── utils.hpp/.cpp     # strings, color, ASCII bars
├── scanner.hpp/.cpp   # filesystem walk + data models (FileNode, DirNode, Repository)
├── parser.hpp/.cpp    # language detection, line counting, static analysis
├── graph.hpp/.cpp     # dependency & call graphs
├── analyzer.hpp/.cpp  # metrics, module insights, search
├── view.hpp/.cpp      # terminal visualizations (tree, bars, graph lists)
├── exporter.hpp/.cpp  # txt/md/json, mermaid, graphviz
├── tui.hpp/.cpp       # optional ncurses UI
└── main.cpp           # CLI parsing + command dispatch
test/
└── test.cpp           # dependency-free test runner
```

See [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) for the full design, and
[docs/FILES.md](docs/FILES.md) for a detailed explanation of **every file and
its logic**.

---

## Troubleshooting

- **`'std::filesystem' has not been declared` / linker errors about filesystem**
  — your compiler is too old. Use GCC 11+ or Clang 13+. On some GCC 8–9 setups
  you must also add `-lstdc++fs` to the link line.
- **`cmake: command not found`** — install CMake (see above) or use the single
  `g++` command, which needs no CMake.
- **`g++: command not found` on Windows** — the MinGW `bin/` folder is not on
  your `PATH`. Add it, or run the build from the w64devkit shell.
- **Tree/colors show garbage like `←[34m`** — your terminal isn't interpreting
  ANSI codes. Add `--no-color`, or use Windows Terminal.
- **The `tui` command just prints a message** — ncurses wasn't found at build
  time (always the case on Windows). Build on Linux/macOS with ncurses installed
  to get the interactive UI; the other commands work everywhere.

---

## Notes & limitations

The parser is an **approximate** static analyzer based on regular expressions,
not a full compiler front end. It is intentionally fast and language-tolerant;
results (especially the call graph) are best-effort, which is the right
trade-off for repository-scale exploration.
