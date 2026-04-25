#pragma once

#include "CommonTypes.hpp"
#include "DCHeader.hpp"
#include "DCScript.hpp"
#include "lib/ByteUtils.hpp"
#include <cstddef>
#include <stdexcept>
#include <vector>

namespace carbon {

    struct function;
    struct global_state;

    struct ProgramBinaryElement {

        ProgramBinaryElement(const u64 size) noexcept;
        ProgramBinaryElement(const ProgramBinaryElement&) = delete;
        ProgramBinaryElement& operator=(const ProgramBinaryElement&) = delete;
        ProgramBinaryElement(ProgramBinaryElement&& other) noexcept
            : m_entry(std::move(other.m_entry))  // ← std::move для POD - просто копия
            , m_rawData(std::move(other.m_rawData))
            , m_stringOffsets(std::move(other.m_stringOffsets))
            , m_relocTable(std::move(other.m_relocTable))
            , m_byteOffset(other.m_byteOffset)
            , m_bitOffset(other.m_bitOffset)
        {
            // Очищаем other
            other.m_rawData.clear();
            other.m_relocTable.clear();
            other.m_stringOffsets.clear();
            other.m_entry.m_entryPtr = nullptr;
            other.m_byteOffset = 0;
            other.m_bitOffset = 0;
        }    
        /**
         * @brief Сериализует объект в сырые байты и регистрирует позиции для релокации.
         * * Метод копирует побайтовое представление объекта T в конец внутреннего буфера m_rawData.
         * Дополнительные аргументы (bits) используются для пометки конкретных частей записанных данных
         * как требующих релокации (например, если записываемая структура содержит указатели).
         * * @tparam T Тип записываемых данных (обычно POD-структура или примитив).
         * @tparam bits Вариативный список аргументов, интерпретируемых как флаги или смещения для таблицы релокаций.
         * * @param data Ссылка на объект, байты которого нужно записать.
         * @param b Флаги релокации, которые будут обработаны методом insert_into_reloctable.
         * * @note Метод помечен как noexcept, так как предполагает прямую работу с памятью и локальными данными.
         * @warning Тип T должен быть Trivially Copyable для корректной сериализации через reinterpret_cast.
         * 
         * struct Header {
         *     u64 version;
         *     void* pTable1;
         *     void* pTable2;
         * };
         * 
         * Header h = { 1, ptr1, ptr2 };
         * // Передаем 3 бита: версия (нет), таблица1 (да), таблица2 (да)
         * push_bytes(h, 0, 1, 1)         
         */
        template<typename T, typename ... bits>
        void push_bytes(const T& data, bits... b) noexcept {
            check_size();

            const std::byte* p = reinterpret_cast<const std::byte*>(std::addressof(data));
            m_rawData.insert(m_rawData.end(), p, p + sizeof(T));
            const std::vector<u8> bits_list = {static_cast<u8>(b)...};
            
            ///if (bits_list.empty()) return;  // ← добавить проверку!
            
            for (u32 i = 0; i < bits_list.size() - 1; ++i) {
                insert_into_reloctable(bits_list[i], 8);
            }
            insert_into_reloctable(bits_list.back(), (sizeof(T) / 8) % 8);

            check_size();
        }

        void push_blob(const void* data, size_t size, u8 relocation_bit = 0) noexcept {
            check_size();            
            
            const std::byte* p = reinterpret_cast<const std::byte*>(data);
            
            // 1. Копируем данные
            m_rawData.insert(m_rawData.end(), p, p + size);

            // 2. Рассчитываем количество 8-байтовых слотов
            // Используем округление вверх, чтобы покрыть весь блок
            size_t num_slots = (size + 7) / 8;

            // 3. Регистрируем в таблице релокаций
            for (size_t i = 0; i < num_slots; ++i) {
                insert_into_reloctable(relocation_bit, 8); 
            }
            check_size();
        }

        void check_size() {
            auto data_size = m_rawData.size() / 8;
            auto reloc_size = m_relocTable.size();
            if (data_size != reloc_size) {
                throw std::runtime_error(fmt::format("ProgramBinaryElement raw_data {} not equal with reloc table size {}", data_size, reloc_size));
            }
        }

        void insert_into_reloctable(const u8 bits, const u64 num_bits) noexcept;

        void insert_string_offset() noexcept;
        void insert_string_offset(const u64 offset) noexcept;

        void adjust_offsets(const u64 offset) noexcept;

        size_t size() { return m_rawData.size(); }
        bool is_empty() { return m_rawData.size() == 0;}

        void dump(const std::string& title = "", size_t max_len = 256);

        byte_uptr to_byte_uptr() const;

        DCEntry m_entry;

        std::vector<std::byte> m_rawData;
        std::vector<u64> m_stringOffsets;
        std::vector<bool> m_relocTable;
    

        u64 m_byteOffset = 0;
        u8 m_bitOffset = 0;
    };

} // namespace vm
