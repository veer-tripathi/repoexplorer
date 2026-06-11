// Minimal dependency-free test runner. CHECK records failures; main returns
// non-zero if any failed. No external framework needed.
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#include "analyzer.hpp"
#include "graph.hpp"
#include "parser.hpp"
#include "scanner.hpp"

namespace fs = std::filesystem;

static int g_failures = 0;
static int g_checks = 0;

#define CHECK(cond)                                                      \
    do {                                                                 \
        ++g_checks;                                                      \
        if (!(cond)) {                                                   \
            ++g_failures;                                                \
            std::cerr << "FAIL: " << #cond << "  (" << __FILE__ << ":"   \
                      << __LINE__ << ")\n";                              \
        }                                                                \
    } while (0)

// --- parser ----------------------------------------------------------------
static void testParser() {
    CHECK(re::langFromExt(".cpp") == re::Lang::Cpp);
    CHECK(re::langFromExt(".py") == re::Lang::Python);
    CHECK(re::langFromExt(".zzz") == re::Lang::Unknown);

    const char* code =
        "#include \"parser.hpp\"\n"
        "// a comment\n"
        "\n"
        "namespace demo {\n"
        "class Widget {};\n"
        "struct Point {};\n"
        "enum class Color { Red };\n"
        "template <typename T>\n"
        "void doThing(T v) {\n"
        "    helper(v);\n"
        "}\n"
        "}\n";
    re::FileAnalysis a = re::parseText(code, "demo.cpp", re::Lang::Cpp);
    CHECK(a.includes.size() == 1);
    CHECK(a.includes[0] == "parser.hpp");
    CHECK(a.namespaces.size() == 1);
    CHECK(a.classes.size() == 2);     // class + struct
    CHECK(a.enums.size() == 1);
    CHECK(a.templates == 1);
    CHECK(a.lines.blank == 1);

    bool fn = false, call = false;
    for (const auto& f : a.functions) if (f.name == "doThing") fn = true;
    for (const auto& c : a.calls) if (c == "helper") call = true;
    CHECK(fn);
    CHECK(call);
}

// --- graph -----------------------------------------------------------------
static void testGraph() {
    re::AnalysisSet set;
    re::FileAnalysis a; a.path = "src/main.cpp"; a.lang = re::Lang::Cpp; a.includes = {"parser.hpp"};
    re::FileAnalysis b; b.path = "include/parser.hpp"; b.lang = re::Lang::Cpp;
    set.emplace(a.path, a);
    set.emplace(b.path, b);

    re::DependencyGraph deps;
    deps.build(set, {"src/main.cpp", "include/parser.hpp"});
    CHECK(deps.outCount("src/main.cpp") == 1);
    CHECK(deps.outgoing("src/main.cpp")[0] == "include/parser.hpp");
    CHECK(deps.inCount("include/parser.hpp") == 1);

    re::AnalysisSet cyc;
    re::FileAnalysis x; x.path = "a.hpp"; x.lang = re::Lang::Cpp; x.includes = {"b.hpp"};
    re::FileAnalysis y; y.path = "b.hpp"; y.lang = re::Lang::Cpp; y.includes = {"a.hpp"};
    cyc.emplace(x.path, x);
    cyc.emplace(y.path, y);
    re::DependencyGraph d2;
    d2.build(cyc, {"a.hpp", "b.hpp"});
    CHECK(!d2.cycles().empty());
}

// --- scanner + integration -------------------------------------------------
static void testScannerIntegration() {
    fs::path root = fs::temp_directory_path() / "repoexplorer_test";
    fs::remove_all(root);
    fs::create_directories(root / "src");
    std::ofstream(root / "src" / "main.cpp")
        << "#include \"util.hpp\"\nint main(){ helper(); return 0; }\n";
    std::ofstream(root / "src" / "util.hpp") << "#pragma once\nvoid helper(){}\n";
    std::ofstream(root / ".hidden") << "x\n";

    re::Repository repo = re::scan(root);
    CHECK(repo.find("src/main.cpp") != nullptr);
    CHECK(repo.find("src/util.hpp") != nullptr);
    const re::FileNode* hidden = repo.find(".hidden");
    CHECK(hidden != nullptr && hidden->isHidden);

    re::AnalysisSet set = re::parseRepository(repo);
    CHECK(set.size() == 2);

    std::vector<std::string> files;
    for (const auto& [rel, _] : repo.index()) files.push_back(rel);
    re::DependencyGraph deps;
    deps.build(set, files);
    CHECK(deps.outCount("src/main.cpp") == 1);

    re::Metrics m = re::computeMetrics(repo, set);
    CHECK(m.sourceFiles >= 2);
    CHECK(m.loc > 0);

    // depth limit: should not descend into src/
    re::ScanOptions opt; opt.maxDepth = 1;
    re::Repository shallow = re::scan(root, opt);
    CHECK(shallow.find("src/main.cpp") == nullptr);

    fs::remove_all(root);
}

int main() {
    testParser();
    testGraph();
    testScannerIntegration();

    std::cout << (g_checks - g_failures) << "/" << g_checks << " checks passed\n";
    if (g_failures == 0) { std::cout << "ALL TESTS PASSED\n"; return 0; }
    std::cerr << g_failures << " CHECK(S) FAILED\n";
    return 1;
}
