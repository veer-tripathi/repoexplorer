#include "exporter.hpp"

#include <fstream>
#include <unordered_map>

#include "utils.hpp"

namespace fs = std::filesystem;

namespace re {

namespace {

std::string jsonEscape(const std::string& s) {
    std::string o;
    o.reserve(s.size() + 2);
    for (char c : s) {
        switch (c) {
            case '"':  o += "\\\""; break;  case '\\': o += "\\\\"; break;
            case '\n': o += "\\n";  break;  case '\t': o += "\\t";  break;
            case '\r': o += "\\r";  break;  default:   o += c;       break;
        }
    }
    return o;
}

void writeText(const Metrics& m, const ModuleReport& mods, std::ostream& os) {
    os << "REPOSITORY REPORT\n=================\n\n";
    os << "Total files........: " << m.files << "\n";
    os << "Total directories..: " << m.dirs << "\n";
    os << "Source files.......: " << m.sourceFiles << "\n";
    os << "Lines of code......: " << m.loc << "\n";
    os << "Total size.........: " << humanSize(m.totalSize) << "\n";
    os << "Largest file.......: " << m.largestFile << " (" << humanSize(m.largestFileSize) << ")\n";
    os << "Largest directory..: " << m.largestDir << " (" << m.largestDirFiles << " files)\n\n";

    os << "LANGUAGE DISTRIBUTION\n---------------------\n";
    for (const auto& l : m.byFiles())
        os << "  " << langName(l.lang) << ": " << l.files << " files, " << l.code
           << " code, " << l.comment << " comment, " << l.blank << " blank\n";

    os << "\nMOST CONNECTED FILES\n--------------------\n";
    for (const auto& r : mods.mostConnected) os << "  " << r.path << " (degree " << r.value << ")\n";

    os << "\nCIRCULAR DEPENDENCIES\n---------------------\n";
    if (mods.circular.empty()) os << "  none\n";
    for (const auto& c : mods.circular) {
        os << "  ";
        for (std::size_t i = 0; i < c.size(); ++i) os << c[i] << (i + 1 < c.size() ? " -> " : "\n");
    }
    os << "\nORPHAN FILES\n------------\n";
    for (const auto& o : mods.orphans) os << "  " << o << "\n";
    os << "\nUNUSED HEADERS\n--------------\n";
    for (const auto& h : mods.unusedHeaders) os << "  " << h << "\n";
}

void writeMarkdown(const Metrics& m, const ModuleReport& mods, std::ostream& os) {
    os << "# Repository Report\n\n## Summary\n\n| Metric | Value |\n|---|---|\n";
    os << "| Total files | " << m.files << " |\n";
    os << "| Total directories | " << m.dirs << " |\n";
    os << "| Source files | " << m.sourceFiles << " |\n";
    os << "| Lines of code | " << m.loc << " |\n";
    os << "| Total size | " << humanSize(m.totalSize) << " |\n";
    os << "| Largest file | `" << m.largestFile << "` (" << humanSize(m.largestFileSize) << ") |\n\n";

    os << "## Language Distribution\n\n| Language | Files | Code | Comment | Blank |\n|---|---|---|---|---|\n";
    for (const auto& l : m.byFiles())
        os << "| " << langName(l.lang) << " | " << l.files << " | " << l.code
           << " | " << l.comment << " | " << l.blank << " |\n";

    os << "\n## Most Connected Files\n\n";
    for (const auto& r : mods.mostConnected) os << "- `" << r.path << "` — degree " << r.value << "\n";

    os << "\n## Circular Dependencies\n\n";
    if (mods.circular.empty()) os << "_None detected._\n";
    for (const auto& c : mods.circular) {
        os << "- ";
        for (std::size_t i = 0; i < c.size(); ++i) os << "`" << c[i] << "`" << (i + 1 < c.size() ? " → " : "\n");
    }
}

void writeJson(const Metrics& m, const DependencyGraph& deps, std::ostream& os) {
    os << "{\n";
    os << "  \"totalFiles\": " << m.files << ",\n";
    os << "  \"totalDirectories\": " << m.dirs << ",\n";
    os << "  \"sourceFiles\": " << m.sourceFiles << ",\n";
    os << "  \"linesOfCode\": " << m.loc << ",\n";
    os << "  \"totalSizeBytes\": " << m.totalSize << ",\n";
    os << "  \"largestFile\": \"" << jsonEscape(m.largestFile) << "\",\n";
    os << "  \"languages\": [\n";
    auto langs = m.byFiles();
    for (std::size_t i = 0; i < langs.size(); ++i) {
        const auto& l = langs[i];
        os << "    { \"name\": \"" << jsonEscape(std::string(langName(l.lang)))
           << "\", \"files\": " << l.files << ", \"code\": " << l.code
           << ", \"comment\": " << l.comment << ", \"blank\": " << l.blank << " }"
           << (i + 1 < langs.size() ? "," : "") << "\n";
    }
    os << "  ],\n  \"dependencies\": {\n";
    auto nodes = deps.nodes();
    for (std::size_t i = 0; i < nodes.size(); ++i) {
        const auto& outs = deps.outgoing(nodes[i]);
        os << "    \"" << jsonEscape(nodes[i]) << "\": [";
        for (std::size_t j = 0; j < outs.size(); ++j)
            os << "\"" << jsonEscape(outs[j]) << "\"" << (j + 1 < outs.size() ? ", " : "");
        os << "]" << (i + 1 < nodes.size() ? "," : "") << "\n";
    }
    os << "  }\n}\n";
}

// Mermaid needs alphanumeric ids; map each label to n0, n1, ...
struct IdMap {
    std::unordered_map<std::string, std::string> ids;
    std::string operator()(const std::string& label) {
        auto it = ids.find(label);
        if (it != ids.end()) return it->second;
        std::string id = "n" + std::to_string(ids.size());
        ids.emplace(label, id);
        return id;
    }
};

void writeMermaidDeps(const DependencyGraph& deps, std::ostream& os) {
    os << "graph TD\n";
    IdMap id;
    for (const std::string& n : deps.nodes()) {
        std::string from = id(n);
        os << "    " << from << "[\"" << n << "\"]\n";
        for (const std::string& d : deps.outgoing(n))
            os << "    " << from << " --> " << id(d) << "[\"" << d << "\"]\n";
    }
}

void writeMermaidCalls(const CallGraph& calls, std::ostream& os) {
    os << "graph TD\n";
    IdMap id;
    for (const std::string& fn : calls.functions()) {
        std::string from = id(fn);
        os << "    " << from << "[\"" << fn << "()\"]\n";
        for (const std::string& c : calls.callees(fn))
            os << "    " << from << " --> " << id(c) << "[\"" << c << "()\"]\n";
    }
}

void writeDotDeps(const DependencyGraph& deps, std::ostream& os) {
    os << "digraph Repo {\n    rankdir=LR;\n    node [shape=box, fontname=\"monospace\"];\n";
    for (const std::string& n : deps.nodes())
        for (const std::string& d : deps.outgoing(n))
            os << "    \"" << n << "\" -> \"" << d << "\";\n";
    os << "}\n";
}

void writeDotCalls(const CallGraph& calls, std::ostream& os) {
    os << "digraph CallGraph {\n    node [shape=ellipse, fontname=\"monospace\"];\n";
    for (const std::string& fn : calls.functions())
        for (const std::string& c : calls.callees(fn))
            os << "    \"" << fn << "\" -> \"" << c << "\";\n";
    os << "}\n";
}

} // namespace

bool exportAll(const Metrics& m, const ModuleReport& mods,
               const DependencyGraph& deps, const CallGraph& calls,
               const fs::path& outDir) {
    std::error_code ec;
    fs::create_directories(outDir, ec);

    std::ofstream txt(outDir / "report.txt");
    std::ofstream md(outDir / "report.md");
    std::ofstream js(outDir / "report.json");
    if (!txt || !md || !js) return false;
    writeText(m, mods, txt);
    writeMarkdown(m, mods, md);
    writeJson(m, deps, js);

    std::ofstream md1(outDir / "dependencies.mmd"); writeMermaidDeps(deps, md1);
    std::ofstream md2(outDir / "callgraph.mmd");    writeMermaidCalls(calls, md2);
    std::ofstream dot1(outDir / "dependencies.dot"); writeDotDeps(deps, dot1);
    std::ofstream dot2(outDir / "callgraph.dot");    writeDotCalls(calls, dot2);
    return true;
}

} // namespace re
