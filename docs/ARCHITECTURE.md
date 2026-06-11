# Architecture

`repoexplorer` is a **one-directional pipeline**. Each stage consumes typed data
from the previous stage and produces typed data for the next; no stage mutates
its inputs. Every module is one `.hpp`/`.cpp` pair in a single `re` namespace,
and depends only on the shared data structs — not on each other.

This keeps the design aligned with SOLID without ceremony:

- **Single Responsibility** — each module does one thing (scan, parse, graph,
  analyze, present, export).
- **Open/Closed** — adding a language is one table entry; adding an export format
  is one function; nothing existing changes.
- **Low coupling** — `main.cpp` is the only place that knows the whole pipeline;
  the modules communicate purely through plain data.

```
 path ─▶ scan() ─▶ Repository ─▶ parseRepository() ─▶ AnalysisSet ─┐
                                                                   │
        ┌───────────────────────────────────────────────────────── ┘
        ▼
   DependencyGraph + CallGraph
        ▼
   computeMetrics + analyzeModules + search
        ▼
   view (tree/bars/lists) / exportAll (txt,md,json,mmd,dot) / runTui
```

## Modules (one file pair each)

| File | Responsibility | Key types / functions |
|------|----------------|-----------------------|
| `utils` | strings, ANSI color, ASCII bars | `trim`, `paint`, `bar` |
| `scanner` | `std::filesystem` walk + data models | `FileNode`, `DirNode`, `Repository`, `scan()` |
| `parser` | language detection, line counting, regex extraction | `Lang`, `FileAnalysis`, `parseText`, `parseRepository` |
| `graph` | dependency & call graphs (adjacency lists) | `DependencyGraph`, `CallGraph` |
| `analyzer` | metrics, module insights, search | `Metrics`, `ModuleReport`, `analyzeModules`, `search` |
| `view` | terminal visualizations | `printTree`, `printLanguages`, `printDependencies` |
| `exporter` | reports + diagram files | `exportAll` |
| `tui` | optional ncurses explorer | `runTui` |
| `main` | CLI parsing + dispatch | `main`, `build()` |

## Data models

| Type | Role |
|------|------|
| `Repository` | Aggregate root: owns the tree + flat `path → FileNode*` index |
| `DirNode` / `FileNode` | The tree (move-only, `unique_ptr` children) |
| `FileAnalysis` / `AnalysisSet` | Per-file parse results and their map |
| `DependencyGraph` / `CallGraph` | Adjacency lists |
| `Metrics` / `LangStats` | Aggregate numbers |
| `ModuleReport` / `Ranked` | Insight lists |
| `FuncInfo` / `ClassInfo` | Extracted entities |

## Data flow for `repoexplorer export .`

1. `main` parses argv and calls `build(path, depth)`.
2. `scan()` → `Repository` (tree + index).
3. `parseRepository()` → `AnalysisSet`.
4. `DependencyGraph::build` + `CallGraph::build`.
5. `computeMetrics` + `analyzeModules`.
6. `exportAll` writes the reports and the Mermaid/Graphviz files.

## Performance notes

- The tree is **move-only**; large repositories are never deep-copied.
- `string_view` is used in the hot string helpers and the line counter to avoid
  allocations.
- The flat index gives O(1) file resolution instead of repeated tree walks.
- Parsing is per-file and parallel-friendly — a thread pool could be dropped into
  `parseRepository()` without touching any other module.

## Trade-offs (why it's intentionally simple)

The parser is regex-based, not a real compiler front end; the call graph is
file-granular. These are deliberate approximations that keep the code small and
fast at repository scale. There is no plugin system, no threading, and no
external libraries — the whole tool is ~1,600 lines of standard C++.
