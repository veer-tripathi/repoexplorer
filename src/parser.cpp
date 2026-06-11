#include "parser.hpp"

#include <fstream>
#include <regex>
#include <sstream>
#include <unordered_set>

#include "utils.hpp"

namespace fs = std::filesystem;

namespace re {

Lang langFromExt(std::string_view ext) {
    static const std::unordered_map<std::string_view, Lang> t{
        {".c", Lang::C}, {".h", Lang::C},
        {".cpp", Lang::Cpp}, {".cc", Lang::Cpp}, {".cxx", Lang::Cpp},
        {".hpp", Lang::Cpp}, {".hh", Lang::Cpp}, {".hxx", Lang::Cpp},
        {".py", Lang::Python}, {".java", Lang::Java},
        {".js", Lang::JavaScript}, {".jsx", Lang::JavaScript}, {".mjs", Lang::JavaScript},
        {".ts", Lang::TypeScript}, {".tsx", Lang::TypeScript},
        {".rs", Lang::Rust}, {".go", Lang::Go},
        {".kt", Lang::Kotlin}, {".kts", Lang::Kotlin},
        {".cs", Lang::CSharp}, {".php", Lang::Php},
        {".sh", Lang::Shell}, {".bash", Lang::Shell},
        {".md", Lang::Markdown}, {".markdown", Lang::Markdown}};
    auto it = t.find(ext);
    return it == t.end() ? Lang::Unknown : it->second;
}

std::string_view langName(Lang l) {
    switch (l) {
        case Lang::C: return "C";                case Lang::Cpp: return "C++";
        case Lang::Python: return "Python";      case Lang::Java: return "Java";
        case Lang::JavaScript: return "JavaScript";
        case Lang::TypeScript: return "TypeScript";
        case Lang::Rust: return "Rust";          case Lang::Go: return "Go";
        case Lang::Kotlin: return "Kotlin";      case Lang::CSharp: return "C#";
        case Lang::Php: return "PHP";            case Lang::Shell: return "Shell";
        case Lang::Markdown: return "Markdown";  case Lang::Unknown: return "Unknown";
    }
    return "Unknown";
}

bool isSource(Lang l) { return l != Lang::Unknown && l != Lang::Markdown; }

namespace {

// Comment markers per language: {line, blockOpen, blockClose}.
struct Comments { std::string_view line, open, close; };
Comments comments(Lang l) {
    if (l == Lang::Python || l == Lang::Shell) return {"#", "", ""};
    if (l == Lang::Markdown || l == Lang::Unknown) return {"", "", ""};
    return {"//", "/*", "*/"};
}

LineStats countLines(const std::string& content, Lang lang) {
    LineStats s;
    Comments c = comments(lang);
    bool hasBlock = !c.open.empty();
    bool inBlock = false;
    std::istringstream in(content);
    std::string line;
    while (std::getline(in, line)) {
        ++s.total;
        std::string_view t = trim(line);
        if (t.empty()) { ++s.blank; continue; }
        if (inBlock) {
            ++s.comment;
            if (t.find(c.close) != std::string_view::npos) inBlock = false;
            continue;
        }
        bool isComment = false;
        if (!c.line.empty() && startsWith(t, c.line)) {
            isComment = true;
        } else if (hasBlock && startsWith(t, c.open)) {
            isComment = true;
            if (t.find(c.close, c.open.size()) == std::string_view::npos) inBlock = true;
        }
        isComment ? ++s.comment : ++s.code;
    }
    return s;
}

const std::regex kCInclude(R"(^\s*#\s*include\s*[<"]([^">]+)[">])");
const std::regex kPyImport(R"(^\s*(?:from\s+([\w\.]+)\s+import|import\s+([\w\.]+)))");
const std::regex kJsImport(R"(^\s*import\s+.*?from\s+['"]([^'"]+)['"])");
const std::regex kNamespace(R"(\bnamespace\s+([A-Za-z_]\w*))");
const std::regex kClass(R"(\b(class|struct)\s+([A-Za-z_]\w*))");
const std::regex kEnum(R"(\benum\s+(?:class\s+)?([A-Za-z_]\w*))");
const std::regex kTemplate(R"(\btemplate\s*<)");
const std::regex kFunc(R"(^[A-Za-z_][\w:<>,\*&\s]*?\b([A-Za-z_]\w*)\s*\([^;{]*\)\s*(?:const)?\s*\{)");
const std::regex kCall(R"(\b([A-Za-z_]\w*)\s*\()");

bool keyword(const std::string& w) {
    static const std::unordered_set<std::string> kw{"if","for","while","switch",
        "return","sizeof","catch","do","else","case","new","delete","and","or",
        "not","throw","static","const","constexpr","inline","virtual","explicit",
        "template"};
    return kw.count(w) > 0;
}

} // namespace

FileAnalysis parseText(std::string_view content, std::string path, Lang lang) {
    FileAnalysis a;
    a.path = std::move(path);
    a.lang = lang;
    std::string buf(content);
    a.lines = countLines(buf, lang);

    std::istringstream walk(buf);
    std::string line;
    int no = 0;
    std::smatch m;
    while (std::getline(walk, line)) {
        ++no;
        if (lang == Lang::Cpp || lang == Lang::C) {
            if (std::regex_search(line, m, kCInclude)) a.includes.push_back(m[1].str());
            if (std::regex_search(line, m, kFunc)) {
                std::string n = m[1].str();
                if (!keyword(n)) a.functions.push_back({n, no});
            }
        } else if (lang == Lang::Python) {
            if (std::regex_search(line, m, kPyImport))
                a.includes.push_back(m[1].matched ? m[1].str() : m[2].str());
        } else if (lang == Lang::JavaScript || lang == Lang::TypeScript) {
            if (std::regex_search(line, m, kJsImport)) a.includes.push_back(m[1].str());
        }
        for (std::sregex_iterator it(line.begin(), line.end(), kCall), end; it != end; ++it) {
            std::string n = (*it)[1].str();
            if (!keyword(n)) a.calls.push_back(std::move(n));
        }
    }

    if (lang == Lang::Cpp || lang == Lang::C) {
        for (std::sregex_iterator it(buf.begin(), buf.end(), kNamespace), e; it != e; ++it)
            a.namespaces.push_back((*it)[1].str());
        for (std::sregex_iterator it(buf.begin(), buf.end(), kClass), e; it != e; ++it) {
            // Skip "enum class X" - that is an enum, not a class.
            std::string_view before = trim((*it).prefix().str());
            if (endsWith(before, "enum")) continue;
            a.classes.push_back({(*it)[2].str(), (*it)[1].str() == "struct"});
        }
        for (std::sregex_iterator it(buf.begin(), buf.end(), kEnum), e; it != e; ++it)
            a.enums.push_back((*it)[1].str());
        for (std::sregex_iterator it(buf.begin(), buf.end(), kTemplate), e; it != e; ++it)
            ++a.templates;
    }
    return a;
}

AnalysisSet parseRepository(const Repository& repo) {
    AnalysisSet set;
    fs::path root(repo.rootPath());
    for (const auto& [path, node] : repo.index()) {
        Lang lang = langFromExt(node->extension);
        if (lang == Lang::Unknown) continue;
        std::ifstream in(root / fs::path(path), std::ios::binary);
        std::string content;
        if (in) {
            std::ostringstream ss; ss << in.rdbuf(); content = ss.str();
        }
        set.emplace(path, parseText(content, path, lang));
    }
    return set;
}

} // namespace re
