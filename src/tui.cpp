#include "tui.hpp"

#include <iostream>

#if REPOEXPLORER_HAS_NCURSES

#include <ncurses.h>

#include <string>
#include <vector>

namespace re {

namespace {

struct Row { std::string display, path; };

void flatten(const DirNode& d, int depth, std::vector<Row>& rows) {
    for (const auto& s : d.subdirs) {
        rows.push_back({std::string(depth * 2, ' ') + s->name + "/", ""});
        flatten(*s, depth + 1, rows);
    }
    for (const auto& f : d.files)
        rows.push_back({std::string(depth * 2, ' ') + f.name, f.path});
}

enum class V { Tree, Stats, Deps, Search };
const char* vName(V v) {
    switch (v) { case V::Tree: return "Tree"; case V::Stats: return "Statistics";
                 case V::Deps: return "Dependencies"; case V::Search: return "Search"; }
    return "";
}

void chrome(V v) {
    int h, w; getmaxyx(stdscr, h, w);
    attron(A_REVERSE);
    std::string head = std::string(" repoexplorer  |  View: ") + vName(v);
    head.resize(w, ' '); mvprintw(0, 0, "%s", head.c_str());
    std::string foot = " [1]Tree [2]Stats [3]Deps [/]Search  Up/Down  Enter  q quit ";
    foot.resize(w, ' '); mvprintw(h - 1, 0, "%s", foot.c_str());
    attroff(A_REVERSE);
}

void details(const AnalysisSet& set, const std::string& path) {
    int row = 1;
    mvprintw(row++, 0, "File: %s", path.c_str());
    auto it = set.find(path);
    if (it == set.end()) { mvprintw(row, 0, "(not a recognized source file)"); return; }
    const auto& a = it->second;
    mvprintw(row++, 0, "Language: %s", std::string(langName(a.lang)).c_str());
    mvprintw(row++, 0, "Lines: total=%zu code=%zu comment=%zu blank=%zu",
             a.lines.total, a.lines.code, a.lines.comment, a.lines.blank);
    mvprintw(row++, 0, "Classes: %zu  Functions: %zu  Includes: %zu",
             a.classes.size(), a.functions.size(), a.includes.size());
    ++row; mvprintw(row++, 0, "Functions:");
    for (const auto& fn : a.functions) mvprintw(row++, 2, "%s (line %d)", fn.name.c_str(), fn.line);
}

void stats(const Metrics& m) {
    int row = 1;
    mvprintw(row++, 0, "Total files........: %zu", m.files);
    mvprintw(row++, 0, "Total directories..: %zu", m.dirs);
    mvprintw(row++, 0, "Source files.......: %zu", m.sourceFiles);
    mvprintw(row++, 0, "Lines of code......: %zu", m.loc);
    ++row; mvprintw(row++, 0, "Language distribution:");
    for (const auto& l : m.byFiles())
        mvprintw(row++, 2, "%-12s %5zu files  %7zu LOC",
                 std::string(langName(l.lang)).c_str(), l.files, l.code);
}

} // namespace

int runTui(const Repository& repo, const AnalysisSet& set, const Metrics& m,
           const DependencyGraph& deps) {
    std::vector<Row> rows;
    flatten(repo.root(), 0, rows);

    initscr(); cbreak(); noecho(); keypad(stdscr, TRUE); curs_set(0);
    V view = V::Tree;
    int sel = 0, top = 0, depsTop = 0;
    std::string query;
    bool showDetails = false;

    while (true) {
        erase(); chrome(view);
        int h, w; getmaxyx(stdscr, h, w); (void)w;
        int viewport = h - 3;

        if (view == V::Tree) {
            if (showDetails && sel < (int)rows.size() && !rows[sel].path.empty())
                details(set, rows[sel].path);
            else
                for (int i = 0; i < viewport && top + i < (int)rows.size(); ++i) {
                    int idx = top + i;
                    if (idx == sel) attron(A_REVERSE);
                    mvprintw(i + 1, 0, "%s", rows[idx].display.c_str());
                    if (idx == sel) attroff(A_REVERSE);
                }
        } else if (view == V::Stats) {
            stats(m);
        } else if (view == V::Deps) {
            auto nodes = deps.nodes();
            int row = 1;
            for (int i = depsTop; i < (int)nodes.size() && row < h - 1; ++i) {
                const auto& outs = deps.outgoing(nodes[i]);
                if (outs.empty()) continue;
                mvprintw(row++, 0, "%s", nodes[i].c_str());
                for (const auto& d : outs) { if (row >= h - 1) break; mvprintw(row++, 2, "-> %s", d.c_str()); }
            }
        } else {
            mvprintw(1, 0, "Search: %s", query.c_str());
            int row = 3;
            for (const auto& hit : search(set, query)) {
                if (row >= h - 1) break;
                mvprintw(row++, 0, "[%s] %s  (%s)",
                         std::string(hitKindName(hit.kind)).c_str(),
                         hit.symbol.c_str(), hit.path.c_str());
            }
        }
        refresh();

        int ch = getch();
        if (view == V::Search) {
            if (ch == 27) view = V::Tree;
            else if (ch == KEY_BACKSPACE || ch == 127 || ch == 8) { if (!query.empty()) query.pop_back(); }
            else if (ch >= 32 && ch < 127) query.push_back((char)ch);
            continue;
        }
        if (ch == 'q') break;
        else if (ch == '1') { view = V::Tree; showDetails = false; }
        else if (ch == '2') view = V::Stats;
        else if (ch == '3') view = V::Deps;
        else if (ch == '/') { view = V::Search; query.clear(); }
        else if (ch == 27) showDetails = false;
        else if (ch == KEY_UP) {
            if (view == V::Tree && sel > 0) { --sel; if (sel < top) top = sel; }
            else if (view == V::Deps && depsTop > 0) --depsTop;
        } else if (ch == KEY_DOWN) {
            if (view == V::Tree && sel + 1 < (int)rows.size()) { ++sel; if (sel >= top + viewport) top = sel - viewport + 1; }
            else if (view == V::Deps) ++depsTop;
        } else if (ch == '\n') {
            if (view == V::Tree && sel < (int)rows.size() && !rows[sel].path.empty())
                showDetails = !showDetails;
        }
    }
    endwin();
    return 0;
}

} // namespace re

#else

namespace re {
int runTui(const Repository&, const AnalysisSet&, const Metrics&, const DependencyGraph&) {
    std::cout << "The interactive TUI requires ncurses, which was not found at build time.\n"
                 "Install ncurses and rebuild, or use: tree, stats, deps, graph, export.\n";
    return 0;
}
} // namespace re

#endif
