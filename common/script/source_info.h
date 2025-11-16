#pragma once

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <optional>

namespace script {
    // Базовый класс для исходного текста (аналог SourceText)
    class SourceText {
    public:
        explicit SourceText(std::string text);
        virtual ~SourceText() = default;

        virtual std::string get_description() const = 0;
        std::string get_line_containing_offset(int offset) const;
        int get_line_number(int offset) const;
        int get_column_number(int offset) const;

        const char* get_text() const { return m_text.c_str(); }
        int get_size() const { return m_text.size(); }

        std::pair<int, int> get_line_and_column(int offset) const {
            int line = get_line_number(offset);
            int column = get_column_number(offset);
            return { line, column };
        }
    protected:
        void build_line_offsets();
        std::string m_text;
        std::vector<int> m_line_offsets;
    };

    // Текст из файла
    class FileSource : public SourceText {
    public:
        FileSource(const std::string& filename, const std::string& file_content);
        std::string get_description() const override { return m_filename; }

    private:
        std::string m_filename;
    };

    // Текст из REPL
    class ReplSource : public SourceText {
    public:
        explicit ReplSource(const std::string& text);
        std::string get_description() const override { return "REPL"; }
    };

    // Текст из строки программы
    class StringSource : public SourceText {
    public:
        StringSource(const std::string& text, const std::string& name = "string");
        std::string get_description() const override { return m_name; }

    private:
        std::string m_name;
    };

    // Ссылка на позицию в исходном коде (аналог TextRef)
    struct SourceLocation {
        int offset;
        std::shared_ptr<SourceText> source;
    };

    // Главный менеджер исходного кода (аналог TextDb)
    class SourceManager {
    public:
        struct ShortInfo {
            std::string description;
            int line = -1;
            int column = -1;
            std::string line_text;
        };

        // Регистрация исходников
        void register_source(std::shared_ptr<SourceText> source);

        // Привязка объектов к исходному коду
        void link_object(const class Object& obj, std::shared_ptr<SourceText> source, int offset);
        void link_heap_object(const class HeapObject* obj, std::shared_ptr<SourceText> source, int offset);

        // Получение информации
        std::string get_info_for(const class Object& obj) const;
        std::optional<ShortInfo> get_short_info(const class Object& obj) const;
        bool has_info(const class Object& obj) const;

        // Наследование информации (для макросов и т.д.)
        void inherit_info(const class Object& parent, const class Object& child);

    private:
        std::vector<std::shared_ptr<SourceText>> m_sources;
        std::unordered_map<const HeapObject*, SourceLocation> m_location_map;

        std::optional<ShortInfo> get_short_info_for_location(const SourceLocation& loc) const;
    };
} // namespace script