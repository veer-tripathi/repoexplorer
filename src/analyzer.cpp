#include "analyzer.hpp"

#include <algorithm>

#include "utils.hpp"

namespace re {

namespace {

void walk(const DirNode& d, Metrics& m) {
    if (d.files.size() > m.largestDirFiles) {
        m.largestDirFiles = d.files.size();
        m.largestDir = d.path.empty() ? "." : d.path;
    }
    for (const auto& f : d.files) {
        m.totalSize += f.size;
        if (f.size > m.largestFileSize) { m.largestFileSize = f.size; m.largestFile = f.path; }
    }
    for (const auto& s : d.subdirs) walk(*s, m);
}

bool isHeader(const std::string& p) {
    for (const char* e : {".h", ".hpp", ".hh", ".hxx"}) if (endsWith(p, e)) return true;
    return false;
}

void rank(std::vector<Ranked>& v, std::size_t topN) {
    std::sort(v.begin(), v.end(), [](const Ranked& a, const Ranked& b) { return a.value > b.value; });
    if (v.size() > topN) v.resize(topN);
}

void collectDirs(const DirNode& d, std::vector<Ranked>& out) {
    out.push_back({d.path.empty() ? "." : d.path, d.files.size()});
    for (const auto& s : d.subdirs) collectDirs(*s, out);
}

} // namespace

Metrics computeMetrics(const Repository& repo, const AnalysisSet& set) {
    Metrics m;
    m.files = repo.fileCount();
    m.dirs = repo.dirCount();
    walk(repo.root(), m);
    if (m.files) m.avgFileSize = double(m.totalSize) / double(m.files);

    for (const auto& [path, a] : set) {
        if (!isSource(a.lang) && a.lang != Lang::Markdown) continue;
        if (isSource(a.lang)) { ++m.sourceFiles; m.loc += a.lines.code; }
        LangStats& s = m.languages[std::string(langName(a.lang))];
        s.lang = a.lang;
        ++s.files;
        s.total += a.lines.total;
        s.code += a.lines.code;
        s.blank += a.lines.blank;
        s.comment += a.lines.comment;
    }
    return m;
}

std::vector<LangStats> Metrics::byFiles() const {
    std::vector<LangStats> r;
    for (const auto& [_, s] : languages) r.push_back(s);
    std::sort(r.begin(), r.end(), [](const LangStats& a, const LangStats& b) { return a.files > b.files; });
    return r;
}

ModuleReport analyzeModules(const Repository& repo, const DependencyGraph& deps,
                            std::size_t topN) {
    ModuleReport r;
    for (const std::string& n : deps.nodes()) {
        std::size_t degree = deps.inCount(n) + deps.outCount(n);
        if (degree) r.mostConnected.push_back({n, degree});
        if (deps.inCount(n) == 0 && deps.outCount(n) == 0) r.orphans.push_back(n);
        if (isHeader(n) && deps.inCount(n) == 0) r.unusedHeaders.push_back(n);
    }
    rank(r.mostConnected, topN);
    std::sort(r.orphans.begin(), r.orphans.end());
    std::sort(r.unusedHeaders.begin(), r.unusedHeaders.end());

    for (const auto& [path, node] : repo.index()) r.largestFiles.push_back({path, node->size});
    rank(r.largestFiles, topN);

    collectDirs(repo.root(), r.largestDirs);
    rank(r.largestDirs, topN);

    r.circular = deps.cycles();
    return r;
}

std::string_view hitKindName(HitKind k) {
    switch (k) {
        case HitKind::File: return "file";       case HitKind::Function: return "function";
        case HitKind::Class: return "class";     case HitKind::Include: return "include";
    }
    return "?";
}

std::vector<Hit> search(const AnalysisSet& set, const std::string& query) {
    std::vector<Hit> hits;
    if (query.empty()) return hits;
    std::string needle = toLower(query);
    auto match = [&](HitKind k, const std::string& sym, const std::string& path) {
        if (toLower(sym).find(needle) != std::string::npos) hits.push_back({k, sym, path});
    };
    for (const auto& [path, a] : set) {
        match(HitKind::File, path, path);
        for (const auto& fn : a.functions) match(HitKind::Function, fn.name, path);
        for (const auto& c : a.classes) match(HitKind::Class, c.name, path);
        for (const auto& inc : a.includes) match(HitKind::Include, inc, path);
    }
    return hits;
}

} // namespace re
