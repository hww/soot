// common/carbon/files/RelocatableBuffer.hpp
#pragma once

#include <cstddef>
#include <vector>
#include <cstring>
#include "common/CommonTypes.hpp"
#include "files/Definition.hpp"
#include "fmt/format.h"

namespace carbon::files {
    
    struct Relocation {
        enum class Type { 
            FILE_RELATIVE,  // относительное смещение
            LABEL_REF,      // по имени (для символов)
            LABEL_COPY      // скопированием указателя метки 
        };
        
        Type type;          // тип релокации
        std::string name;   // имя целевого объекта
        u64 offset;         // позиция в буфере, куда нужно записать адрес
    };

    struct Label {
        std::string name;   // имя целевого объекта
        u64 offset;         // позиция в буфере, куда нужно записать адрес
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
            case Relocation::Type::FILE_RELATIVE:
                relocatations_.push_back({type, name, offset});
                break;
            case Relocation::Type::LABEL_REF:
                if (name.empty()) {
                    throw std::runtime_error("Name must be provided for BYNAME relocation");
                }
                relocatations_.push_back({type, name, offset});
                break;
            case Relocation::Type::LABEL_COPY:
                // COPY — это особый случай, который может потребовать другой обработки
                // В данном контексте мы просто добавляем его в список релокаций,
                // но логика обработки может отличаться при линковке.
                relocatations_.push_back({type, name, offset});
                break;
        }
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
            labels_.push_back({ label.name, insert_pos + label.offset });
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

    const Label* get_label(std::string name) const {
        for (const auto& label : labels_) {
            if (label.name == name) {
                return &label;
            }
        }
        return nullptr;
    }

    const Label* get_label(size_t index) const {
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
                case Relocation::Type::LABEL_REF: {
                    // ПРЯМАЯ ЗАПИСЬ: Находим метку и пишем её адрес "с нуля"
                    const Label* label = get_label(reloc.name);
                    if (!label) throw std::runtime_error("Undefined label: " + reloc.name);
                    *target_ptr = label->offset; 
                    break;
                }
                case Relocation::Type::LABEL_COPY: {

                    const Label* label = get_label(reloc.name);
                    if (!label) throw std::runtime_error("Undefined label: " + reloc.name); 
                    for (size_t i = 0; i < sizeof(u64); i++) {
                        *target_ptr =  bytes_[reloc.offset + i];
                    }
                    break; 
                }
                case Relocation::Type::FILE_RELATIVE: {
                    // АДДИТИВНАЯ ЗАПИСЬ: Мы ничего не ищем по имени, 
                    // мы просто считаем, что в *target_ptr уже лежит офсет,
                    // и его нужно оставить как есть для загрузчика.
                    // (Или прибавить базу, если link() — это финальная стадия).
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
                case Relocation::Type::FILE_RELATIVE:
                    result += fmt::format("{:016x} FILE_RELATIVE {}", relocatations_[j].offset, relocatations_[j].name);
                    break;
                case Relocation::Type::LABEL_REF:
                    result += fmt::format("{:016x} REF_TO_NAME   {}", relocatations_[j].offset, relocatations_[j].name);
                    break;
                case Relocation::Type::LABEL_COPY:
                    result += fmt::format("{:016x} COPY_FROM_NAME {}", relocatations_[j].offset, relocatations_[j].name);
                    break;                    
            }
        }
        result += "]";
        result += fmt::format("  Labels[{}] [", labels_.size());
        for (size_t j = 0; j < std::min(labels_.size(), size_t(MAX_LEN)); j++) {
            result += fmt::format("{:016x} LABEL          {}", labels_[j].offset, labels_[j].name);
        }
        result += "]";

        return result;
    }

private:
    std::vector<u8> bytes_;
    std::vector<Relocation> relocatations_;
    std::vector<Label> labels_;
    u64 dummy_ = 0;
};

} // namespace carbon::files