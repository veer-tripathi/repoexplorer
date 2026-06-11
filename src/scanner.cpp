#include "scanner.hpp"

#include <algorithm>
#include <chrono>
#include <system_error>

#include "utils.hpp"

namespace fs = std::filesystem;

namespace re {

void Repository::buildIndex() {
    index_.clear();
    dirCount_ = 0;
    indexDir(root_);
}

void Repository::indexDir(const DirNode& d) {
    for (const FileNode& f : d.files) index_.emplace(f.path, &f);
    for (const auto& s : d.subdirs) { ++dirCount_; indexDir(*s); }
}

const FileNode* Repository::find(const std::string& path) const {
    auto it = index_.find(path);
    return it == index_.end() ? nullptr : it->second;
}

// --- scanning ---------------------------------------------------------------

namespace {

// Names/extensions skipped during scanning.
bool ignored(std::string_view name) {
    static const char* names[] = {".git", ".svn", ".hg", "node_modules", "build",
        "cmake-build-debug", "cmake-build-release", "dist", "target", ".cache",
        "__pycache__", ".idea", ".vscode"};
    static const char* exts[] = {".o", ".obj", ".a", ".so", ".dll", ".exe",
        ".class", ".pyc"};
    for (const char* n : names) if (name == n) return true;
    for (const char* e : exts) if (endsWith(name, e)) return true;
    return false;
}

std::int64_t toEpoch(fs::file_time_type t) {
    using namespace std::chrono;
    auto sys = time_point_cast<system_clock::duration>(
        t - fs::file_time_type::clock::now() + system_clock::now());
    return duration_cast<seconds>(sys.time_since_epoch()).count();
}

std::string join(const std::string& prefix, const std::string& name) {
    return prefix.empty() ? name : prefix + "/" + name;
}

void scanDir(const fs::path& fsPath, const std::string& prefix, int depth,
             const ScanOptions& opt, DirNode& dir) {
    if (opt.maxDepth >= 0 && depth >= opt.maxDepth) return;

    std::error_code ec;
    fs::directory_iterator it(fsPath, fs::directory_options::skip_permission_denied, ec);
    if (ec) return;

    for (const fs::directory_entry& e : it) {
        std::string name = e.path().filename().string();
        if (name.empty()) continue;
        bool hidden = name.front() == '.';
        if (hidden && !opt.includeHidden) continue;
        if (ignored(name)) continue;

        std::error_code se;
        bool isSymlink = e.is_symlink(se);
        if (e.is_directory(se)) {
            if (isSymlink && !opt.followSymlinks) continue;
            auto child = std::make_unique<DirNode>();
            child->name = name;
            child->path = join(prefix, name);
            scanDir(e.path(), child->path, depth + 1, opt, *child);
            dir.subdirs.push_back(std::move(child));
        } else {
            FileNode f;
            f.name = name;
            f.path = join(prefix, name);
            f.isHidden = hidden;
            f.isSymlink = isSymlink;
            f.extension = toLower(e.path().extension().string());
            std::error_code fe;
            f.size = e.is_regular_file(fe) ? fs::file_size(e.path(), fe) : 0;
            if (fe) f.size = 0;
            std::error_code te;
            auto t = fs::last_write_time(e.path(), te);
            f.modified = te ? 0 : toEpoch(t);
            dir.files.push_back(std::move(f));
        }
    }

    std::sort(dir.subdirs.begin(), dir.subdirs.end(),
              [](const auto& a, const auto& b) { return a->name < b->name; });
    std::sort(dir.files.begin(), dir.files.end(),
              [](const FileNode& a, const FileNode& b) { return a.name < b.name; });
}

} // namespace

Repository scan(const fs::path& root, const ScanOptions& opt) {
    Repository repo;
    repo.setRootPath(root.string());
    DirNode& r = repo.root();
    r.name = root.filename().empty() ? root.string() : root.filename().string();
    r.path = "";
    if (fs::exists(root) && fs::is_directory(root))
        scanDir(root, "", 0, opt, r);
    repo.buildIndex();
    return repo;
}

} // namespace re
