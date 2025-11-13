#ifndef SOURCE_INFO_H
#define SOURCE_INFO_H

#include <string>
#include <unordered_map>
#include <memory>

struct SourceInfo {
    std::string filename;
    int line;
    int column;
    std::string line_text;  // текст строки для отображения
    
    SourceInfo(const std::string& file = "", int ln = 0, int col = 0, const std::string& text = "")
        : filename(file), line(ln), column(col), line_text(text) {}
    
    std::string to_string() const {
        if (filename.empty()) return "unknown source";
        return filename + ":" + std::to_string(line) + ":" + std::to_string(column);
    }
    
    // Для ошибок с контекстом
    std::string format_error(const std::string& message) const {
        std::string result = "Error at " + to_string() + ": " + message + "\n";
        if (!line_text.empty()) {
            result += "  " + line_text + "\n";
            result += "  " + std::string(column, ' ') + "^\n";  // Добавляем отступ
        }
        return result;
    }
};

// Простая база исходников - отслеживает происхождение AST узлов
class SourceDB {
    std::unordered_map<const void*, SourceInfo> source_map;
    
public:
    // Связать AST узел с информацией об исходнике
    void link(const void* ast_node, const SourceInfo& info) {
        source_map[ast_node] = info;
    }
    
    // Получить информацию об исходнике для AST узла
    SourceInfo get_info(const void* ast_node) const {
        auto it = source_map.find(ast_node);
        return it != source_map.end() ? it->second : SourceInfo();
    }
    
    // Проверить есть ли информация об узле
    bool has_info(const void* ast_node) const {
        return source_map.find(ast_node) != source_map.end();
    }
    
    // Наследование позиции (для макросов)
    void inherit_info(const void* parent, const void* child) {
        auto it = source_map.find(parent);
        if (it != source_map.end()) {
            source_map[child] = it->second;
        }
    }
    
    // Очистка базы (для тестов)
    void clear() {
        source_map.clear();
    }

    // УДАЛЯЕМ format_error из SourceDB - он должен быть только в SourceInfo
    // SourceDB только хранит информацию, а форматирование делает SourceInfo
};

// Глобальная база исходников
extern SourceDB g_source_db;

#endif