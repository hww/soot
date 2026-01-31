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
        std::shared_ptr<TypeSystem> m_ts;
        Type* m_type;
        std::string m_path; 
        // 1. Конструктор по умолчанию (теперь make_shared<TypeCell>() заработает)
        TypeCell() : MemoryCell(nullptr), m_ts(nullptr), m_type(nullptr) {}

        // 2. Конструктор от сырого указателя
        TypeCell(void* ptr) : MemoryCell(ptr), m_ts(nullptr), m_type(nullptr) {}

        // 3. Полный конструктор
        TypeCell(TypeSystem* ts, void* ptr, Type* type, std::string path = "") 
            : MemoryCell(ptr, MemoryAccessKind::CUSTOM)
            , m_ts(ts ? std::shared_ptr<TypeSystem>(ts, [](TypeSystem*){}) : nullptr)
            , m_type(type)
            , m_path(std::move(path))
        {
        }

        Object get() override;

        void set(const Object& val) override;

        Object make_step_accessor(const Object& key) override;

        std::string print() const override;
        Object inspect() const override;
    };
    
}