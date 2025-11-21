#pragma once

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <optional>

#include "object.h"
#include "common/util/file_util.h"

namespace script {

    class SourceText {
    public:
        explicit SourceText(std::string text);
        SourceText() = default;
        virtual ~SourceText() = default;

        virtual std::string get_description() = 0;
        std::string get_line_containing_offset(int offset);
        int get_line_idx(int offset);
        int get_offset_of_line(int line_idx);
        virtual bool terminate_compiler_error() { return true; }

        const char* get_text() { return m_text.c_str(); }
        int get_size() { return m_text.size(); }

    protected:
        void build_offsets();
        std::string m_text;
        std::vector<int> m_offset_by_line;
        std::pair<int, int> get_containing_line(int offset);
    };

    class ReplText : public SourceText {
    public:
        explicit ReplText(const std::string& text) : SourceText(text) {}
        ReplText() = default;
        std::string get_description() override { return "REPL"; }
  ~     ReplText() = default;
    };

    class ProgramString : public SourceText {
    public:
        explicit ProgramString(const std::string& text, const std::string& string_name = "Program string")
            : SourceText(text), m_string_name(string_name) {
        }
        ProgramString() = default;
        std::string get_description() override { return m_string_name; }
        ~ProgramString() = default;

    private:
        std::string m_string_name;
    };

    class FileText : public SourceText {
    public:
        FileText(const std::string& file_path, const std::string& description_name);
        std::string get_description() override { return m_desc_name; }
        FileText() = default;
        ~FileText() = default;

    private:
        std::string m_filepath;
        std::string m_desc_name;
    };

    struct TextRef {
        int offset;
        std::shared_ptr<SourceText> frag;
    };

    class TextDb {
    public:
        struct ShortInfo {
            std::string filename;
            int line_idx_to_display = -1;
            int pos_in_line = -1;
            std::string line_text;

            bool operator==(const ShortInfo& other) const {
                return line_idx_to_display == other.line_idx_to_display &&
                    pos_in_line == other.pos_in_line &&
                    filename == other.filename;
            }

            bool operator!=(const ShortInfo& other) const {
                return !(*this == other);
            }

            bool operator<(const ShortInfo& other) const {
                if (filename != other.filename) return filename < other.filename;
                if (line_idx_to_display != other.line_idx_to_display) return line_idx_to_display < other.line_idx_to_display;
                return pos_in_line < other.pos_in_line;
            }
        };
        void insert(const std::shared_ptr<SourceText>& frag);
        void link(const Object& o, std::shared_ptr<SourceText> frag, int offset);
        std::string get_info_for(const Object& o, bool* terminate_compiler_error = nullptr) const;
        std::optional<ShortInfo> get_short_info_for(const Object& o) const;
        std::string get_info_for(const std::shared_ptr<SourceText>& frag, int offset) const;
        std::optional<ShortInfo> get_short_info_for(const std::shared_ptr<SourceText>& frag, int offset) const;
        std::optional<ShortInfo> try_get_short_info(const Object& o) const;
        std::optional<ShortInfo> try_get_short_info(const std::shared_ptr<HeapObject>& o) const;

        bool has_info(const Object& o) const;
        void inherit_info(const Object& parent, const Object& child);
        void clear_info();

        // Добавляем методы для инспекции
        size_t get_fragment_count() const { return m_fragments.size(); }
        size_t get_object_count() const { return m_map.size(); }

        const auto& get_fragments() const { return m_fragments; }
        const auto& get_mapping() const { return m_map; }

        std::vector<std::string> get_fragment_descriptions() const {
            std::vector<std::string> result;
            for (const auto& frag : m_fragments) {
                result.push_back(frag->get_description());
            }
            return result;
        }
    private:
        std::vector<std::shared_ptr<SourceText>> m_fragments;
        std::unordered_map<std::shared_ptr<HeapObject>, TextRef> m_map;
    };

} // namespace sсript