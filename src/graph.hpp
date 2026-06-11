#pragma once
// Dependency and call graphs as adjacency lists.
#include <string>
#include <unordered_map>
#include <vector>

#include "parser.hpp"

namespace re {

// File -> file dependency graph from include/import statements.
class DependencyGraph {
public:
    // 'files' is the list of valid repo paths used to resolve includes.
    void build(const AnalysisSet& set, const std::vector<std::string>& files);

    const std::vector<std::string>& outgoing(const std::string& f) const;
    const std::vector<std::string>& incoming(const std::string& f) const;
    std::size_t outCount(const std::string& f) const { return outgoing(f).size(); }
    std::size_t inCount(const std::string& f) const { return incoming(f).size(); }
    std::vector<std::string> nodes() const;
    std::vector<std::vector<std::string>> cycles() const;

private:
    std::unordered_map<std::string, std::vector<std::string>> out_, in_;
    static const std::vector<std::string> kEmpty;
};

// Approximate function call graph (function -> known callees).
class CallGraph {
public:
    void build(const AnalysisSet& set);
    const std::vector<std::string>& callees(const std::string& fn) const;
    std::vector<std::string> functions() const;

private:
    std::unordered_map<std::string, std::vector<std::string>> calls_;
    static const std::vector<std::string> kEmpty;
};

} // namespace re
