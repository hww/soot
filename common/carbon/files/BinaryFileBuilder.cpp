#include "common/carbon/files/BinaryFileBuilder.hpp"
#include "common/carbon/modules/Module.hpp"
#include "files/BinaryFile.hpp"
#include "files/FunctionDesc.hpp"
#include "files/TypeDesc.hpp"
#include "files/StateDesc.hpp"
#include "fmt/format.h"
#include "lib/StringId.hpp"
#include "util/Log.hpp"
#include <cstddef>

using namespace carbon::lib;
using namespace carbon::modules;

namespace carbon::files {

    /** Построить и загрузить модуль в пул */
    std::shared_ptr<Module> BinaryFileBuilder::build_module() {
        // Make new module
        auto module = std::make_shared<Module>();
        // Build file for module
        std::vector<u8> data = build();
        BinaryFile::make_for_memory(data,module.get());
        // Setup module file
        module->name = StringId(name.c_str());
        module->set_file(std::move(data));
        return module;
    }
    
    BinaryFile*  BinaryFileBuilder::build_file() {
        std::vector<u8> data = build();
        BinaryFile::make_for_memory(data,nullptr);
        return reinterpret_cast<BinaryFile*>(data.data());
    }


    /** Построить бинарник - ПРОСТОЙ ВАРИАНТ */
    std::vector<u8> BinaryFileBuilder::build() {
        std::vector<u8> buffer(65536);
        
        // Заголовок
        BinaryFile* header = reinterpret_cast<BinaryFile*>(buffer.data());
        new (header) BinaryFile();
        header->base_offset = 0;
        header->magic = BinaryFile::MAGIC;
        header->generation = BinaryFile::CURRENT_GENERATION;
        
        u32 current_pos = sizeof(BinaryFile);
        
        // Таблица дефиниций
        Definition* defs_table = reinterpret_cast<Definition*>(buffer.data() + current_pos);
        u32 defs_count = definitions_.size();
        current_pos += defs_count * sizeof(Definition);
        current_pos = align_size(current_pos);
        
        // Сохраняем позиции для каждого определения
        std::vector<u32> def_offsets(defs_count);
        
        for (u32 i = 0; i < defs_count; i++) {
            const auto& def = definitions_[i];
            
            // Инициализируем Definition в таблице
            new (&defs_table[i]) Definition{
                StringId(def.name),
                StringId(def.type),
                def.flags,
                0,
                Ptr<u8>()
            };
            
            // Запоминаем позицию, куда будем копировать данные
            def_offsets[i] = current_pos;
            current_pos += def.data.bytes().size();
            current_pos = align_size(current_pos);
        }
        
        // Расширяем буфер
        ensure_capacity(buffer, current_pos);
        buffer.resize(current_pos);
        
        // Копируем данные и обновляем указатели
        for (u32 i = 0; i < defs_count; i++) {
            const auto& def = definitions_[i];
            u8* dest = buffer.data() + def_offsets[i];
            
            // Копируем буфер
            std::memcpy(dest, def.data.bytes().data(), def.data.bytes().size());
            
            // Обновляем указатель в таблице
            defs_table[i].data = Ptr<u8>(def_offsets[i]);
            
            // Релокация: обновляем все помеченные указатели внутри этого буфера
            for (u32 reloc_offset : def.data.relocatable_offsets()) {
                u64* ptr = reinterpret_cast<u64*>(dest + reloc_offset);
                *ptr += def_offsets[i];
            }
        }
        
        // Обновляем заголовок
        header->file_size = current_pos;
        header->used_size = current_pos;
        header->definitions_count = defs_count;
        header->definitions = Ptr<Definition>(sizeof(BinaryFile));
        
        buffer.resize(current_pos);
        
        // Релокация всего файла
        BinaryFile::make_for_memory(buffer, nullptr);
        
        return buffer;
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

                result += fmt::format("data:{} bytes", def.data.size());

            // Показать начало данных для простых типов
            result += def.data.inspect();
            result += "\n";
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
            result += fmt::format("Hex: {}\n", header->dump());
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