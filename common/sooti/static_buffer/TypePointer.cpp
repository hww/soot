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
    if (!ptr || m_type.empty()) {
        throw std::runtime_error(fmt::format("CRITICAL: type is null at ptr {:p}\n", ptr));
        return;
    }

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
    if (!m_ptr || m_type.empty())
        return Object::make_none();

    Type *current_type = TypeSystem::instance().lookup_type(m_type);
    if (!current_type)
        throw std::runtime_error("TypePointer: unknown type " + m_type);

    // --- СЛУЧАЙ А: МЫ В СТРУКТУРЕ ---
    if (auto *struct_type = dynamic_cast<StructureType *>(current_type)) {
        Field field_info;
        if (key.is_symbol() || key.is_string()) {
            if (struct_type->lookup_field(key.to_std_string(), &field_info)) {

                // Если это ОБЫЧНОЕ поле (не массив), сразу возвращаем указатель на него
                if (!field_info.is_array()) {
                    uint8_t *field_addr = static_cast<uint8_t *>(m_ptr) + field_info.offset();
                    auto     ptr = std::make_shared<TypePointer>(
                        field_addr, field_info.type().base_type(), m_owner);
                    return Object::make_pointer(ptr);
                }

                // Если это МАССИВ, нам нужно вернуть "умный" объект,
                // который знает, что он массив, чтобы обработать следующий индекс.
                // В твоей системе проще всего вернуть указатель на начало массива,
                // но пометить его особым типом-оберткой или обработать индекс прямо здесь.

                // Для простоты: возвращаем указатель на начало, но TypePointer
                // должен будет уметь работать с индексами.
                uint8_t *array_start = static_cast<uint8_t *>(m_ptr) + field_info.offset();
                auto ptr = std::make_shared<TypePointer>(array_start, field_info.type().base_type(),
                                                         m_owner);
                return Object::make_pointer(ptr);
            }
        }
    }

    // --- СЛУЧАЙ Б: ДОСТУП ПО ИНДЕКСУ ---
    if (key.is_integer()) {
        int index = key.as_integer();
        // Здесь current_type — это тип ЭЛЕМЕНТА (например, uint8)
        // Мы предполагаем, что если к TypePointer обратились по индексу,
        // то это индекс этого типа в памяти.

        size_t stride = current_type->get_size_in_memory();
        int    align = current_type->get_inline_array_stride_alignment();
        if (align > 1) {
            stride = (stride + align - 1) & ~(align - 1);
        }

        uint8_t *elem_addr = static_cast<uint8_t *>(m_ptr) + (index * stride);

        auto ptr = std::make_shared<TypePointer>(elem_addr, m_type, m_owner);
        return Object::make_pointer(ptr);
    }

    return Object::make_none();
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
