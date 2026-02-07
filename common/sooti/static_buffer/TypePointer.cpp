#include "TypePointer.hpp"
#include "StaticBufferReader.hpp"
#include "StaticBufferWriter.hpp"
#include "common/type_system/TypeSystem.hpp"
#include <fmt/format.h>

namespace script {

// ===========================================================================
// TypePointer
// ===========================================================================

TypePointer::TypePointer(void *ptr, Type *type, std::shared_ptr<HeapObject> owner)
    : Pointer(ptr, type->get_name()), m_owner(owner) {}
} // namespace script

Type *TypePointer::get_type() {
    return TypeSystem::instance().lookup_type(m_type);
}

Object TypePointer::get() {
    void *ptr = resolve_addr();
    if (!ptr || m_type.empty())
        return Object::make_none();

    auto type = get_type();
    if (!type) {
        throw std::runtime_error(fmt::format("TypePointer::get: type {} not found", m_type));
    }

    // 1. Если это примитив (ValueType, Enum, Bitfield)
    // Мы делегируем чтение ридеру, который вернет число, символ или список флагов.
    if (type->get_class_name() == "value" || type->get_class_name() == "enum" ||
        type->get_class_name() == "bitfield") {
        return StaticBufferReader::read_value_at_ptr(&TypeSystem::instance(), ptr, type);
    }

    // 2. Строки и Символы - это тоже "значения", которые мы хотим получить сразу
    if (type->get_name() == "string" || type->get_name() == "symbol") {
        return StaticBufferReader::read_value_at_ptr(&TypeSystem::instance(), ptr, type);
    }

    // 3. СТРУКТУРЫ (Самый важный момент)
    // Если мы просто вызвали (get ptr), где ptr указывает на структуру,
    // у нас есть два пути:

    // ПУТЬ А (Отладочный): Вернуть alist через read_structure_to_alist.
    // return StaticBufferReader::read_structure_to_alist(&TypeSystem::instance(), ptr,
    // static_cast<StructureType*>(type));

    // ПУТЬ Б (Системный): Вернуть сам указатель.
    // Это позволяет писать (-> ptr-to-struct field), потому что интерпретатор
    // получит TypePointer и вызовет у него step.
    return Object::make_heap_object(shared_from_this(), ObjectType::POINTER);
}

void TypePointer::set(const Object &val) {
    void *ptr = resolve_addr();
    if (!ptr || m_type.empty())
        return;

    auto type = get_type();
    if (!type)
        throw std::runtime_error(fmt::format("TypePointer::get: type {} not found", m_type));

    if (auto *buffer = dynamic_cast<StaticBuffer *>(m_owner.get())) {
        // Буфер сам внутри вызовет StaticBufferWriter::write_value_at_ptr
        // и сам же вычислит offset (ptr - data()) для update_addr_range
        buffer->write_value_at_ptr(ptr, type, val);
    } else {
        // Если владелец — не буфер (например, обычный HeapObject или статика),
        // используем базовую логику записи в память без уведомлений
        StaticBufferWriter::write_value_at_ptr(ptr, type, val);
    }
}
Object TypePointer::make_step_accessor(const Object &key) {
    // 1. Быстрая проверка валидности
    if (!m_ptr || m_type.empty()) {
        return Object::make_none();
    }

    // 2. Получаем актуальный объект типа через TypeSystem
    Type *current_type = TypeSystem::instance().lookup_type(m_type);
    if (!current_type) {
        throw std::runtime_error("TypePointer: unknown type " + m_type);
    }

    Type  *next_type_obj = nullptr;
    size_t offset_delta = 0;

    // --- ЛОГИКА СМЕЩЕНИЯ ---
    if (key.is_integer()) {
        // Случай A: Доступ по индексу (Массив / Inline Array)
        int    index = key.as_integer();
        size_t stride = current_type->get_size_in_memory();
        int    align = current_type->get_inline_array_stride_alignment();

        // Учитываем выравнивание элементов в массиве (как в OpenGOAL)
        if (align > 1) {
            stride = (stride + align - 1) & ~(align - 1);
        }

        offset_delta = index * stride;
        next_type_obj = current_type; // Тип элемента тот же

    } else if (key.is_symbol() || key.is_string()) {
        // Случай Б: Доступ к полю структуры
        auto *struct_type = dynamic_cast<StructureType *>(current_type);
        if (!struct_type) {
            throw std::runtime_error(
                fmt::format("TypePointer: type {} is not a structure", m_type));
        }

        Field       field_info;
        std::string field_name = key.to_std_string();
        if (struct_type->lookup_field(field_name, &field_info)) {
            offset_delta = field_info.offset();
            // Получаем тип поля (lookup здесь не нужен, нам нужна только строка имени типа)
            next_type_obj = TypeSystem::instance().lookup_type(field_info.type());
        }
    }

    // Если не удалось вычислить смещение (нет такого поля или неверный ключ)
    if (!next_type_obj) {
        return Object::make_none();
    }

    // --- ГЕНЕРАЦИЯ НОВОГО УКАЗАТЕЛЯ ---
    // Вычисляем физический адрес в памяти хоста
    uint8_t *new_phys_ptr = static_cast<uint8_t *>(m_ptr) + offset_delta;

    // Создаем новый TypePointer.
    // m_owner прокидывается дальше, обеспечивая безопасность ссылки.
    auto next_cell =
        std::make_shared<TypePointer>(new_phys_ptr,              // Новая физика
                                      next_type_obj->get_name(), // Имя нового типа (строка)
                                      m_owner                    // Тот же владелец
        );

    return Object::make_heap_object(next_cell, ObjectType::POINTER);
}

void *TypePointer::resolve_addr() const {
    // В данной реализации m_ptr уже является вычисленным адресом.
    // Однако, проверка на существование владельца важна для безопасности GC.
    return m_owner ? m_ptr : nullptr;
}

std::string TypePointer::print() const {
    return fmt::format("#<type-pointer {} @ {:p}>", m_type, m_ptr);
}

Object TypePointer::inspect() const {
    ListBuilder lb{};
    lb.push_back(Object::make_symbol("type-pointer"));
    lb.push_kv("address", Object::make_integer((uintptr_t)m_ptr));
    lb.push_kv("type", Object::make_string(m_type));
    return lb.build();
}
