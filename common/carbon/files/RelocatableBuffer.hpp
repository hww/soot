// common/carbon/files/RelocatableBuffer.hpp
#pragma once

#include <cstddef>
#include <vector>
#include <cstring>
#include "common/CommonTypes.hpp"
#include "common/carbon/files/Definition.hpp"
#include "vm/Instructions.hpp"
#include "fmt/format.h"

using namespace carbon::vm;

namespace carbon::files {
    
    struct Relocation {
        enum class Type { 
            FIXED_ADDRESS,  // относительное смещение от начала файла (для глобальных символов)
            LABEL_ADDRESS,  // адрес метки по имени (для символов)
            BRANCH_DISP16   // для инструкций перехода (рассчитывается как смещение от текущей позиции до метки)
        };

        Type type;          // тип релокации
        std::string name;   // имя целевого объекта
        u64 offset;         // позиция в буфере, куда нужно записать адрес
    };

    struct RelLabel {
        std::string name;   // имя целевого объекта
        u64 address;        // позиция в буфере
    };

class RelocatableBuffer {
public:


    // ==============================================================================
    // Дополнительные методы для удобства
    // ==============================================================================

    // Просто записать значение (без релокации)
    void add_u64(u64 value) {
        add_bytes(&value, sizeof(u64));
    }
    
    void add_u32(u32 value) {
        add_bytes(&value, sizeof(u32));
    }
    
    void add_u8(u8 value) {
        add_bytes(&value, sizeof(u8));
    }
    
    // Добавить сырые байты
    void add_bytes(const void* data, size_t size) {
        const u8* ptr = reinterpret_cast<const u8*>(data);
        bytes_.insert(bytes_.end(), ptr, ptr + size);
    }

    // ==============================================================================
    // Добавить инструкцию
    // ==============================================================================

    void add_instruction(Opcode op, u8 a = 0, u8 b = 0, u8 c = 0) {
        auto inst = Instruction::create_abc(op, a, b, c);
        add_bytes(&inst, sizeof(Instruction));
    }

    void add_instruction(Opcode op, u8 a = 0, u8 b = 0) {
        auto inst = Instruction::create_ab(op, a, b);
        add_bytes(&inst, sizeof(Instruction));
    }

    void add_instruction(Opcode op, u8 a = 0) {
        auto inst = Instruction::create_a(op, a);
        add_bytes(&inst, sizeof(Instruction));
    }

    void add_instruction_imm_s16(Opcode op, u8 a = 0, s16 imm = 0) {
        auto inst = Instruction::create_imm(op, a, imm);
        add_bytes(&inst, sizeof(Instruction));
    }

    void add_instruction_imm_u16(Opcode op, u8 a = 0, u16 imm = 0) {
        auto inst = Instruction::create_k(op, a, imm);
        add_bytes(&inst, sizeof(Instruction));
    }

    // ==============================================================================
    // Добавить метку для последующей релокации
    // ==============================================================================

    void add_label(const std::string& name = "") {
        labels_.push_back({name, static_cast<u64>(bytes_.size())});
    }

    // ==============================================================================
    // Добавить релоцируемый указатель
    // ==============================================================================
      
    // Добавить значение с возможностью релокации
    void add_relocatable(u64 offset, Relocation::Type type, const std::string& name = "") {
        switch (type) {
            case Relocation::Type::FIXED_ADDRESS:
                relocatations_.push_back({type, name, offset});
                break;
            case Relocation::Type::LABEL_ADDRESS:
                if (name.empty()) {
                    throw std::runtime_error("Name must be provided for BYNAME relocation");
                }
                relocatations_.push_back({type, name, offset});
                break;
            case Relocation::Type::BRANCH_DISP16:
                relocatations_.push_back({type, name, offset});
                break;
        }
    }

    /** Сохранить текузий адрес как метсо branch инструкции которую нужно релокировать */
    void add_branch_reference(const std::string& label_name) {
        add_relocatable(bytes_.size(), Relocation::Type::BRANCH_DISP16, label_name);
    } 

    // Добавить указатель (смещение)
    void add_relocatable(u32 offset, Relocation::Type type, const std::string& name = "") {
        // В любом случае добавляем 64 битный релоцируемый указатель
        add_relocatable((u64)offset, type, name);
    }

    void add_relocatable(Relocation::Type type, const std::string& name = "") {
        add_relocatable(bytes_.size(), type, name);
    }

    void add_relocatable_pointer(u64 ptr, Relocation::Type type, const std::string& name = "") {
        add_relocatable(bytes_.size(), type, name);
        add_u64(ptr);
    }

    // ==============================================================================
    // Добавить функцию
    // ==============================================================================

    /** Добавить функцию */
    void add_function(
            const std::string& name,
            const std::vector<vm::Instruction>& code,
            const std::vector<u8>& data = {},
            const std::vector<SourceLocation>& debug_info = {},
            SymbolFlags flags = SymbolFlags::Export);

    // ==============================================================================
    // Добавить другой буфер (для вложенных структур)
    // ==============================================================================

    // Добавить подбуфер (для вложенных структур)
    void add_buffer(const RelocatableBuffer& other, u64 base_offset = 0, bool throw_error = false) {

        (void)base_offset;

        u32 insert_pos = bytes_.size();
        bytes_.insert(bytes_.end(), other.bytes_.begin(), other.bytes_.end());
        
        // Копируем relocatable offsets из подбуфера
        for (const auto& item : other.relocatations_) {
            relocatations_.push_back({ item.type, item.name, insert_pos + item.offset });            
        }

        // Копируем метки offsets из подбуфера
        for (const auto& label : other.labels_) {
            if (get_label(label.name)) {
                throw std::runtime_error("Duplicate label name: " + label.name);
            }
            labels_.push_back({ label.name, insert_pos + label.address });
        }
    }

