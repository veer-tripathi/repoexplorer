#pragma once
// Language detection, line counting and lightweight regex static analysis.
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "scanner.hpp"

namespace re {

enum class Lang { Unknown, C, Cpp, Python, Java, JavaScript, TypeScript, Rust,
                  Go, Kotlin, CSharp, Php, Shell, Markdown };

Lang langFromExt(std::string_view ext);
std::string_view langName(Lang l);
bool isSource(Lang l);                 // excludes Unknown and Markdown

struct LineStats { std::size_t total = 0, blank = 0, comment = 0, code = 0; };

struct FuncInfo { std::string name; int line = 0; };
struct ClassInfo { std::string name; bool isStruct = false; };

struct FileAnalysis {
    std::string path;
    Lang lang = Lang::Unknown;
    LineStats lines;
    std::vector<std::string> includes;     // resolved later by the graph
    std::vector<std::string> namespaces;
    std::vector<ClassInfo> classes;
    std::vector<FuncInfo> functions;       // includes methods
    std::vector<std::string> enums;
    std::size_t templates = 0;
    std::vector<std::string> calls;        // for the call graph
};

using AnalysisSet = std::unordered_map<std::string, FileAnalysis>;

// Parse already-loaded text (used by tests) or a file on disk.
FileAnalysis parseText(std::string_view content, std::string path, Lang lang);

// Parse every recognized source file in a repository.
AnalysisSet parseRepository(const Repository& repo);

} // namespace re
