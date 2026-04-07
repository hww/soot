// common/carbon/files/RelocatableBuffer.hpp
#pragma once

#include <cstddef>
#include <stdexcept>
#include <string>
#include <vector>
#include <cstring>
#include "common/CommonTypes.hpp"
#include "common/carbon/files/Definition.hpp"
#include "files/BinaryFile.hpp"
#include "fmt/base.h"
#include "vm/Instructions.hpp"
#include "fmt/format.h"

using namespace carbon::vm;

namespace carbon::files {
    
    struct Relocation {
        enum class Type { 
            NULL_ADDRESS,   // значение пустое
            FIXED_ADDRESS,  // относительное смещение от начала файла (для глобальных символов)
            LABEL_ADDRESS,  // адрес метки по имени (для символов)
            BRANCH_DISP16   // для инструкций перехода (рассчитывается как смещение от текущей позиции до метки)
        };

        Type type;          // тип релокации
        std::string name;   // имя целевого объекта
        u64 offset;         // позиция в буфере, куда нужно записать адрес

        std::string to_string() const { 
            switch (type) {
                case Type::NULL_ADDRESS:
                    return fmt::format("Relocation(type=NULL_ADDRESS  name={} offset=0x{:016X})", name, offset);
                case Type::FIXED_ADDRESS:
                    return fmt::format("Relocation(type=FIXED_ADDRESS name={} offset=0x{:016X})", name, offset);
                case Type::LABEL_ADDRESS:
                    return fmt::format("Relocation(type=LABEL_ADDRESS name={} offset=0x{:016X})", name, offset);
                case Type::BRANCH_DISP16:
                    return fmt::format("Relocation(type=BRANCH_DISP   name={} offset=0x{:016X})", name, offset);
            }
            throw std::runtime_error("unexpected");
        }
    };

    struct RelLabel {
        std::string name;   // имя целевого объекта
        u64 offset;         // позиция в буфере

        std::string to_string() const { return fmt::format("RelLabel(name={} offset=0x{:016X})", name, offset);}
    };

class RelocatableBuffer {
public:
    // Конструкторы
    RelocatableBuffer() : name_("unnamed"), type_("unknown") {};
    RelocatableBuffer(std::string name, std::string type, bool create_label = false) : name_(name), type_(type){
        if (create_label) add_label(name);
    }

    // Конструктор копирования
    RelocatableBuffer(const RelocatableBuffer& other)
        : bytes_(other.bytes_)
        , relocatations_(other.relocatations_)
        , labels_(other.labels_)
        , name_(other.name_)
        , type_(other.type_) {}
    
    // Конструктор перемещения
    RelocatableBuffer(RelocatableBuffer&& other) noexcept
        : bytes_(std::move(other.bytes_))
        , relocatations_(std::move(other.relocatations_))
        , labels_(std::move(other.labels_))
        , name_(other.name_)
        , type_(other.type_) {
    }
    
    // Оператор присваивания копированием
    RelocatableBuffer& operator=(const RelocatableBuffer& other) {
        if (this != &other) {
            bytes_ = other.bytes_;
            relocatations_ = other.relocatations_;
            labels_ = other.labels_;
            name_ = other.name_;
            type_ = other.type_;
        }
        return *this;
    }
    
    // Оператор присваивания перемещением
    RelocatableBuffer& operator=(RelocatableBuffer&& other) noexcept {
        if (this != &other) {
            bytes_ = std::move(other.bytes_);
            relocatations_ = std::move(other.relocatations_);
            labels_ = std::move(other.labels_);
            name_ = other.name_;
            type_ = other.type_;
        }
        return *this;
    }
    
    // Деструктор (по умолчанию)
    ~RelocatableBuffer() = default;

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
    // Заменить данные
    // ==============================================================================    

    void write_at(u64 position, const void* data, size_t size) {
        if (position + size > bytes_.size()) {
            throw std::out_of_range("write_at out of range");
        }
        std::memcpy(bytes_.data() + position, data, size);
    }

    // Чтение по позиции
    void read_at(u64 position, void* data, size_t size) const {
        if (position + size > bytes_.size()) {
            throw std::out_of_range("read_at out of range");
        }
        std::memcpy(data, bytes_.data() + position, size);
    }

    // Generic read для произвольного типа T
    template<typename T>
    T read_at(u64 position) const {
        T result;
        read_at(position, &result, sizeof(T));
        return result;
    }

