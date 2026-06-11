#pragma once
// Optional ncurses explorer. Without ncurses it prints a fallback message.
#include "analyzer.hpp"
#include "graph.hpp"
#include "parser.hpp"
#include "scanner.hpp"

namespace re {

int runTui(const Repository& repo, const AnalysisSet& set, const Metrics& m,
           const DependencyGraph& deps);

} // namespace re
