#include "TextDb.hpp"
#include "common/util/FileUtil.hpp"
#include "fmt/args.h"
#include "fmt/base.h"
#include "fmt/color.h"
#include "fmt/format.h"
#include <algorithm>
#include <sstream>
#include <stdexcept>

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

/*!
 * Inform the TextDB about a source of text.
 */
void TextDb::insert(const std::shared_ptr<SourceText> &frag) {
    m_fragments.push_back(frag);
}

/*!
 * Link the GOOS object o to the offset into the given text fragment.
 * The object _must_ be a pair or empty list.
 */
void TextDb::link(const Object &o, std::shared_ptr<SourceText> frag, int offset) {
    if (o.is_null())
        return;

    ASSERT(o.is_pair());

    if (false) {
        std::string item = o.print();
        std::string text = frag->get_text();
        fmt::print(fg(fmt::color::orange), "<item heap={:p} offset=[{}] item={}>{}\n",
                   static_cast<void *>(o.heap_obj.get()), // Получаем сырой указатель из shared_ptr
                   offset,
                   item.substr(0, 16)
                       .c_str(), // Заменил на print(), если inspect_short капризничает с const
                   text.substr(0, 70).c_str());
    }
    TextRef ref;
    ref.offset = offset;
    ref.frag = std::move(frag);
    m_map[o.heap_obj] = ref;
}

// Ассоцировать новый объект со старыми данными другого объекта
void TextDb::copy_link(const Object &from, const Object &to) {
    if (from.is_null() || to.is_null())
        return;
    auto it = m_map.find(from.heap_obj);
    if (it != m_map.end()) {
        m_map[to.heap_obj] = it->second;
    }
}
/**
 * @brief Генерирует детализированный строковый отчет о расположении объекта в исходном коде.
 * * Метод выполняет роль диспетчера: он определяет тип объекта (LexToken или Pair) и пытается
 * сопоставить его с метаданными, хранящимися в базе данных (TextDb).
 * * @param o Объект, для которого запрашивается информация (обычно LexToken или Pair).
 * @param terminate_compiler_error [out] Указатель на булеву пcopy_linkеременную, определяющую
 * критичность ошибки.
 * * ### О параметре terminate_compiler_error:
 * Этот флаг позволяет вызывающей стороне (например, компилятору или REPL) понять, можно ли
 * игнорировать данную ошибку или она является фатальной для текущего контекста:
 * * - **true (Fatal):** Ошибка произошла в контексте, который делает дальнейшую сборку или
 * выполнение невозможным. Например, ошибка в основном скрипте или системной библиотеке.
 * Интерпретатор должен немедленно прекратить работу.
 * * - **false (Recoverable):** Ошибка произошла в "мягком" контексте. Например, в REPL, где мы
 * хотим просто вывести сообщение и позволить пользователю ввести новую команду, не убивая
 * весь процесс.
 * * @return std::string Отформатированный блок текста со ссылкой на файл, номером строки,
 * исходным кодом и визуальным указателем (стрелкой ^) под объектом.
 * Возвращает "?", если объект не найден в базе данных.
 */
std::string TextDb::get_info_for(const Object &o, bool *terminate_compiler_error) const {
    if (o.is_pair()) {
        // fmt::print(fg(fmt::color::orange), "<item heap={:p}>\n",
        // static_cast<void*>(o.heap_obj.get()));

        auto kv = m_map.find(o.heap_obj);
        if (kv != m_map.end()) {
            if (terminate_compiler_error) {
                *terminate_compiler_error = kv->second.frag->terminate_compiler_error();
            }
            return get_info_for(kv->second.frag, kv->second.offset);
        } else {
            if (terminate_compiler_error) {
                *terminate_compiler_error = false;
            }
            return "?";
        }
    } else {
        if (terminate_compiler_error) {
            *terminate_compiler_error = false;
        }
        return "?";
    }
}

std::optional<ShortInfo> TextDb::get_short_info_for(const Object &o) const {
    if (o.is_pair()) {
        auto kv = m_map.find(o.heap_obj);
        if (kv != m_map.end()) {
            return get_short_info_for(kv->second.frag, kv->second.offset);
        } else {
            return {};
        }
    } else {
        return {};
    }
}

std::optional<TextRef> TextDb::get_text_ref(const Object &o) const {
    if (o.is_pair()) {
        auto kv = m_map.find(o.heap_obj);
        if (kv != m_map.end()) {
            return kv->second;
        } else {
            return {};
        }
    } else {
        return {};
    }
}

