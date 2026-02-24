#pragma once

#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "Object.hpp"
#include "SourceText.hpp"
#include "common/util/FileUtil.hpp"

namespace script {

struct ShortInfo {
    std::string filename;
    std::string line_text;
    int         line_idx_to_display = -1;
    int         pos_in_line = -1;

    bool operator==(const ShortInfo &other) const {
        return line_idx_to_display == other.line_idx_to_display &&
               pos_in_line == other.pos_in_line && filename == other.filename;
    }

    bool operator!=(const ShortInfo &other) const {
        return !(*this == other);
    }

    bool operator<(const ShortInfo &other) const {
        if (filename != other.filename)
            return filename < other.filename;
        if (line_idx_to_display != other.line_idx_to_display)
            return line_idx_to_display < other.line_idx_to_display;
        return pos_in_line < other.pos_in_line;
    }

    static ShortInfo empty() {
        return {};
    }

    std::string print(bool detailed = false) const {
        if (line_idx_to_display == -1) {
            return "at unknown location";
        }

        // Краткий формат: "at filename:line:pos"
        std::string result = fmt::format("at {}:{}:{}", filename, line_idx_to_display, pos_in_line);

        // Если нужен детальный вывод (со строкой кода и стрелочкой)
        if (detailed && !line_text.empty()) {
            result += "\n    " + line_text + "\n    ";

            // Рисуем стрелочку точно под символом
            for (int i = 0; i < pos_in_line; ++i) {
                // Учитываем табуляцию, если она есть в исходнике
                if (line_text[i] == '\t')
                    result += "    ";
                else
                    result += " ";
            }
            result += "\033[1;31m^\033[0m"; // Красная стрелочка
        }

        return result;
    }
};

class TextDb {
  public:
    void        insert(const std::shared_ptr<SourceText> &frag);
    void        link(const Object &o, std::shared_ptr<SourceText> frag, int offset);
    void        copy_link(const Object &from, const Object &to);
    std::string get_info_for(const Object &o, bool *terminate_compiler_error = nullptr) const;
    std::optional<ShortInfo> get_short_info_for(const Object &o) const;
    std::optional<TextRef>   get_text_ref(const Object &o) const;

    std::string get_info_for(const std::shared_ptr<SourceText> &frag, int offset) const;
    std::optional<ShortInfo> get_short_info_for(const std::shared_ptr<SourceText> &frag,
                                                int                                offset) const;
    std::optional<ShortInfo> try_get_short_info(const Object &o) const;
    std::optional<ShortInfo> try_get_short_info(const std::shared_ptr<HeapObject> &o) const;

    bool has_info(const Object &o) const;
    void inherit_info(const Object &parent, const Object &child);
    void clear_info();

    // Добавляем методы для инспекции
    size_t get_fragment_count() const {
        return m_fragments.size();
    }
    size_t get_object_count() const {
        return m_map.size();
    }

    const auto &get_fragments() const {
        return m_fragments;
    }
    const auto &get_mapping() const {
        return m_map;
    }

    std::vector<std::string> get_fragment_descriptions() const {
        std::vector<std::string> result;
        for (const auto &frag : m_fragments) {
            result.push_back(frag->get_description());
        }
        return result;
    }

  private:
    std::vector<std::shared_ptr<SourceText>>                 m_fragments;
    std::unordered_map<std::shared_ptr<HeapObject>, TextRef> m_map;
};

} // namespace script
