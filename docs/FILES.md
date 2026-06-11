# File-by-File Reference

The project is one `.hpp`/`.cpp` pair per module, in a single `re` namespace.
Data flows: **scanner → parser → graph → analyzer → view/exporter/tui**, with
`main.cpp` wiring it together. ~1,600 lines, standard library only.

For each file: its **purpose**, the **key types/functions**, and the **logic**.

---

## `CMakeLists.txt`
Builds the `repoexplorer` executable from `main.cpp` + the eight module `.cpp`
files. Auto-detects ncurses with `find_package(Curses)`: if found it defines
`REPOEXPLORER_HAS_NCURSES=1` and links it, otherwise the TUI compiles its
fallback. Also builds `repoexplorer_tests` from `test/test.cpp` + the same module
files and registers it with CTest. No external dependencies, no downloads.

---

## `src/utils.hpp` / `utils.cpp`
**Purpose:** small helpers used everywhere — strings, color, ASCII bars.
**Key functions & logic:**
- `trim` — returns a `string_view` sub-range between the first/last non-space (no
  allocation).
- `toLower`, `split`, `endsWith`, `startsWith` — straightforward string ops.
- `humanSize(bytes)` — divides by 1024 while possible, picks a unit, formats with
  `snprintf`.
- `colorEnabled(bool)` + `paint(text, Color)` — wraps text in an ANSI escape and
  reset, or returns it unchanged when color is globally disabled (`--no-color`,
  pipes).
- `bar(ratio, width)` — clamps ratio to `[0,1]` and builds `[###   ]` by rounding
  `ratio * width` filled cells.

## `src/scanner.hpp` / `scanner.cpp`
**Purpose:** turn a path on disk into an in-memory tree + flat index.
**Key types:** `FileNode` (name, extension, path, size, mtime, symlink/hidden
flags), `DirNode` (owns `unique_ptr` subdirs + `FileNode` files), `Repository`
(owns the root + a `path → FileNode*` index; **move-only** so a big tree is never
copied), `ScanOptions` (maxDepth, includeHidden, followSymlinks).
**Logic:**
- `scan(root, opt)` sets up the root node, calls the recursive `scanDir`, then
  `buildIndex()`, and moves the repository out.
- `scanDir` iterates with `directory_iterator` (`skip_permission_denied` +
  `error_code`, so unreadable dirs are skipped silently). It applies the hidden
  option and an `ignored()` check (`.git`, `build`, `node_modules`, object/binary
  extensions, …), recurses into directories (depth guard implements `--depth`),
  and fills a `FileNode` for files (lowercased extension, `file_size`, and a
  portable `file_time_type → unix seconds` conversion). Children are sorted
  alphabetically for stable output.
- `Repository::buildIndex` / `indexDir` recursively fill the flat map and count
  directories; `find(path)` is a single O(1) lookup.

## `src/parser.hpp` / `parser.cpp`
**Purpose:** per-file language detection, line counting, and regex static
analysis.
**Key types:** `Lang` enum, `LineStats`, `FuncInfo`, `ClassInfo`,
`FileAnalysis` (path, language, line stats, includes, namespaces, classes,
functions, enums, template count, calls), and the `AnalysisSet` alias
(`unordered_map<path, FileAnalysis>`).
**Logic:**
- `langFromExt` — static extension→language table; `langName`, `isSource` (drops
  Unknown/Markdown).
- `countLines` — line-by-line classification with one `inBlock` boolean for
  multi-line comments: empty→blank; inside a block→comment (clears on the close
  marker); starts with line/block marker→comment; else→code.
- `parseText(content, path, lang)` — runs `countLines`, then walks the text line
  by line to capture includes/imports (C/C++ `#include`, Python `import`/`from`,
  JS/TS `import … from`) and C/C++ function definitions (with line numbers),
  collecting called identifiers on every line. A keyword set filters out `if(`,
  `for(`, etc. so control flow isn't mistaken for functions/calls. For C/C++ it
  then scans the whole buffer for namespaces, classes/structs (skipping
  `enum class X`), enums, and counts `template<`.
- `parseRepository(repo)` — reads every recognized source file from disk and
  parses it into the `AnalysisSet`.

## `src/graph.hpp` / `graph.cpp`
**Purpose:** dependency and call graphs as adjacency lists.
**Logic:**
- `DependencyGraph::build` indexes repo files by **basename** so
  `#include "parser.hpp"` resolves to `src/parser.hpp`; records `out_` (file →
  includes) and `in_` (included → includers); drops unresolved/external headers
  and self-edges; dedups each list. `outgoing`/`incoming`/counts are map lookups
  returning a shared empty vector when absent.
