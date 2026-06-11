#pragma once
// Repository scanner + data models. Walks a directory with std::filesystem and
// builds a tree of nodes plus a flat path->file index.
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace re {

struct FileNode {
    std::string name;          // "main.cpp"
    std::string extension;     // ".cpp" (lowercased, with dot)
    std::string path;          // "src/main.cpp" (always '/')
    std::uintmax_t size = 0;
    std::int64_t modified = 0; // unix seconds
    bool isSymlink = false;
    bool isHidden = false;
};

struct DirNode {
    std::string name;
    std::string path;
    std::vector<std::unique_ptr<DirNode>> subdirs;
    std::vector<FileNode> files;
};

// Owns the tree root and a flat index. Move-only: never deep-copied.
class Repository {
public:
    Repository() = default;
    Repository(const Repository&) = delete;
    Repository& operator=(const Repository&) = delete;
    Repository(Repository&&) = default;
    Repository& operator=(Repository&&) = default;

    DirNode& root() { return root_; }
    const DirNode& root() const { return root_; }
    const std::string& rootPath() const { return rootPath_; }
    void setRootPath(std::string p) { rootPath_ = std::move(p); }

    void buildIndex();
    const FileNode* find(const std::string& path) const;
    const std::unordered_map<std::string, const FileNode*>& index() const { return index_; }
    std::size_t fileCount() const { return index_.size(); }
    std::size_t dirCount() const { return dirCount_; }

private:
    void indexDir(const DirNode& d);
    DirNode root_;
    std::string rootPath_;
    std::unordered_map<std::string, const FileNode*> index_;
    std::size_t dirCount_ = 0;
};

struct ScanOptions {
    int maxDepth = -1;          // -1 = unlimited
    bool includeHidden = true;
    bool followSymlinks = false;
};

// Scan a path into a Repository, skipping common noise (.git, build, *.o, ...).
Repository scan(const std::filesystem::path& root, const ScanOptions& opt = {});

} // namespace re
