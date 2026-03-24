#include "common/carbon/files/BinaryFileBuilder.hpp"
#include "common/carbon/modules/Module.hpp"
#include "files/BinaryFile.hpp"
#include "files/FunctionDesc.hpp"
#include "files/TypeDesc.hpp"
#include "files/StateDesc.hpp"
#include "fmt/format.h"
#include "lib/StringId.hpp"

using namespace carbon::lib;
using namespace carbon::modules;

namespace carbon::files {

    /** Построить и загрузить модуль в пул */
    std::shared_ptr<Module> BinaryFileBuilder::build_module() {
        std::vector<u8> data = build();
        BinaryFile::make_for_memory(data);
        
        auto module = std::make_shared<Module>();
        module->name = StringId(name.c_str());
        module->set_file(std::move(data));
        return module;
    }

    /** Построить бинарник - ПРОСТОЙ ВАРИАНТ */
    std::vector<u8> BinaryFileBuilder::build() {
        // 1. Создаем буфер начального размера (64KB)
        std::vector<u8> buffer(65536);

        // 2. Заголовок файла
        BinaryFile* header = reinterpret_cast<BinaryFile*>(buffer.data());
        new (header) BinaryFile();
        header->base_offset = 0;  //<< чтобы релокация работала необходимо записать 0 в offset
        header->magic = BinaryFile::MAGIC;
        header->generation = BinaryFile::CURRENT_GENERATION;

        u32 current_pos = 0;
        current_pos += sizeof(BinaryFile);
        // 3. Таблица дефиниций
        Definition* defs_table = reinterpret_cast<Definition*>(buffer.data() + current_pos);
        u32 defs_count = static_cast<u32>(definitions_.size());

        lg::info("=== BUILD DEBUG ===");
        lg::info("Building {} definitions", defs_count);

        for (u32 i = 0; i < defs_count; i++) {
            const auto& def_data = definitions_[i];
            lg::info("Definition[{}] :name='{}'({}) :type '{}'({})",
                i,
                def_data.name, def_data.name,
                def_data.type, def_data.type);

            // ЯВНАЯ инициализация каждого определения
            new (&defs_table[i]) Definition{
                StringId(def_data.name),      // StringId name
                StringId(def_data.type),      // StringId type  
                def_data.flags,
                Ptr<u8>()           // Временный нулевой указатель
            };

            // Проверим что записалось
            lg::info("  Written :name {} :type {} :data-ptr {}",
                defs_table[i].name, defs_table[i].type, defs_table[i].data.offset);
        }

        current_pos += defs_count * sizeof(Definition);
        current_pos = align_size(current_pos);

        // 4. Записываем данные дефиниций и обновляем указатели
        for (u32 i = 0; i < defs_count; i++) {
            const auto& def = definitions_[i];

            // Проверяем, не вышли ли за пределы буфера
            ensure_capacity(buffer, current_pos + 1024);

            // Обновляем указатель в таблице дефиниций
            defs_table[i].data = Ptr<u8>(current_pos);
            lg::info("Updated defs_table[{}].data_ptr = {}", i, current_pos);
            if (def.type == "function") {
                // Записываем FunctionDesc структуру
                FunctionDesc* bc = reinterpret_cast<FunctionDesc*>(buffer.data() + current_pos);
                new (bc) FunctionDesc();  // Явная инициализация
                current_pos += sizeof(FunctionDesc);

                // Код
                if (!def.code.empty()) {
                    u32 code_size = static_cast<u32>(def.code.size() * sizeof(Instruction));
                    ensure_capacity(buffer, current_pos + code_size);

                    bc->code_ptr = Ptr<Instruction>(current_pos);
                    bc->code_count = static_cast<u32>(def.code.size());

                    Instruction* code_dest = reinterpret_cast<Instruction*>(buffer.data() + current_pos);
                    std::memcpy(code_dest, def.code.data(), code_size);
                    current_pos += code_size;
                    current_pos = align_size(current_pos);
                }

                // Данные
                if (!def.data.empty()) {
                    ensure_capacity(buffer, current_pos + def.data.size());

                    bc->data_ptr = Ptr<u8>(current_pos);
                    bc->data_size = static_cast<u32>(def.data.size());

                    u8* data_dest = buffer.data() + current_pos;
                    std::memcpy(data_dest, def.data.data(), def.data.size());
                    current_pos += def.data.size();
                    current_pos = align_size(current_pos);
                }

                // Отладочная информация
                if (!def.debug_info.empty()) {
                    u32 debug_size = static_cast<u32>(def.debug_info.size() * sizeof(SourceLocation));
                    ensure_capacity(buffer, current_pos + debug_size);

                    bc->debug_ptr = Ptr<SourceLocation>(current_pos);
                    bc->debug_count = static_cast<u32>(def.debug_info.size());

                    SourceLocation* debug_dest = reinterpret_cast<SourceLocation*>(buffer.data() + current_pos);
                    std::memcpy(debug_dest, def.debug_info.data(), debug_size);
                    current_pos += debug_size;
                    current_pos = align_size(current_pos);
                }
            }
            else {
                // Простая дефиниция - просто копируем данные
                ensure_capacity(buffer, current_pos + def.data.size());

                u8* data_dest = buffer.data() + current_pos;
                std::memcpy(data_dest, def.data.data(), def.data.size());
                current_pos += def.data.size();
                current_pos = align_size(current_pos);
            }
        }

        // 5. Обновляем заголовок
        header->file_size = current_pos;
        header->used_size = current_pos;
        header->definitions_count = defs_count;
        header->definitions = Ptr<Definition>(sizeof(BinaryFile));

        // 6. Обрезаем буфер до реального размера
        buffer.resize(current_pos);

        // Теперь буфер готов, можно делать make_for_memory
        BinaryFile::make_for_memory(buffer);
        
        return buffer;
    }

