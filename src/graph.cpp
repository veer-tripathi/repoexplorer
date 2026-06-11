#include "graph.hpp"

#include <algorithm>
#include <functional>
#include <unordered_set>

namespace re {

const std::vector<std::string> DependencyGraph::kEmpty{};
const std::vector<std::string> CallGraph::kEmpty{};

static std::string baseName(const std::string& p) {
    auto pos = p.find_last_of('/');
    return pos == std::string::npos ? p : p.substr(pos + 1);
}

void DependencyGraph::build(const AnalysisSet& set,
                            const std::vector<std::string>& files) {
    out_.clear();
    in_.clear();

    // Resolve includes like "parser.hpp" to "src/parser.hpp" by basename.
    std::unordered_map<std::string, std::string> byBase;
    for (const std::string& f : files) byBase.emplace(baseName(f), f);

    for (const auto& [path, a] : set) {
        out_.try_emplace(path);
        for (const std::string& inc : a.includes) {
            std::string target;
            if (set.count(inc)) target = inc;
            else {
                auto it = byBase.find(baseName(inc));
                if (it != byBase.end()) target = it->second;
            }
            if (target.empty() || target == path) continue;
            out_[path].push_back(target);
            in_[target].push_back(path);
        }
    }
    auto dedup = [](auto& map) {
        for (auto& [_, v] : map) {
            std::sort(v.begin(), v.end());
            v.erase(std::unique(v.begin(), v.end()), v.end());
        }
    };
    dedup(out_);
    dedup(in_);
}

const std::vector<std::string>& DependencyGraph::outgoing(const std::string& f) const {
    auto it = out_.find(f);
    return it == out_.end() ? kEmpty : it->second;
}

const std::vector<std::string>& DependencyGraph::incoming(const std::string& f) const {
    auto it = in_.find(f);
    return it == in_.end() ? kEmpty : it->second;
}

std::vector<std::string> DependencyGraph::nodes() const {
    std::vector<std::string> r;
    r.reserve(out_.size());
    for (const auto& [n, _] : out_) r.push_back(n);
    std::sort(r.begin(), r.end());
    return r;
}

std::vector<std::vector<std::string>> DependencyGraph::cycles() const {
    // DFS with a recursion stack; a back edge to an on-stack node = a cycle.
    std::vector<std::vector<std::string>> found;
    std::unordered_set<std::string> visited, onStack;
    std::vector<std::string> stack;
    std::function<void(const std::string&)> dfs = [&](const std::string& n) {
        visited.insert(n);
        onStack.insert(n);
        stack.push_back(n);
        auto it = out_.find(n);
        if (it != out_.end())
            for (const std::string& nx : it->second) {
                if (onStack.count(nx)) {
                    auto start = std::find(stack.begin(), stack.end(), nx);
                    std::vector<std::string> c(start, stack.end());
                    c.push_back(nx);
                    found.push_back(std::move(c));
                } else if (!visited.count(nx)) dfs(nx);
            }
        onStack.erase(n);
        stack.pop_back();
    };
    for (const auto& [n, _] : out_) if (!visited.count(n)) dfs(n);
    return found;
}

void CallGraph::build(const AnalysisSet& set) {
    calls_.clear();
    std::unordered_set<std::string> defined;
    for (const auto& [_, a] : set)
        for (const auto& fn : a.functions) defined.insert(fn.name);

    for (const auto& [_, a] : set) {
        if (a.functions.empty()) continue;
        std::vector<std::string> known;
        for (const std::string& c : a.calls)
            if (defined.count(c)) known.push_back(c);
        for (const auto& fn : a.functions) {
            auto& e = calls_[fn.name];
            for (const std::string& c : known) if (c != fn.name) e.push_back(c);
        }
    }
    for (auto& [_, v] : calls_) {
        std::sort(v.begin(), v.end());
        v.erase(std::unique(v.begin(), v.end()), v.end());
    }
}

const std::vector<std::string>& CallGraph::callees(const std::string& fn) const {
    auto it = calls_.find(fn);
    return it == calls_.end() ? kEmpty : it->second;
}

std::vector<std::string> CallGraph::functions() const {
    std::vector<std::string> r;
    r.reserve(calls_.size());
    for (const auto& [fn, _] : calls_) r.push_back(fn);
    std::sort(r.begin(), r.end());
    return r;
}

} // namespace re
