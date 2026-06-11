#pragma once
// Aggregate metrics, module insights, and a simple symbol search.
#include <map>
#include <string>
#include <vector>

#include "graph.hpp"
#include "parser.hpp"
#include "scanner.hpp"

namespace re {

struct LangStats {
    Lang lang = Lang::Unknown;
    std::size_t files = 0, total = 0, code = 0, blank = 0, comment = 0;
};

struct Metrics {
    std::size_t files = 0, dirs = 0, sourceFiles = 0, loc = 0;
    std::uintmax_t totalSize = 0;
    double avgFileSize = 0.0;
    std::string largestFile;     std::uintmax_t largestFileSize = 0;
    std::string largestDir;      std::size_t largestDirFiles = 0;
    std::map<std::string, LangStats> languages;
    std::vector<LangStats> byFiles() const; // sorted desc by file count
};

Metrics computeMetrics(const Repository& repo, const AnalysisSet& set);

struct Ranked { std::string path; std::size_t value = 0; };
struct ModuleReport {
    std::vector<Ranked> mostConnected, largestFiles, largestDirs;
    std::vector<std::vector<std::string>> circular;
    std::vector<std::string> orphans, unusedHeaders;
};

ModuleReport analyzeModules(const Repository& repo, const DependencyGraph& deps,
                            std::size_t topN = 10);

// Search by file/function/class/include name (case-insensitive substring).
enum class HitKind { File, Function, Class, Include };
struct Hit { HitKind kind; std::string symbol, path; };
std::string_view hitKindName(HitKind k);
std::vector<Hit> search(const AnalysisSet& set, const std::string& query);

} // namespace re