    // Generic write для произвольного типа T
    template<typename T>
    void write_at(u64 position, const T& value) {
        write_at(position, &value, sizeof(T));
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
    void add_relocatable_at(u64 offset, Relocation::Type type, const std::string& name = "") {
        switch (type) {
            case Relocation::Type::NULL_ADDRESS:
                relocatations_.push_back({type, name, offset});
                break;
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

    void add_relocatable(u64 offset, Relocation::Type type, const std::string& name = "") {
        add_relocatable_at(bytes_.size() + offset, type, name);
    }

    /** Сохранить текузий адрес как метсо branch инструкции которую нужно релокировать */
    void add_branch_reference(const std::string& label_name) {
        add_relocatable(Relocation::Type::BRANCH_DISP16, label_name);
    } 

    void add_relocatable(Relocation::Type type, const std::string& name = "") {
        add_relocatable(type, name);
    }

    void add_relocatable_pointer(u64 ptr, Relocation::Type type, const std::string& name = "") {
        add_relocatable(type, name);
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
    void add_buffer(const RelocatableBuffer& other) {
        u32 insert_pos = bytes_.size();
        lg::info("add_buffer {}[{:08X}] {}", name(), other.bytes_.size(), other.name());
        

        bytes_.insert(bytes_.end(), other.bytes_.begin(), other.bytes_.end());
        
        // Копируем метки
        for (const auto& label : other.labels_) {
            auto duplicate = get_label(label.name);
            if (duplicate) {
                throw std::runtime_error(fmt::format("Label {}\n conflicting with existing {}", label.to_string(), duplicate->to_string()));
            }
            RelLabel new_label{label.name, insert_pos + label.offset};
            labels_.push_back(new_label);
            print_formatted_line("add_buffer", new_label.offset, new_label.name, "LABEL", "");
        }
        
        // Копируем релокации
        for (const auto& reloc : other.relocatations_) {
            // Для FIXED_ADDRESS обновляем значение в буфере
            Relocation relocation{reloc.type, reloc.name, insert_pos + reloc.offset};
            relocatations_.push_back(relocation);

            u64* target_ptr = reinterpret_cast<u64*>(bytes_.data() + relocation.offset);
            if (reloc.type == Relocation::Type::FIXED_ADDRESS) {
                *target_ptr += insert_pos;
            } else if (reloc.type  == Relocation::Type::NULL_ADDRESS) {
                *target_ptr = 0;
            }
            print_formatted_line("add_buffer", relocation.offset, *target_ptr, reloc.name, reloc_type_to_string(reloc.type), "");
            // Для всех типов копируем релокацию
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
      
            
            switch (reloc.type) {
                case Relocation::Type::NULL_ADDRESS: {
                    // Ничего или обнулить
                    u64* target_ptr = reinterpret_cast<u64*>(bytes_.data() + reloc.offset);
                    *target_ptr = 0;
                    print_formatted_line("link", reloc.offset, *target_ptr, reloc.name, reloc_type_to_string(reloc.type), "");
                    break;
                }
                case Relocation::Type::FIXED_ADDRESS: {
                    // АДДИТИВНАЯ ЗАПИСЬ: Мы ничего не ищем по имени, 
                    // мы просто считаем, что в *target_ptr уже лежит офсет,
                    // и его нужно оставить как есть для загрузчика.
                    // (Или прибавить базу, если link() — это финальная стадия).
                    u64* target_ptr = reinterpret_cast<u64*>(bytes_.data() + reloc.offset);
                    print_formatted_line("link", reloc.offset, *target_ptr, reloc.name, reloc_type_to_string(reloc.type), "");
                    break; 
                }
                case Relocation::Type::LABEL_ADDRESS: {
                    // ПРЯМАЯ ЗАПИСЬ: Находим метку и пишем её адрес "с нуля"
                    const RelLabel* label = get_label(reloc.name);
                    if (!label) throw std::runtime_error("Undefined label: " + reloc.name);
                    u64* target_ptr = reinterpret_cast<u64*>(bytes_.data() + reloc.offset);
                    *target_ptr = label->offset; 
                    print_formatted_line("link", reloc.offset, *target_ptr, reloc.name, reloc_type_to_string(reloc.type), "");
                    break;
                }
                case Relocation::Type::BRANCH_DISP16: {
                    // ПРЯМАЯ ЗАПИСЬ: Находим метку и пишем смещение от текущей позиции до метки
                    const RelLabel* label = get_label(reloc.name);
                    if (!label) throw std::runtime_error("Undefined label: " + reloc.name);
                    u64 reloc_pos = reloc.offset;                   
                    u64 target_pos = label->offset;
                    s64 offset = static_cast<s64>(target_pos) - static_cast<s64>(reloc_pos);
                    s64 inst_offset = offset / sizeof(Instruction);
                    Instruction* instr = reinterpret_cast<Instruction*>(bytes_.data() + reloc.offset);
                    instr->imm16 = static_cast<u16>(inst_offset);
                    auto disasm = InstructionTable::instance().disassemble(*instr);
                    print_formatted_line("link", reloc.offset, *(u64*)instr, reloc.name, disasm, "");
                    break;
                }
            }
        }
    }

    /**
     * Link and update file size
     */
    BinaryFile* link_file() {
        link();
        BinaryFile* binary_file = reinterpret_cast<BinaryFile*>(data());
        binary_file->file_size = size();
        return binary_file;
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
        //
        result += fmt::format("  Relocations[{}] [", relocatations_.size());
        for (size_t j = 0; j < std::min(relocatations_.size(), size_t(MAX_LEN)); j++) {
            result += fmt::format("{}\n", relocatations_[j].to_string());
        }
        result += "]";
        // 
        result += fmt::format("  Labels[{}] [", labels_.size());
        for (size_t j = 0; j < std::min(labels_.size(), size_t(MAX_LEN)); j++) {
            result += fmt::format("{}\n", labels_[j].to_string());
        }
        result += "]";

        return result;
    }
    std::string reloc_type_to_string(Relocation::Type type) {
        switch (type) {
            case Relocation::Type::FIXED_ADDRESS:
                return "FIXED_ADDRESS";
            case Relocation::Type::LABEL_ADDRESS:
                return "LABEL_ADDRESS";
            case Relocation::Type::BRANCH_DISP16:
                return "BRANCH_DISP16";
            case Relocation::Type::NULL_ADDRESS:
                return "NULL_ADDRESS";
        }
    }
    void print_formatted_line(const std::string& prefix, 
                            u64 address, 
                            const std::string& label, 
                            const std::string& instruction, 
                            const std::string& comment,
                            int prefix_width = 20,
                            int addr_width = 16,
                            int data_width = 16,
                            int label_width = 24,
                            int inst_width = 30) {
        print_formatted_line(prefix, 
                                    address, 
                                    0, 
                                    label, 
                                    instruction, 
                                    comment,
                                    prefix_width,
                                    addr_width,
                                    data_width,
                                    label_width,
                                    inst_width,
                                    false);                                
    }

    void print_formatted_line(const std::string& prefix, 
                            u64 address, 
                            u64 data, 
                            const std::string& label, 
                            const std::string& instruction, 
                            const std::string& comment,
                            int prefix_width = 20,
                            int addr_width = 16,
                            int data_width = 16,
                            int label_width = 24,
                            int inst_width = 30,
                            bool dump_data = true) {
        std::string result;
        
        // Prefix
        result += fmt::format("{:<{}}", prefix, prefix_width);
        
        // Address
        result += fmt::format(" 0x{:0{}X}", address, addr_width);
        
        // Data
        if (dump_data)
            result += fmt::format(" 0x{:0{}X}", data, data_width);
        else
            result += fmt::format("   {: >{}}", "", data_width);  // пробелы

        // Label
        if (!label.empty()) {
            result += fmt::format(" {}:", label);
            int padding = label_width - (int)label.length() - 1;
            if (padding > 0) result += std::string(padding, ' ');
            else result += " ";
        } else {
            result += std::string(label_width, ' ');
        }
        
        // Instruction
        if (!instruction.empty()) {
            result += fmt::format(" {}", instruction);
            int padding = inst_width - (int)instruction.length();
            if (padding > 0) result += std::string(padding, ' ');
            else result += " ";
        } else {
            result += std::string(inst_width, ' ');
        }
        
        // Comment
        if (!comment.empty()) {
            result += fmt::format(" ;; {}", comment);
        }
        
        lg::info("{}", result);
    }
    std::string name() const { return name_; }
    std::string type_tag() const { return type_; }
    void set_name(std::string name) { name_ = name; }
    void set_type(std::string type_tag) { type_ = type_tag; }
private:
    std::vector<u8> bytes_;
    std::vector<Relocation> relocatations_;
    std::vector<RelLabel> labels_;
    std::string name_;
    std::string type_;
};

} // namespace carbon::files