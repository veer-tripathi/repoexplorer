#pragma once
// Write reports (txt/md/json) and diagram files (mermaid/dot).
#include <filesystem>

#include "analyzer.hpp"
#include "graph.hpp"

namespace re {

// Writes report.{txt,md,json}, dependencies.{mmd,dot}, callgraph.{mmd,dot}.
bool exportAll(const Metrics& m, const ModuleReport& mods,
               const DependencyGraph& deps, const CallGraph& calls,
               const std::filesystem::path& outDir);

} // namespace re
