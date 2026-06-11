#include "view.hpp"

#include <algorithm>
#include <functional>
#include <vector>

#include "utils.hpp"

namespace re {

namespace {

void printDir(const DirNode& dir, std::ostream& os, const std::string& prefix,
              int depth, int maxDepth) {
    if (maxDepth >= 0 && depth > maxDepth) return;
    std::size_t total = dir.subdirs.size() + dir.files.size();
    std::size_t i = 0;
    for (const auto& sub : dir.subdirs) {
        bool last = (++i == total);
        os << prefix << (last ? "└── " : "├── ")
           << paint(sub->name + "/", Color::Blue) << "\n";
        printDir(*sub, os, prefix + (last ? "    " : "│   "), depth + 1, maxDepth);
    }
    for (const auto& f : dir.files) {
        bool last = (++i == total);
        Color c = f.isSymlink ? Color::Cyan : f.isHidden ? Color::Gray : Color::Reset;
        os << prefix << (last ? "└── " : "├── ")
           << paint(f.name, c) << "\n";
    }
}

} // namespace

void printTree(const Repository& repo, std::ostream& os, int maxDepth) {
    os << paint(repo.root().name + "/", Color::Bold) << "\n";
    printDir(repo.root(), os, "", 0, maxDepth);
}

void printLanguages(const Metrics& m, std::ostream& os) {
    auto langs = m.byFiles();
    std::size_t total = 0;
    for (const auto& l : langs) total += l.files;
    if (!total) { os << "(no recognized source files)\n"; return; }
    std::size_t w = 0;
    for (const auto& l : langs) w = std::max(w, langName(l.lang).size());
    for (const auto& l : langs) {
        double ratio = double(l.files) / double(total);
        std::string name(langName(l.lang));
        name.resize(w, ' ');
        os << name << " " << bar(ratio) << " " << int(ratio * 100 + 0.5)
           << "% (" << l.files << " files, " << l.code << " LOC)\n";
    }
}

void printDirSizes(const Repository& repo, std::ostream& os, std::size_t topN) {
    std::vector<std::pair<std::string, std::size_t>> dirs;
    std::function<void(const DirNode&)> walk = [&](const DirNode& d) {
        dirs.emplace_back(d.path.empty() ? "." : d.path, d.files.size());
        for (const auto& s : d.subdirs) walk(*s);
    };
    walk(repo.root());
    std::sort(dirs.begin(), dirs.end(), [](const auto& a, const auto& b) { return a.second > b.second; });
    if (dirs.size() > topN) dirs.resize(topN);
    std::size_t maxCount = dirs.empty() ? 0 : dirs.front().second;
    std::size_t w = 0;
    for (const auto& d : dirs) w = std::max(w, d.first.size());
    for (const auto& d : dirs) {
        double ratio = maxCount ? double(d.second) / double(maxCount) : 0.0;
        std::string name = d.first;
        name.resize(w, ' ');
        os << name << " " << bar(ratio) << " " << d.second << "\n";
    }
}

void printDependencies(const DependencyGraph& deps, std::ostream& os) {
    for (const std::string& n : deps.nodes()) {
        const auto& outs = deps.outgoing(n);
        if (outs.empty()) continue;
        os << paint(n, Color::Bold) << "\n";
        for (std::size_t i = 0; i < outs.size(); ++i)
            os << (i + 1 == outs.size() ? "└── " : "├── ") << outs[i] << "\n";
        os << "\n";
    }
}

void printCallGraph(const CallGraph& calls, std::ostream& os) {
    for (const std::string& fn : calls.functions()) {
        const auto& cs = calls.callees(fn);
        if (cs.empty()) continue;
        os << paint(fn + "()", Color::Bold) << "\n";
        for (std::size_t i = 0; i < cs.size(); ++i)
            os << "  " << (i + 1 == cs.size() ? "└──> " : "├──> ") << cs[i] << "()\n";
        os << "\n";
    }
}

} // namespace re
