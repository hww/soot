#include "source_info.h"
#include "object.h"
#include <sstream>
#include <algorithm>

namespace script 
{
    SourceText::SourceText(std::string text) : m_text(std::move(text)) {
        build_line_offsets();
    }
    void SourceText::build_line_offsets() {
        m_line_offsets.clear();
        m_line_offsets.push_back(0);

        for (size_t i = 0; i < m_text.size(); ++i) {  // Change int to size_t
            if (m_text[i] == '\n') {
                m_line_offsets.push_back(i + 1);
            }
        }
    }


    std::string SourceText::get_line_containing_offset(int offset) const {
        int line_idx = get_line_number(offset);
        if (line_idx < 0 || static_cast<size_t>(line_idx) >= m_line_offsets.size()) {
            return "";
        }

        int line_start = m_line_offsets[line_idx];
        int line_end = (static_cast<size_t>(line_idx + 1) < m_line_offsets.size())
            ? m_line_offsets[line_idx + 1] - 1
            : static_cast<int>(m_text.size());

        return m_text.substr(line_start, line_end - line_start);
    }

    int SourceText::get_line_number(int offset) const {
        if (offset < 0 || static_cast<size_t>(offset) > m_text.size()) return -1;

        auto it = std::upper_bound(m_line_offsets.begin(), m_line_offsets.end(), offset);
        return std::distance(m_line_offsets.begin(), it) - 1;
    }

    int SourceText::get_column_number(int offset) const {
        int line_idx = get_line_number(offset);
        if (line_idx < 0) return -1;

        int line_start = m_line_offsets[line_idx];
        return offset - line_start;
    }

    // FileSource implementation
    FileSource::FileSource(const std::string& filename, const std::string& file_content)
        : SourceText(file_content), m_filename(filename) {
    }

    // ReplSource implementation  
    ReplSource::ReplSource(const std::string& text) : SourceText(text) {}

    // StringSource implementation
    StringSource::StringSource(const std::string& text, const std::string& name)
        : SourceText(text), m_name(name) {
    }

    // SourceManager implementation
    void SourceManager::register_source(std::shared_ptr<SourceText> source) {
        m_sources.push_back(std::move(source));
    }

    void SourceManager::link_object(const Object& obj, std::shared_ptr<SourceText> source, int offset) {
        if (obj.heap_obj) {
            link_heap_object(obj.heap_obj.get(), std::move(source), offset);
        }
    }

    void SourceManager::link_heap_object(const HeapObject* obj, std::shared_ptr<SourceText> source, int offset) {
        m_location_map[obj] = SourceLocation{ offset, std::move(source) };
    }

    std::string SourceManager::get_info_for(const Object& obj) const {
        auto short_info = get_short_info(obj);
        if (!short_info) {
            return "unknown source";
        }

        std::ostringstream oss;
        oss << short_info->filename;
        if (short_info->line != -1) {
            oss << ":" << (short_info->line + 1); // Convert to 1-based line numbers
            if (short_info->column != -1) {
                oss << ":" << (short_info->column + 1); // Convert to 1-based columns
            }
        }

        if (!short_info->line_text.empty()) {
            oss << "\n  " << short_info->line_text << "\n  "
                << std::string(short_info->column, ' ') << "^";
        }

        return oss.str();
    }

    std::optional<SourceManager::ShortInfo> SourceManager::get_short_info(const Object& obj) const {
        if (!obj.heap_obj) {
            return std::nullopt;
        }

        auto it = m_location_map.find(obj.heap_obj.get());
        if (it == m_location_map.end()) {
            return std::nullopt;
        }

        return get_short_info_for_location(it->second);
    }

    bool SourceManager::has_info(const Object& obj) const {
        return obj.heap_obj && m_location_map.find(obj.heap_obj.get()) != m_location_map.end();
    }

    void SourceManager::inherit_info(const Object& parent, const Object& child) {
        if (!parent.heap_obj || !child.heap_obj) return;

        auto it = m_location_map.find(parent.heap_obj.get());
        if (it != m_location_map.end()) {
            m_location_map[child.heap_obj.get()] = it->second;
        }
    }

    std::optional<SourceManager::ShortInfo> SourceManager::get_short_info_for_location(const SourceLocation& loc) const {
        if (!loc.source) return std::nullopt;

        ShortInfo info;
        info.filename = loc.source->get_description();
        info.line = loc.source->get_line_number(loc.offset);
        info.column = loc.source->get_column_number(loc.offset);

        if (info.line != -1) {
            info.line_text = loc.source->get_line_containing_offset(loc.offset);
        }

        return info;
    }
} // namespace script