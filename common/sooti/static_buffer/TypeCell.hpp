#pragma once

#include <cstring>
#include <stdexcept>
#include <unordered_map>
#include <fmt/format.h>

#include "common/type_system/TypeSystem.hpp"
#include "StaticBuffer.hpp"
#include "common/sooti/Object.hpp"
#include "common/sooti/ListBuilder.hpp"

namespace script {

    class TypeCell : public MemoryCell {
    public:
        Type* m_type;
        std::string m_path; 

        // 1. Конструктор по умолчанию (теперь make_shared<TypeCell>() заработает)
        TypeCell() : MemoryCell(nullptr), m_type(nullptr) {}

        // 2. Конструктор от сырого указателя
        TypeCell(void* ptr) : MemoryCell(ptr), m_type(nullptr) {}

        // 3. Полный конструктор
        TypeCell(void* ptr, Type* type, std::string path = "") 
            : MemoryCell(ptr, MemoryAccessKind::CUSTOM)
            , m_type(type)
            , m_path(std::move(path))
        {
        }
    
        Type* get_type() const { return m_type; }

        Object get() override;

        void set(const Object& val) override;

        Object make_step_accessor(const Object& key) override;

        std::string print() const override;
        Object inspect() const override;
    };
    
    class BufferCell : public MemoryCell {
        std::shared_ptr<StaticBuffer> m_buffer;
        size_t m_offset;

    public:
        BufferCell(std::shared_ptr<StaticBuffer> buf, size_t off) 
            : MemoryCell(nullptr) // Базовый класс инициализируем nullptr, так как работаем через m_buffer
            , m_buffer(buf)
            , m_offset(off) 
        {}

        // 1. Чтение/запись "по умолчанию" (как uint8)
        void set(const Object& value) override {
            // Используем write_u8, так как patch у тебя в StaticBuffer 
            // может быть завязан на логику линковщика
            m_buffer->write_u8(m_offset, static_cast<uint8_t>(value.as_integer())); 
        }
        
        Object get() override {
            // У тебя в StaticBuffer есть метод read_u8
            return Object::make_integer(m_buffer->read_u8(m_offset));
        }

        // 2. Превращение в TypeCell
        Object make_step_accessor(const Object& key) {
            if (key.is_symbol()) {
                auto type = TypeSystem::instance().lookup_type(key.as_symbol());
                if (type) {
                    uint8_t* ptr = m_buffer->data() + m_offset;
                    // Создаем TypeCell и упаковываем его через make_cell
                    auto t_cell = std::make_shared<TypeCell>(ptr, type);
                    return Object::make_cell(std::move(t_cell), MemoryAccessKind::CUSTOM);
                }
            }
            
            if (key.is_integer()) {
                // Создаем новый BufferCell (сдвинутый) и тоже через make_cell
                auto b_cell = std::make_shared<BufferCell>(m_buffer, m_offset + key.as_integer());
                return Object::make_cell(std::move(b_cell), MemoryAccessKind::CUSTOM);
            }

            return Object::make_undefined();
        }
        
        std::string print() const override {
            return fmt::format("#<buffer-cell offset:{:#x}>", m_offset);
        }
    };
}