#ifndef BOX_FILE_RESOLVER_HPP
#define BOX_FILE_RESOLVER_HPP

#include <string>
#include <vector>
#include <unordered_set>
#include <filesystem>
#include <optional>

namespace box {

class FileResolver {
private:
    std::unordered_set<std::string> processing_stack;
    std::unordered_set<std::string> resolved_files;
    std::string current_directory;
    
    std::string normalize_path(const std::string& path) const;
    std::string get_absolute_path(const std::string& path) const;
    
public:
    explicit FileResolver(const std::string& base_directory);
    
    std::optional<std::string> resolve_import(const std::string& import_path, 
                                              const std::string& importing_file);
    
    bool is_processing(const std::string& file_path) const;
    bool is_resolved(const std::string& file_path) const;
    
    void begin_processing(const std::string& file_path);
    void end_processing(const std::string& file_path);
    void mark_resolved(const std::string& file_path);
    
    std::vector<std::string> get_processing_stack_vector() const;
    void clear();
};

}

#endif
