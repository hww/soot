#include "binary_file_builder.hpp"
#include "fmt/format.h"

namespace vm {

    /** Построить и загрузить модуль в пул */
    std::shared_ptr<Module> BinaryFileBuilder::build_and_load_to_pool() {
        std::vector<u8> data = build();

        // Создаем модуль
        auto module = std::make_shared<Module>(
            module_name_,
            module_name_,
            std::filesystem::path("generated.bin")
        );

        // Загружаем в BinaryFilePool
        void* pool_addr = BinaryFilePool::allocate(
            static_cast<u32>(data.size()),
            module.get(),
            module->name
        );

        if (!pool_addr) {
            throw std::runtime_error("Failed to allocate memory in BinaryFilePool");
        }

        // Копируем данные в пул
        std::memcpy(pool_addr, data.data(), data.size());

        // Релоцируем указатели BinaryFile
        BinaryFile* binary_file = static_cast<BinaryFile*>(pool_addr);
        binary_file->relocate_pointers(BinaryFilePool::get_base_address());

        // Устанавливаем owner_module для всех ByteCode
        setup_bytecode_owners(binary_file, module.get());

        module->load_state = Module::LoadState::BINARY_LOADED;
        build_export_table(module);

        return module;
    }

    /** Построить бинарник - ПРОСТОЙ ВАРИАНТ */
    std::vector<u8> BinaryFileBuilder::build() {
        if (module_name_ == 0) {
            throw std::runtime_error("Module name not set");
        }

        // 1. Создаем буфер начального размера (64KB)
        std::vector<u8> buffer(65536);
        u32 current_pos = 0;

        // 2. Заголовок файла
        BinaryFile* header = reinterpret_cast<BinaryFile*>(buffer.data());
        new (header) BinaryFile();
        header->base_offset = 0;

        current_pos += sizeof(BinaryFile);

        // 3. Таблица дефиниций
        Definition* defs_table = reinterpret_cast<Definition*>(buffer.data() + current_pos);
        u32 defs_count = static_cast<u32>(definitions_.size());

        lg::info("=== BUILD DEBUG ===");
        lg::info("Building {} definitions", defs_count);

        for (u32 i = 0; i < defs_count; i++) {
            const auto& def_data = definitions_[i];
            lg::info("Definition[{}]: name='{}'({}), type='{}'({})",
                i,
                string_id::to_string(def_data.name), def_data.name,
                string_id::to_string(def_data.type), def_data.type);

            // ЯВНАЯ инициализация каждого определения
            new (&defs_table[i]) Definition{
                def_data.name,      // StringId name
                def_data.type,      // StringId type  
                Ptr<void>(0)        // Временный нулевой указатель
            };

            // Проверим что записалось
            lg::info("  Written: name={}, type={}, data_ptr={}",
                defs_table[i].name, defs_table[i].type, defs_table[i].data_ptr.offset);
        }

        current_pos += defs_count * sizeof(Definition);
        current_pos = align_size(current_pos);

        // 4. Записываем данные дефиниций и обновляем указатели
        for (u32 i = 0; i < defs_count; i++) {
            const auto& def = definitions_[i];

            // Проверяем, не вышли ли за пределы буфера
            ensure_capacity(buffer, current_pos + 1024);

            // Обновляем указатель в таблице дефиниций
            defs_table[i].data_ptr = Ptr<void>(current_pos);
            lg::info("Updated defs_table[{}].data_ptr = {}", i, current_pos);
            if (def.type == SID("function")) {
                // Записываем ByteCode структуру
                ByteCode* bc = reinterpret_cast<ByteCode*>(buffer.data() + current_pos);
                new (bc) ByteCode();  // Явная инициализация
                current_pos += sizeof(ByteCode);

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

        return buffer;
    }


    /** Просмотреть входные данные которые были добавлены */
    std::string BinaryFileBuilder::inspect_input() const {
        std::string result = fmt::format("BinaryFileBuilder<name:{}>:\n",
            string_id::to_string(module_name_));

        result += fmt::format("  Total definitions: {}\n", definitions_.size());

        for (size_t i = 0; i < definitions_.size(); i++) {
            const auto& def = definitions_[i];
            result += fmt::format("  [{}] {} ({}): ", i,
                string_id::to_string(def.name),
                string_id::to_string(def.type));

            if (def.type == SID("function")) {
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

        // Если бинарник валиден, показать его структуру
        if (binary.size() >= sizeof(BinaryFile)) {
            const BinaryFile* header = reinterpret_cast<const BinaryFile*>(binary.data());
            if (header->is_valid()) {
                result += "BINARY STRUCTURE:\n";
                result += header->inspect();
            }
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




}