- `DependencyGraph::cycles` — DFS with a recursion stack and `visited`/`onStack`
  sets; a back edge to an on-stack node yields a cycle (sliced from the stack).
- `CallGraph::build` — pass 1 collects all defined function names; pass 2 links
  each function defined in a file to the known callees in that file (library
  calls ignored). File-granular, deduped.

## `src/analyzer.hpp` / `analyzer.cpp`
**Purpose:** aggregate metrics, module insights, and symbol search.
**Logic:**
- `computeMetrics` — `walk()` recurses the tree for total size, largest file, and
  the directory with the most files; rolls each analysis into per-language
  `LangStats`; computes source-file count, LOC, and average file size.
  `Metrics::byFiles()` sorts languages by file count.
- `analyzeModules` — per graph node computes degree (in+out) → `mostConnected`;
  zero-degree nodes → `orphans`; headers with no incoming edges → `unusedHeaders`;
  largest files (by size) and largest dirs (by file count); cycles from the
  graph. Each ranked list is sorted and truncated to `topN`.
- `search` — case-insensitive substring match over every file path, function,
  class, and include, returning typed `Hit`s.

## `src/view.hpp` / `view.cpp`
**Purpose:** terminal visualizations (pure output).
**Logic:**
- `printTree` — recursive `printDir` prints `├── `/`└── ` connectors and extends
  the prefix with `│   ` / spaces so vertical guides align; files colored by kind;
  depth guard for `--depth`.
- `printLanguages` — percentage bars per language, names padded to equal width.
- `printDirSizes` — gathers `(dir, fileCount)`, sorts, top N, bars scaled to the
  largest.
- `printDependencies` / `printCallGraph` — each node followed by its edges as an
  indented `├──` / `├──>` list.

## `src/exporter.hpp` / `exporter.cpp`
**Purpose:** write reports and diagram files.
**Logic:** `exportAll` writes `report.txt` (aligned text), `report.md` (Markdown
tables), `report.json` (hand-rolled JSON with a local `jsonEscape`),
`dependencies.mmd` / `callgraph.mmd` (Mermaid `graph TD`, with an `IdMap` giving
each label an `n0`/`n1` id), and `dependencies.dot` / `callgraph.dot` (Graphviz
`digraph`).

## `src/tui.hpp` / `tui.cpp`
**Purpose:** optional ncurses explorer, compiled in two variants.
**Logic (with ncurses):** `flatten()` turns the tree into a scrollable list;
`runTui` draws a header/footer plus the active view (Tree with an Enter-toggled
file-details panel, Statistics, Dependencies, live Search) and handles keys
(`1/2/3` switch views, `/` search, arrows, Enter, `q` quit). Restores the
terminal with `endwin()`.
**Logic (without ncurses):** prints a message saying the TUI needs ncurses and to
use the other commands — so the project always builds.

## `src/main.cpp`
**Purpose:** CLI parsing and command dispatch — the only file that knows the
whole pipeline.
**Logic:** separates flags (`--depth`, `--out`, `--no-color`) from positionals
(command, path, query). A local `build(path, depth)` runs the pipeline once
(`scan → parseRepository → DependencyGraph.build + CallGraph.build`) into a
`Pipeline` bundle; each command (`scan/tree/stats/deps/graph/search/export/tui`)
reuses it. Wrapped in try/catch returning exit code 2 on a fatal error.

## `test/test.cpp`
**Purpose:** dependency-free test runner.
**Logic:** a `CHECK(cond)` macro increments counters and prints failures with
file/line. Three groups: **parser** (extension mapping, line categories,
include/namespace/class/enum/template/function/call extraction — including that
`enum class` is not counted as a class), **graph** (basename include resolution,
in/out counts, cycle detection), and **scanner + integration** (temp repo on
disk: file/dir discovery, hidden flag, full pipeline, dependency edge, metrics,
and the depth limit). `main` returns non-zero if any check fails.

---

## How it all connects (one sentence)

`scan()` builds a `Repository`; `parseRepository()` turns its files into an
`AnalysisSet`; `DependencyGraph`/`CallGraph` derive edges; `computeMetrics`,
`analyzeModules`, and `search` summarize it; and `view`, `exportAll`, and the TUI
present it — with `main.cpp` wiring the stages together.
