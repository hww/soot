#include "TypeCell.hpp"
#include "StaticBufferWriter.hpp"
#include "StaticBufferReader.hpp"

namespace script {

Object TypeCell::get() {
    if (!m_ptr || !m_type) return Object::make_undefined();

    // 1. Примитивы, Enum, Bitfield
    if (!m_type->is_reference()) {
        return StaticBufferReader::read_value_at_ptr(&TypeSystem::instance(), m_ptr, m_type);
    }

    // 2. Строки и Символы
    if (m_type->get_name() == "string" || m_type->get_name() == "symbol") {
         return StaticBufferReader::read_string_at_ptr(m_ptr);
    }

    // 3. Структуры — возвращаем саму ячейку (для дальнейшей навигации через ->)
    return Object::make_heap_object(shared_from_this(), ObjectType::CELL);
}

void TypeCell::set(const Object& val) {
    if (!m_ptr || !m_type) return;

    try {
        // Выполняем физическую запись в буфер (учитывая тип: LE/BE, string и т.д.)
        StaticBufferWriter::write_value_at_ptr(m_ptr, m_type, val);
        /**
        std::string valhex;
        if (val.is_integer())
            valhex = fmt::format("0x{:04X}", val.as_integer());
        // ВЫВОД В ЛОГ: Красивая таблица для анализа
        // Показываем: ПУТЬ | АДРЕС | ТИП | ЗНАЧЕНИЕ
        fmt::print(fmt::runtime("[StaticWrite] {:<25} | @ 0x{:04X} | {:<10} | <- {} {}\n"), 
                   m_path.empty() ? m_type->get_name() : m_path, 
                   reinterpret_cast<uintptr_t>(m_ptr), // Можно вычесть base буфера, если нужно
                   m_type->get_name(), 
                   val.print(),
                   valhex);
        */        
    } 
    catch (const std::exception& e) {
        fmt::print(stderr, "[Error] Write failed for {}: {}\n", m_path, e.what());
    }
}

Object TypeCell::make_step_accessor(const Object& key) {
    if (!m_ptr || !m_type) return Object::make_undefined();

    // --- ЛОГИКА МАССИВОВ (Индексация по числу) ---
    if (key.is_integer()) {
        int index = key.as_integer();
        
        // Здесь мы используем heap_base типа (как в GOAL) или его размер
        // чтобы понять, на сколько байт прыгнуть
        size_t stride = m_type->get_size_in_memory(); 
        
        // Если мы в inline-array, выравнивание элементов критично
        int alignment = m_type->get_inline_array_stride_alignment();
        if (alignment > 1) {
            stride = (stride + alignment - 1) & ~(alignment - 1);
        }

        uint8_t* next_ptr = static_cast<uint8_t*>(m_ptr) + (index * stride);
        
        // Мы НЕ меняем тип, так как мы просто перешли к i-му элементу ТОГО ЖЕ типа
        return Object::make_heap_object(
            std::make_shared<TypeCell>(next_ptr, m_type, fmt::format("{}[{}]", m_path, index)),
            ObjectType::CELL
        );
    }

    // --- ЛОГИКА СТРУКТУР (Доступ по символу/ключу) ---
    auto* struct_type = dynamic_cast<StructureType*>(m_type);
    
    // Если это не структура, но мы пытаемся взять поле, 
    // возможно это массив, и мы хотим взять поле у его элементов? 
    // Нет, по правилам Лиспа (-> obj index 'field) — индекс должен быть первым.
    
    if (struct_type) {
        Field field;
        std::string field_name = key.to_std_string();
        
        if (struct_type->lookup_field(field_name, &field)) {
            Type* next_type_raw = TypeSystem::instance().lookup_type(field.type());
            if (!next_type_raw) {
                throw std::runtime_error("Unknown field type: " + field.type().print());
            }

            uint8_t* next_ptr = static_cast<uint8_t*>(m_ptr) + field.offset();
            std::string next_path = m_path.empty() ? field_name : m_path + "." + field_name;

            auto next_cell = std::make_shared<TypeCell>(next_ptr, next_type_raw, next_path);
            return Object::make_heap_object(next_cell, ObjectType::CELL);
        }
    }

    return Object::make_undefined();
}

std::string TypeCell::print() const {
    auto value = const_cast<TypeCell*>(this)->get().print();
    return fmt::format("#<type-cell {} @ {:p} :value {}>", 
                      m_path.empty() ? m_type->get_name() : m_path, m_ptr, value.c_str());
}

Object TypeCell::inspect() const {
    ListBuilder lb{};
    lb.push_back(Object::symbol_table().core.cell); // Символ 'cell
    
    // Вместо "base" и "key" мы показываем физику:
    lb.push_kv("address", Object::make_integer((uintptr_t)m_ptr)); 
    
    // Показываем текущее значение, раз мы "арестовали" этот участок памяти
    try {
        lb.push_kv("value", const_cast<TypeCell*>(this)->get());
    } catch (...) {
        lb.push_kv("value", Object::symbol_table().core.unknown);
    }
    return lb.finalize();
}
} // namespace script