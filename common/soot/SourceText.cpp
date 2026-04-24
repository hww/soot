#include "SourceText.hpp"
#include "common/util/FileUtil.hpp"

namespace soot {

/*!
 * Initialize with the given string
 */
SourceText::SourceText(std::string text) : m_text(std::move(text)) {
    build_offsets();
}

/*!
 * Update line break data. Should be called any time the text is updated.
 * N.B. Each line start with first visible character
 */
void SourceText::build_offsets() {
    m_offset_by_line.clear();
    m_offset_by_line.push_back(has_bom() ? 3 : 0);

    for (size_t i = 0; i < m_text.size(); i++) {
        if (m_text[i] == '\n') {
            m_offset_by_line.push_back(i + 1);
        }
    }
    m_offset_by_line.push_back(m_text.size());
}

/*!
 * Get the text of the line containing the character at position "offset" from this source.
 */
std::string SourceText::get_line_containing_offset(int offset) {
    auto range = get_containing_line(offset);
    return m_text.substr(range.first, std::max(0, range.second - range.first));
}

/*!
 * Get the index of the line containing the character at position "offset".
 * O(n_lines) crappy implementation.
 * Error if not found.
 * N.B. The line number starts from 0
 */
int SourceText::get_line_idx(int offset) {
    if (m_offset_by_line.size() > 0) {
        if (m_offset_by_line[0] > offset)
            // the offset inside the BOM
            return 0;

        for (uint32_t line = 0; line < m_offset_by_line.size() - 1; line++) {
            if (offset >= m_offset_by_line[line] && offset < m_offset_by_line[line + 1]) {
                return line;
            }
        }
    }
    throw std::runtime_error("Unable to get line index for character at position " +
                             std::to_string(offset));
}

int SourceText::get_offset_of_line(int line_idx) {
    return m_offset_by_line.at(line_idx);
}
/*!
 * Gets the [start, end) character offset of the line containing the given offset.
 */
std::pair<int, int> SourceText::get_containing_line(int offset) {

    if (m_offset_by_line[0] > offset)
        // asjust for UTF8 BOM
        offset = m_offset_by_line[0];

    for (size_t line = 0; line < m_offset_by_line.size() - 1; line++) {
        if (offset >= m_offset_by_line[line] && offset < m_offset_by_line[line + 1]) {
            // original code retured next line start for the end
            return std::make_pair(m_offset_by_line[line], m_offset_by_line[line + 1] - 1);
        }
    }
    return std::make_pair(0, static_cast<int>(m_text.size()));
}

/*!
 * Read text from a file.
 */
FileText::FileText(const std::string &file_path, const std::string &description_name)
    : m_filepath(file_path), m_desc_name(description_name) {
    m_text = file_util::read_text(m_filepath);
    build_offsets();
}
}; // namespace soot