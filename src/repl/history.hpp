#pragma once
#include <vector>
#include <string>
#include <fstream>

class History {
private:
    std::vector<std::string> commands_;
    std::string filename_;
    size_t max_size_ = 1000;

public:
    History(const std::string& filename = ".aleste_history") : filename_(filename) {}
    
    void load() {
        std::ifstream file(filename_);
        if (!file) return;
        
        std::string line;
        while (std::getline(file, line) && commands_.size() < max_size_) {
            if (!line.empty()) {
                commands_.push_back(line);
            }
        }
    }
    
    void save() {
        std::ofstream file(filename_);
        if (!file) return;
        
        for (const auto& cmd : commands_) {
            file << cmd << "\n";
        }
    }
    
    void add(const std::string& command) {
        if (!command.empty()) {
            commands_.push_back(command);
            if (commands_.size() > max_size_) {
                commands_.erase(commands_.begin());
            }
        }
    }
    
    std::string get(int index) const {
        if (index < 0 || index >= commands_.size()) return "";
        return commands_[index];
    }
    
    size_t size() const { return commands_.size(); }
    
    void set_max_size(size_t size) { max_size_ = size; }
};