/*!
 * Given a source text and an offset, print a description of where it is.
 */
std::string TextDb::get_info_for(const std::shared_ptr<SourceText> &frag, int offset) const {
    // ЗАЩИТА: Если смещение вылетело за пределы, прижимаем его к последнему символу
    offset = std::max(0, std::min(offset, (int)frag->get_size() - 1));

    int line_idx = frag->get_line_idx(offset);

    // Формат: "filename:line" (выделяем тусклым)
    std::string result =
        fmt::format(fg(fmt::color::gray), "  at {}:{}\n", frag->get_description(), line_idx + 1);

    // Сама строка кода
    std::string line = frag->get_line_containing_offset(offset);
    result += "    " + line;
    if (result[result.size() - 1] != '\n')
        result += "\n";

    // 1. Вычисляем реальный отступ внутри строки
    int offset_in_line = offset - frag->get_offset_of_line(line_idx);
    if (offset_in_line < 0)
        offset_in_line = 0;

    // 2. Создаем строку пробелов нужной длины (4 базовых + смещение в строке)
    std::string spaces(4 + offset_in_line, ' ');

    // 3. Печатаем стрелку с правильным отступом
    std::string pointer = spaces + fmt::format(fg(fmt::color::red) | fmt::emphasis::bold, "^\n");

    return result + pointer;
}

std::optional<ShortInfo> TextDb::get_short_info_for(const std::shared_ptr<SourceText> &frag,
                                                    int offset) const {
    int line_idx = frag->get_line_idx(offset);
    int offset_in_line = std::max(offset - frag->get_offset_of_line(line_idx), 1) - 1;

    ShortInfo info;
    info.filename = frag->get_description();
    info.line_idx_to_display = line_idx;
    info.pos_in_line = offset_in_line;
    info.line_text = frag->get_line_containing_offset(offset);
    return std::make_optional(info);
}

std::optional<ShortInfo> TextDb::try_get_short_info(const std::shared_ptr<HeapObject> &o) const {
    auto it = m_map.find(o);
    if (it != m_map.end()) {
        auto       &frag = it->second.frag;
        std::string name = frag->get_description();

        // Shorten path
        size_t start = 0;
        for (size_t i = 0; i < name.size(); i++) {
            if (name[i] == '/' || name[i] == '\\') {
                start = i + 1;
            }
        }
        if (start < name.size()) {
            name = name.substr(start);
        }

        ShortInfo result;
        result.filename = name;

        int line_idx = frag->get_line_idx(it->second.offset);
        result.line_idx_to_display = line_idx + 1;

        int offset_of_line = frag->get_offset_of_line(line_idx);
        int offset_of_next_line = frag->get_offset_of_line(line_idx + 1);

        int line_length = offset_of_next_line - offset_of_line;

        int start_offset_in_line = it->second.offset - offset_of_line - 1;
        result.pos_in_line = std::max(start_offset_in_line, 0);
        result.line_text = std::string(frag->get_text() + offset_of_line + 1, line_length - 1);
        return result;
    }
    return {};
}

std::optional<ShortInfo> TextDb::try_get_short_info(const Object &o) const {
    if (o.is_pair()) {
        return try_get_short_info(o.heap_obj);
    }
    return {};
}

bool TextDb::has_info(const Object &o) const {
    return o.is_pair() && (m_map.find(o.heap_obj) != m_map.end());
}

/*!
 * Make child have the same location in the source as parent.  For example, if parent generates
 * code that we want to be associated with the parent's location in source.
 *
 * Note: this only has an effect if both parent and child are pair/list. Otherwise it does nothing.
 */
void TextDb::inherit_info(const Object &parent, const Object &child) {
    if (parent.is_pair() && child.is_pair()) {
        auto parent_kv = m_map.find(parent.heap_obj);
        if (parent_kv != m_map.end()) {
            std::vector<const Object *> children = {&child};
            // mark all forms as children. This will help with error messages in macros, and makes
            // (add-macro-to-autocomplete) work properly.
            while (!children.empty()) {
                auto top = children.back();
                children.pop_back();
                if (m_map.insert({top->heap_obj, parent_kv->second}).second) {
                    if (top->as_pair()->car.is_pair()) {
                        children.push_back(&top->as_pair()->car);
                    }
                    if (top->as_pair()->cdr.is_pair()) {
                        children.push_back(&top->as_pair()->cdr);
                    }
                }
            }
        }
    }
}

void TextDb::clear_info() {
    m_map.clear();
    m_fragments.clear();
}

} // namespace script
