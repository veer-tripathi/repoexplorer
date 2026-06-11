#pragma once
// Terminal visualizations: tree, language bars, directory sizes, graph lists.
#include <ostream>

#include "analyzer.hpp"
#include "graph.hpp"
#include "scanner.hpp"

namespace re {

void printTree(const Repository& repo, std::ostream& os, int maxDepth = -1);
void printLanguages(const Metrics& m, std::ostream& os);
void printDirSizes(const Repository& repo, std::ostream& os, std::size_t topN = 10);
void printDependencies(const DependencyGraph& deps, std::ostream& os);
void printCallGraph(const CallGraph& calls, std::ostream& os);

} // namespace re
