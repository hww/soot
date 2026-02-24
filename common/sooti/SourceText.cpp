#include "SourceText.hpp"

namespace script {

SourceText::SourceText(std::string text) : m_text(std::move(text)) {
    build_offsets();
}

void SourceText::build_offsets() {
    m_offset_by_line.clear();
    m_offset_by_line.push_back(0);

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
    int  start_offset = 0; // range.first == 0 ? 1 : 0;
    return m_text.substr(range.first + start_offset,
                         std::max(0, range.second - range.first - start_offset));
}

int SourceText::get_line_idx(int offset) {
    // Проверка на выход за границы текста
    if (offset < 0 || offset >= (int)m_text.size()) {
        throw std::runtime_error("Offset out of bounds: " + std::to_string(offset));
    }

    // Ищем первый элемент, который БОЛЬШЕ нашего offset
    auto it = std::upper_bound(m_offset_by_line.begin(), m_offset_by_line.end(), offset);

    // Индекс строки — это позиция найденного элемента минус 1
    // (Потому что upper_bound нашел начало СЛЕДУЮЩЕЙ строки)
    return std::distance(m_offset_by_line.begin(), it) - 1;
}

int SourceText::get_offset_of_line(int line_idx) {
    return m_offset_by_line.at(line_idx);
}

std::pair<int, int> SourceText::get_containing_line(int offset) {
    for (size_t line = 0; line < m_offset_by_line.size() - 1; line++) {
        if (offset >= m_offset_by_line[line] && offset < m_offset_by_line[line + 1]) {
            return std::make_pair(m_offset_by_line[line], m_offset_by_line[line + 1]);
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
}; // namespace script