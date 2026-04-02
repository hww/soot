// common/sootc/IR/StaticObject.hpp
#pragma once

#include <string>
#include <vector>
#include <cstring>
#include "common/CommonTypes.hpp"

namespace sootc {

class FunctionEnv;
class TypeEnv;
class MethodEnv;
class StateEnv;

// ============================================================================
// Базовый класс для статических объектов
// ============================================================================

class StaticObject {
public:
    virtual ~StaticObject() = default;
    virtual std::string print() const = 0;

    struct LoadInfo {
        bool requires_load = false;
        int load_size = -1;
        bool load_signed = false;
        bool prefer_xmm = false;
    };

    virtual LoadInfo get_load_info() const = 0;
    virtual void generate(std::vector<u8>& data, std::vector<u64>& relocations) = 0;
    virtual u64 get_addr_offset() const = 0;
    
    u64 file_offset = 0;
    std::string name;
};

// ============================================================================
// StaticFloat — статическое число с плавающей точкой
// ============================================================================

class StaticFloat : public StaticObject {
public:
    explicit StaticFloat(float value);
    
    std::string print() const override;
    LoadInfo get_load_info() const override;
    void generate(std::vector<u8>& data, std::vector<u64>& relocations) override;
    u64 get_addr_offset() const override;
    
    float value = 0;
};

// ============================================================================
// StaticString — статическая строка
// ============================================================================

class StaticString : public StaticObject {
public:
    explicit StaticString(const std::string& text);
    
    std::string print() const override;
    LoadInfo get_load_info() const override;
    void generate(std::vector<u8>& data, std::vector<u64>& relocations) override;
    u64 get_addr_offset() const override;
    
    std::string text;
};

// ============================================================================
// StaticStructure — базовая структура для сложных статических данных
// ============================================================================

class StaticStructure : public StaticObject {
public:
    StaticStructure();
    
    std::string print() const override;
    LoadInfo get_load_info() const override;
    void generate(std::vector<u8>& data, std::vector<u64>& relocations) override;
    u64 get_addr_offset() const override;
    
    struct SymbolRecord {
        u64 offset;
        std::string name;
    };
    
    struct PointerRecord {
        u64 offset_in_this;
        StaticStructure* dest;
        u64 offset_in_dest;
    };
    
    struct FunctionRecord {
        u64 offset_in_this;
        FunctionEnv* func;
    };
    
    struct MethodRecord {
        u64 offset_in_this;
        MethodEnv* method;
    };
    
    struct StateRecord {
        u64 offset_in_this;
        StateEnv* state;
    };
    
    struct TypeRecord {
        u64 offset_in_this;
        TypeEnv* type;
    };
    
    std::vector<u8> data;
    std::vector<SymbolRecord> symbols;
    std::vector<PointerRecord> pointers;
    std::vector<FunctionRecord> functions;
    std::vector<MethodRecord> methods;
    std::vector<StateRecord> states;
    std::vector<TypeRecord> types;
    
    void add_symbol(const std::string& name, u64 offset);
    void add_pointer(u64 offset_in_this, StaticStructure* dest, u64 offset_in_dest);
    void add_function(FunctionEnv* func, u64 offset);
    void add_method(MethodEnv* method, u64 offset);
    void add_state(StateEnv* state, u64 offset);
    void add_type(TypeEnv* type, u64 offset);
    
protected:
    u64 m_offset = 0;
};

// ============================================================================
// StaticBasic — базовый объект GOAL (наследует StaticStructure)
// ============================================================================

class StaticBasic : public StaticStructure {
public:
    explicit StaticBasic(const std::string& type_name);
    
    u64 get_addr_offset() const override;
    std::string type_name;
};

// ============================================================================
// StaticPair — пара (cons cell)
// ============================================================================

class StaticPair : public StaticStructure {
public:
    StaticPair();
    
    u64 get_addr_offset() const override;
    
    void set_car(StaticObject* car, u64 offset);
    void set_cdr(StaticObject* cdr, u64 offset);
    
private:
    StaticObject* m_car = nullptr;
    StaticObject* m_cdr = nullptr;
    u64 m_car_offset = 0;
    u64 m_cdr_offset = 0;
};

} // namespace sootc