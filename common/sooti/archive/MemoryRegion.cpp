#include "MemoryRegion.hpp"
#include <fstream>

namespace script {
Object MemoryRegion::get_at(const Object &key) {
    // Системные свойства
    if (key.is_symbol() || key.is_string()) {
        std::string name = key.to_std_string();

        if (name == ":size" || name == "size")
            return Object::make_integer(size());
        if (name == ":base" || name == "base" || name == ":origin")
            return Object::make_integer(base());
        if (name == ":type")
            return Object::make_string(class_name());
        if (name == ":start-addr")
            return Object::make_integer(base());
        if (name == ":end-addr")
            return Object::make_integer(base() + size());

        return Object::make_null();
    }

    // Доступ по смещению - возвращаем байт как число
    if (key.is_integer()) {
        size_t offset = key.as_integer() - base();
        if (offset >= size()) {
            return Object::make_none();
        }
        return Object::make_integer(m_data[offset]);
    }

    throw std::runtime_error(fmt::format("MemoryRegion: unknown key {}", key.print()));
}

void MemoryRegion::set_at(const Object &key, const Object &value) {
    // Доступ по смещению
    if (key.is_integer()) {
        size_t offset = key.as_integer() - base();
        if (offset >= m_data.size()) {
            throw std::runtime_error("MemoryRegion::set_at: offset out of bounds");
        }

        // Проверяем значение
        if (!value.is_number()) {
            throw std::runtime_error("MemoryRegion::set_at: expected number");
        }

        // Записываем байт
        m_data[offset] = static_cast<uint8_t>(value.as_integer());
        return;
    }

    // Системные свойства только для чтения
    if (key.is_symbol() || key.is_string()) {
        std::string name = key.to_std_string();
        if (name == ":size" || name == "size" || name == ":base" || name == "base" ||
            name == ":origin" || name == ":type" || name == ":start-addr" || name == ":end-addr") {
            throw std::runtime_error("MemoryRegion::set_at: cannot set read-only property: " +
                                     name);
        }
    }

    throw std::runtime_error(fmt::format("MemoryRegion::set_at: invalid key {}", key.print()));
}
/*!
 * Вспомогательная функция для расчета контрольной суммы и форматирования строки
 */
static std::string format_hex_record(uint8_t length, uint16_t addr, uint8_t type,
                                     const uint8_t *data) {
    uint8_t     checksum = length + (addr >> 8) + (addr & 0xFF) + type;
    std::string hex_data;

    for (int i = 0; i < length; ++i) {
        checksum += data[i];
        hex_data += fmt::format("{:02X}", data[i]);
    }

    checksum = static_cast<uint8_t>((~checksum) + 1);
    return fmt::format(":{:02X}{:04X}{:02X}{}{:02X}\n", length, addr, type, hex_data, checksum);
}

/*!
 * Save all region
 */
bool MemoryRegion::export_intel_hex_file(const std::string &path, bool append) {
    return export_intel_hex_file(path, 0, size() - 1, base(), append);
}

/*!
 * Save region between start and end offsets
 */
bool MemoryRegion::export_intel_hex_file(const std::string &path, size_t start_offset,
                                         size_t end_offset, size_t newbase, bool append) const {

    if (newbase == 0)
        newbase = base();

    // Проверка границ
    if (start_offset >= m_data.size()) {
        throw std::runtime_error("MemoryRegion::save_to_hex_file: start offset out of bounds");
    }

    size_t actual_end =
        (end_offset == 0 || end_offset >= m_data.size()) ? m_data.size() - 1 : end_offset;

    if (start_offset > actual_end) {
        throw std::runtime_error("MemoryRegion::save_to_hex_file: invalid offset range");
    }

    std::string full_content;

    // Генерируем HEX записи
    for (size_t i = start_offset; i <= actual_end; i += 16) {
        // Размер текущего чанка (не более 16 байт)
        uint8_t chunk = static_cast<uint8_t>(std::min((size_t)16, actual_end - i + 1));

        // Адрес в Intel HEX: origin + смещение
        uint16_t hex_addr = static_cast<uint16_t>(newbase + i);

        // Форматируем запись
        full_content += format_hex_record(chunk, hex_addr, 0x00, &m_data[i]);
    }

    // Добавляем маркер конца файла (только если не append или это первый чанк)
    if (!append || start_offset == 0) {
        full_content += ":00000001FF\n";
    }

    // Определяем полный путь
    auto        project_path = file_util::get_path(file_util::PathType::PROJECT);
    std::string full_path = path;
    if (path[0] != '/' && path[0] != '\\') {
        full_path = project_path.string() + "/" + path;
    }

    // Режим записи
    std::ios_base::openmode mode = std::ios_base::out;
    if (append) {
        mode |= std::ios_base::app;
    } else {
        mode |= std::ios_base::trunc;
    }

    // Записываем файл
    std::ofstream file(full_path, mode);
    if (!file.is_open()) {
        throw std::runtime_error("MemoryRegion::save_to_hex_file: cannot open file: " + full_path);
    }

    file << full_content;
    file.close();

    return true;
}
} // namespace script