    /** Добавить тип */
    void BinaryFileBuilder::add_type(std::string name, std::string parent, 
                const std::vector<MethodDesc>& methods,
                const std::vector<StateDesc>& states,
                TypeFlags flags,
                RegClass reg_class,
                int load_size,
                int alignment) {
        
        TypeDesc type;
        type.name = StringId(name);
        type.parent_type_id = StringId(parent);
        type.methods_offset = 0; // будет заполнено позже
        type.states_offset = 0;
        type.methods_count = methods.size();
        type.states_count = states.size();
        type.flags = flags;
        type.reg_class = reg_class;
        type.load_size = load_size;
        type.in_memory_alignment = alignment;
        type.inline_array_stride_alignment = alignment;
        type.inline_array_start_alignment = alignment;
        type.offset = 0;
        
        // Сериализуем методы и состояния
        std::vector<u8> type_data;
        // ... сериализация ...
        
        definitions_.push_back(DefinitionData{
            name, 
            "type", 
            SymbolFlags::Export,
            type_data, 
            {}, 
            {}
        });
    }

    /** Добавить состояние */
    void BinaryFileBuilder::add_state(std::string name, std::string parent,
                const std::vector<FunctionDesc*>& handlers,
                StateFlags flags) {
        
        StateDesc state;
        state.name = StringId(name);
        state.parent_state = StringId(parent);
        state.count = handlers.size();
        state.definitions.offset = 0;
        state.flags = flags;
        
        // Сериализуем обработчики
        std::vector<u8> state_data;
        // ... сериализация ...
        
        definitions_.push_back(DefinitionData{
            name, 
            "state", 
            SymbolFlags::Export,
            state_data, 
            {}, 
            {}
        });
    }
    /** Просмотреть входные данные которые были добавлены */
    std::string BinaryFileBuilder::inspect_input() const {
        std::string result;
        result += fmt::format("  Total definitions: {}\n", definitions_.size());

        for (size_t i = 0; i < definitions_.size(); i++) {
            const auto& def = definitions_[i];
            result += fmt::format("  [{}] {} ({}): ", i,
                def.name,
                def.type);

            if (def.type =="function") {
                result += fmt::format("code:{} instructions, data:{} bytes, debug:{} entries\n",
                    def.code.size(), def.data.size(), def.debug_info.size());

                // Показать первые несколько инструкций
                if (!def.code.empty()) {
                    result += "      instructions: ";
                    for (size_t j = 0; j < std::min(def.code.size(), size_t(3)); j++) {
                        result += fmt::format("{} ", def.code[j].to_string());
                    }
                    if (def.code.size() > 3) {
                        result += fmt::format("... ({} total)", def.code.size());
                    }
                    result += "\n";
                }
            }
            else {
                result += fmt::format("data:{} bytes", def.data.size());

                // Показать начало данных для простых типов
                if (!def.data.empty()) {
                    result += " [";
                    for (size_t j = 0; j < std::min(def.data.size(), size_t(8)); j++) {
                        result += fmt::format("{:02x}", def.data[j]);
                    }
                    if (def.data.size() > 8) {
                        result += "...";
                    }
                    result += "]";
                }
                result += "\n";
            }
        }

        return result;
    }