    // ==============================================================================
    // Получить информацию о релокации по имени или индексу
    // ==============================================================================

    const Relocation* get_relocation(std::string name) const {
        for (const auto& relocation : relocatations_) {
            if (relocation.name == name) {
                return &relocation;
            }
        }
        return nullptr;
    }

    const Relocation* get_relocation(size_t index) const {
        if (index >= relocatations_.size()) {
            throw std::out_of_range("Relocation index out of range");
        }
        return &relocatations_[index];
    }
    
    // ==============================================================================
    // Получить информацию о метке по имени или индексу
    // ==============================================================================

    const RelLabel* get_label(std::string name) const {
        for (const auto& label : labels_) {
            if (label.name == name) {
                return &label;
            }
        }
        return nullptr;
    }

    const RelLabel* get_label(size_t index) const {
        if (index >= labels_.size()) {
            throw std::out_of_range("Label index out of range");
        }
        return &labels_[index];
    }

    // ==============================================================================
    // Обновить все релокации с данным именем (например, после разрешения символов)
    // ==============================================================================

    void link() {
        for (auto& reloc : relocatations_) {
            // 1. Пытаемся найти данные по этому офсету в буфере
            u64* target_ptr = reinterpret_cast<u64*>(bytes_.data() + reloc.offset);
            
            switch (reloc.type) {
                case Relocation::Type::FIXED_ADDRESS: {
                    // АДДИТИВНАЯ ЗАПИСЬ: Мы ничего не ищем по имени, 
                    // мы просто считаем, что в *target_ptr уже лежит офсет,
                    // и его нужно оставить как есть для загрузчика.
                    // (Или прибавить базу, если link() — это финальная стадия).
                    break; 
                }
                case Relocation::Type::LABEL_ADDRESS: {
                    // ПРЯМАЯ ЗАПИСЬ: Находим метку и пишем её адрес "с нуля"
                    const RelLabel* label = get_label(reloc.name);
                    if (!label) throw std::runtime_error("Undefined label: " + reloc.name);
                    *target_ptr = label->address; 
                    break;
                }
                case Relocation::Type::BRANCH_DISP16: {
                    // ПРЯМАЯ ЗАПИСЬ: Находим метку и пишем смещение от текущей позиции до метки
                    const RelLabel* label = get_label(reloc.name);
                    if (!label) throw std::runtime_error("Undefined label: " + reloc.name);
                    u64 current_pos = reloc.offset;                   
                    u64 target_pos = label->address;
                    s64 offset = static_cast<s64>(target_pos) - static_cast<s64>(current_pos);
                    
                    Instruction* instr = reinterpret_cast<Instruction*>(bytes_.data() + reloc.offset);
                    instr->imm16 = static_cast<u16>(offset);
                    break;
                }
            }
        }
    }

    // ==============================================================================
    // Получить все данные или все релокации
    // ==============================================================================

    const std::vector<u8>& bytes() const { return bytes_; }
    const std::vector<Relocation>& relocatable_offsets() const { return relocatations_; }

    u8* data() { return bytes_.data(); }
    const u8* data() const { return bytes_.data(); }

    // ==============================================================================
    // Получить размер и проверить, пустой ли буфер
    // ==============================================================================

    size_t size() const { return bytes_.size(); }
    bool is_empty() const { return size()==0; }

    // ==============================================================================
    // Отладочная информация
    // ==============================================================================

    std::string inspect() const {
        const int MAX_LEN = 8;
        std::string result = "RelocatableBuffer\n";
        result += fmt::format("  Bytes[{}] [", size());
        for (size_t j = 0; j < std::min(size(), size_t(MAX_LEN)); j++) {
            result += fmt::format("{:02x}", bytes_[j]);
        }
        if (size() > MAX_LEN) {
            result += "...";
        }
        result += "]";
        result += fmt::format("  Relocations[{}] [", relocatations_.size());
        for (size_t j = 0; j < std::min(relocatations_.size(), size_t(MAX_LEN)); j++) {
            switch (relocatations_[j].type) {
                case Relocation::Type::FIXED_ADDRESS:
                    result += fmt::format("{:016x} FIXED_ADDRESS {}", relocatations_[j].offset, relocatations_[j].name);
                    break;
                case Relocation::Type::LABEL_ADDRESS:
                    result += fmt::format("{:016x} LABEL_ADDRESS {}", relocatations_[j].offset, relocatations_[j].name);
                    break;
                case Relocation::Type::BRANCH_DISP16:
                    result += fmt::format("{:016x} BRANCH_DISP16 {}", relocatations_[j].offset, relocatations_[j].name);
                    break;                    
            }
        }
        result += "]";
        result += fmt::format("  Labels[{}] [", labels_.size());
        for (size_t j = 0; j < std::min(labels_.size(), size_t(MAX_LEN)); j++) {
            result += fmt::format("{:016x} LABEL          {}", labels_[j].address, labels_[j].name);
        }
        result += "]";

        return result;
    }


private:
    std::vector<u8> bytes_;
    std::vector<Relocation> relocatations_;
    std::vector<RelLabel> labels_;
    u64 dummy_ = 0;
};

} // namespace carbon::files