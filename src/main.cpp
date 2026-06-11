// Command-line entry point and dispatch. Builds the pipeline once
// (scan -> parse -> graphs) and routes each subcommand to it.
#include <exception>
#include <iostream>
#include <string>
#include <vector>

#include "analyzer.hpp"
#include "exporter.hpp"
#include "graph.hpp"
#include "parser.hpp"
#include "scanner.hpp"
#include "tui.hpp"
#include "utils.hpp"
#include "view.hpp"

namespace {

void usage() {
    std::cout <<
        "repoexplorer - terminal repository structure & visualization explorer\n\n"
        "Usage: repoexplorer <command> <path> [options]\n\n"
        "Commands:\n"
        "  scan    <path>          Scan and summarize the repository\n"
        "  tree    <path>          Print the directory tree\n"
        "  stats   <path>          Show metrics and language distribution\n"
        "  deps    <path>          Show the file dependency graph\n"
        "  graph   <path>          Show the approximate call graph\n"
        "  search  <path> <query>  Search files/functions/classes/includes\n"
        "  export  <path>          Write report.{txt,md,json}, mermaid & dot\n"
        "  tui     <path>          Launch the interactive ncurses explorer\n\n"
        "Options:\n"
        "  --depth N    Limit tree/scan depth\n"
        "  --out DIR    Output directory for export (default: .)\n"
        "  --no-color   Disable colored output\n";
}

// Bundle of pipeline results so each command reuses one scan/parse.
struct Pipeline {
    re::Repository repo;
    re::AnalysisSet set;
    re::DependencyGraph deps;
    re::CallGraph calls;
};

Pipeline build(const std::string& path, int maxDepth) {
    Pipeline p;
    re::ScanOptions opt; opt.maxDepth = maxDepth;
    p.repo = re::scan(path, opt);
    p.set = re::parseRepository(p.repo);
    std::vector<std::string> files;
    files.reserve(p.repo.index().size());
    for (const auto& [rel, _] : p.repo.index()) files.push_back(rel);
    p.deps.build(p.set, files);
    p.calls.build(p.set);
    return p;
}

} // namespace

int main(int argc, char** argv) try {
    std::vector<std::string> args(argv + 1, argv + argc);
    if (args.empty()) { usage(); return 1; }

    // Parse flags vs positionals.
    std::string command, path = ".", outDir = ".", query;
    int maxDepth = -1;
    bool color = true;
    std::vector<std::string> pos;
    for (std::size_t i = 0; i < args.size(); ++i) {
        const std::string& a = args[i];
        if (a == "--no-color") color = false;
        else if (a == "--depth" && i + 1 < args.size()) maxDepth = std::stoi(args[++i]);
        else if (a == "--out" && i + 1 < args.size()) outDir = args[++i];
        else pos.push_back(a);
    }
    if (!pos.empty()) command = pos[0];
    if (pos.size() > 1) path = pos[1];
    if (pos.size() > 2) query = pos[2];
    re::colorEnabled(color);

    if (command == "help" || command == "--help" || command == "-h") { usage(); return 0; }

    if (command == "scan") {
        Pipeline p = build(path, maxDepth);
        re::Metrics m = re::computeMetrics(p.repo, p.set);
        std::cout << "Scanned: " << path << "\nFiles: " << m.files << "  Dirs: " << m.dirs
                  << "  Source files: " << m.sourceFiles << "  LOC: " << m.loc << "\n";
        return 0;
    }
    if (command == "tree") {
        Pipeline p = build(path, maxDepth);
        re::printTree(p.repo, std::cout, maxDepth);
        return 0;
    }
    if (command == "stats") {
        Pipeline p = build(path, maxDepth);
        re::Metrics m = re::computeMetrics(p.repo, p.set);
        std::cout << "== Repository Metrics ==\n";
        std::cout << "Total files........: " << m.files << "\n";
        std::cout << "Total directories..: " << m.dirs << "\n";
        std::cout << "Source files.......: " << m.sourceFiles << "\n";
        std::cout << "Lines of code......: " << m.loc << "\n";
        std::cout << "Largest file.......: " << m.largestFile << "\n\n";
        std::cout << "== Language Distribution ==\n";
        re::printLanguages(m, std::cout);
        std::cout << "\n== Directory Sizes ==\n";
        re::printDirSizes(p.repo, std::cout);
        return 0;
    }
    if (command == "deps") {
        Pipeline p = build(path, maxDepth);
        re::printDependencies(p.deps, std::cout);
        auto cycles = p.deps.cycles();
        if (!cycles.empty()) {
            std::cout << "Circular dependencies detected:\n";
            for (const auto& c : cycles) {
                std::cout << "  ";
                for (std::size_t i = 0; i < c.size(); ++i)
                    std::cout << c[i] << (i + 1 < c.size() ? " -> " : "\n");
            }
        }
        return 0;
    }
    if (command == "graph") {
        Pipeline p = build(path, maxDepth);
        re::printCallGraph(p.calls, std::cout);
        return 0;
    }
    if (command == "search") {
        if (query.empty()) { std::cerr << "Usage: repoexplorer search <path> <query>\n"; return 1; }
        Pipeline p = build(path, maxDepth);
        auto hits = re::search(p.set, query);
        std::cout << "Found " << hits.size() << " match(es) for \"" << query << "\":\n";
        for (const auto& h : hits)
            std::cout << "  [" << re::hitKindName(h.kind) << "] " << h.symbol << "  (" << h.path << ")\n";
        return 0;
    }
    if (command == "export") {
        Pipeline p = build(path, maxDepth);
        re::Metrics m = re::computeMetrics(p.repo, p.set);
        re::ModuleReport mods = re::analyzeModules(p.repo, p.deps);
        bool ok = re::exportAll(m, mods, p.deps, p.calls, outDir);
        std::cout << (ok ? "Exported reports and diagrams to " : "Export had errors in ")
                  << std::filesystem::absolute(outDir).string() << "\n"
                  << "  report.txt report.md report.json\n"
                  << "  dependencies.mmd callgraph.mmd dependencies.dot callgraph.dot\n";
        return ok ? 0 : 1;
    }
    if (command == "tui") {
        Pipeline p = build(path, maxDepth);
        re::Metrics m = re::computeMetrics(p.repo, p.set);
        return re::runTui(p.repo, p.set, m, p.deps);
    }

    std::cerr << "Unknown command: " << command << "\n\n";
    usage();
    return 1;
} catch (const std::exception& ex) {
    std::cerr << "Fatal error: " << ex.what() << "\n";
    return 2;
}