    /** Дамп памяти полученного бинарника */
    std::string BinaryFileBuilder::inspect_memory_dump(const std::vector<u8>& binary) const {
        if (binary.empty()) {
            return "Binary is empty";
        }

        std::string result = "Binary memory dump:\n";
        const u8* data = binary.data();
        u32 size = static_cast<u32>(binary.size());

        // Дамп заголовка
        if (size >= sizeof(BinaryFile)) {
            const BinaryFile* header = reinterpret_cast<const BinaryFile*>(data);
            result += fmt::format("Header: {}\n", header->to_string());
            result += fmt::format("Hex: {}\n", header->hex_dump());
        }

        // Дамп первых 256 байт или всего файла если он меньше
        u32 dump_size = std::min(size, 256u);
        result += fmt::format("First {} bytes:\n", dump_size);

        for (u32 i = 0; i < dump_size; i += 16) {
            result += fmt::format("{:04x}: ", i);

            // Hex dump
            for (u32 j = 0; j < 16; j++) {
                if (i + j < size) {
                    result += fmt::format("{:02x}", data[i + j]);
                }
                else {
                    result += "  ";
                }
                if (j % 4 == 3) result += " ";
            }

            result += " ";

            // ASCII dump
            for (u32 j = 0; j < 16; j++) {
                if (i + j < size) {
                    u8 c = data[i + j];
                    result += (c >= 32 && c < 127) ? fmt::format("{:c}", c) : ".";
                }
                else {
                    result += " ";
                }
            }

            result += "\n";
        }

        if (size > dump_size) {
            result += fmt::format("... ({} bytes total)\n", size);
        }

        return result;
    }

    /** Полная инспекция - входные данные + результат сборки */
    std::string BinaryFileBuilder::inspect_build_result(const std::vector<u8>& binary) const {
        std::string result = "=== BUILD RESULT INSPECTION ===\n\n";

        result += "INPUT DATA:\n";
        result += inspect_input();
        result += "\n";

        result += "OUTPUT BINARY:\n";
        result += inspect_memory_dump(binary);
        result += "\n";

        // Только если бинарник достаточно большой И валиден
        if (binary.size() >= sizeof(BinaryFile)) {
            // Используем const_cast для снятия константности
            const BinaryFile* header = reinterpret_cast<const BinaryFile*>(binary.data());
            if (header->is_valid()) {
                result += "BINARY STRUCTURE:\n";
                result += header->inspect();
            }
            else {
                result += "Binary header is not valid - cannot interpret structure\n";
            }
        }
        else {
            result += "Binary too small for structure interpretation\n";
        }

        return result;
    }
 

    // Удобные методы для быстрой отладки
    void BinaryFileBuilder::debug_print_input() const {
        lg::info("{}", inspect_input());
    }

    void BinaryFileBuilder::debug_dump_binary(const std::vector<u8>& binary) const {
        lg::info("{}", inspect_memory_dump(binary));
    }

    void BinaryFileBuilder::debug_full_inspect(const std::vector<u8>& binary) const {
        lg::info("{}", inspect_build_result(binary));
    }

} // namespace vm