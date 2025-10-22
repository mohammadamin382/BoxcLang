#include "file_resolver.hpp"
#include <algorithm>

namespace fs = std::filesystem;

namespace box {

FileResolver::FileResolver(const std::string& base_directory)
    : current_directory(base_directory) {}

std::string FileResolver::normalize_path(const std::string& path) const {
    fs::path p(path);
    return fs::canonical(fs::absolute(p)).string();
}

std::string FileResolver::get_absolute_path(const std::string& path) const {
    fs::path p(path);
    if (p.is_absolute()) {
        return p.string();
    }
    return (fs::path(current_directory) / p).string();
}

std::optional<std::string> FileResolver::resolve_import(const std::string& import_path, 
                                                         const std::string& importing_file) {
    fs::path importing_dir = fs::path(importing_file).parent_path();
    if (importing_dir.empty()) {
        importing_dir = current_directory;
    }
    
    fs::path resolved_path = importing_dir / import_path;
    
    if (!fs::exists(resolved_path)) {
        resolved_path = fs::path(current_directory) / import_path;
    }
    
    if (!fs::exists(resolved_path)) {
        return std::nullopt;
    }
    
    std::string normalized = normalize_path(resolved_path.string());
    return normalized;
}

bool FileResolver::is_processing(const std::string& file_path) const {
    std::string normalized = normalize_path(file_path);
    return processing_stack.count(normalized) > 0;
}

bool FileResolver::is_resolved(const std::string& file_path) const {
    std::string normalized = normalize_path(file_path);
    return resolved_files.count(normalized) > 0;
}

void FileResolver::begin_processing(const std::string& file_path) {
    std::string normalized = normalize_path(file_path);
    processing_stack.insert(normalized);
}

void FileResolver::end_processing(const std::string& file_path) {
    std::string normalized = normalize_path(file_path);
    processing_stack.erase(normalized);
}

void FileResolver::mark_resolved(const std::string& file_path) {
    std::string normalized = normalize_path(file_path);
    resolved_files.insert(normalized);
}

std::vector<std::string> FileResolver::get_processing_stack_vector() const {
    return std::vector<std::string>(processing_stack.begin(), processing_stack.end());
}

void FileResolver::clear() {
    processing_stack.clear();
    resolved_files.clear();
}

}
