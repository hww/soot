#pragma once

#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "common/sooti/Object.hpp"
#include "common/util/FileUtil.hpp"

namespace script {

class SourceText {
  public:
    // Создает объект из сырой строки текста
    explicit SourceText(std::string text);
    SourceText() = default;
    virtual ~SourceText() = default;

    // Возвращает имя источника (путь к файлу или "REPL"). Чисто виртуальный метод.
    virtual std::string get_description() = 0;

    // Возвращает содержимое всей строки текста, в которую попал offset
    std::string get_line_containing_offset(int offset);

    // Преобразует абсолютную позицию в тексте (offset) в индекс строки (0..N)
    int get_line_idx(int offset);

    // Возвращает абсолютную позицию (offset) начала строки по её индексу
    int get_offset_of_line(int line_idx);

    // Флаг: должна ли ошибка в этом тексте приводить к остановке компиляции/работы
    virtual bool terminate_compiler_error() {
        return true;
    }

    // Доступ к сырому C-style указателю на текст
    const char *get_text() {
        return m_text.c_str();
    }

    // Общий размер текста в байтах
    int get_size() {
        return m_text.size();
    }

    bool has_bom() {
        return (m_text.size() >= 3) && (uint8_t)m_text[0] == 0xEF && (uint8_t)m_text[1] == 0xBB &&
               (uint8_t)m_text[2] == 0xBF;
    }

  protected:
    // Сканирует m_text и заполняет таблицу смещений строк m_offset_by_line
    void build_offsets();

    // Находит пару (начало_строки, конец_строки) для заданного offset
    std::pair<int, int> get_containing_line(int offset);

    // Буфер, хранящий полный текст исходного кода
    std::string m_text;

    // Таблица смещений: m_offset_by_line[i] — это позиция начала i-й строки в m_text
    std::vector<int> m_offset_by_line;
};

class ReplText : public SourceText {
  public:
    explicit ReplText(const std::string &text) : SourceText(text) {}
    ReplText() = default;
    std::string get_description() override {
        return "REPL";
    }
    ~ReplText() = default;
};

class ProgramString : public SourceText {
  public:
    explicit ProgramString(const std::string &text,
                           const std::string &string_name = "Program string")
        : SourceText(text), m_string_name(string_name) {}
    ProgramString() = default;
    std::string get_description() override {
        return m_string_name;
    }
    ~ProgramString() = default;

  private:
    std::string m_string_name;
};

class FileText : public SourceText {
  public:
    // file_path - absolute file path
    // description_name - relative file path
    FileText(const std::string &file_path, const std::string &description_name);
    std::string get_description() override {
        return m_desc_name;
    }
    FileText() = default;
    ~FileText() = default;

  private:
    std::string m_filepath;  // absolute file path
    std::string m_desc_name; // relative file path
};

struct TextRef {
    int                         offset;
    std::shared_ptr<SourceText> frag;
    static TextRef              empty() {
        return {};
    }
};

}; // namespace script
