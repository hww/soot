// Globals.hpp
#pragma once
#include <stdexcept>
#include <unordered_map>
#include <string>
#include <expected>
#include <vector>
#include <iostream>
#include "common/carbon/lib/StringId.hpp"
#include "file/BinaryFile.hpp"
#include "file/DCHeader.hpp"
#include "file/BinaryFile.hpp"
#include "fmt/format.h"

namespace carbon {

struct Symbol {
    void* ptr = nullptr;       // Указатель на данные/функцию в памяти
    StringId typeId;           // Тип (для проверки рантаймом)
    StringId ownerModulePath;  // Путь модуля, который владеет этим символом
};

class Globals {
public:
    // Синглтон для доступа из любой точки движка
    static Globals& inst() {
        static Globals instance;
        return instance;
    }

    // Запрещаем копирование
    Globals(const Globals&) = delete;
    Globals& operator=(const Globals&) = delete;

    /**
     * @brief Загрузка бинарного модуля и регистрация его символов
     * Максимально быстрая загрузка в глобальную область.
     */
    bool load_module(const std::string& path) {
        StringId modulePathId(path);

        // 1. Проверяем, не загружен ли уже модуль
        if (m_modules.contains(modulePathId)) {
            return true; 
        }

        // 2. Читаем файл (используем ваш метод BinaryFile::from_path)
        auto result = BinaryFile::from_path(path);
        if (!result.has_value()) {
            std::cerr << "[Globals] Failed to load: " << result.error() << std::endl;
            return false;
        }

        // Перемещаем файл во владение Globals
        BinaryFile& file = m_modules.emplace(modulePathId, std::move(*result)).first->second;

        // 3. Регистрация экспортируемых символов
        // Предполагаем, что BinaryFile предоставляет доступ к таблице экспорта через заголовок
        const DC_Header* header = file.m_dcheader;
        if (header) {
            // Проходим по записям (Entry) в файле
            // В реальной системе m_pStartOfData — это смещение внутри m_bytes
            for (u32 i = 0; i < header->m_numEntries; ++i) {
                const auto& entry = header->m_pStartOfData[i];
                
                // Регистрируем символ. Если такой ID уже был, он ПЕРЕЗАПИСЫВАЕТСЯ (Shadowing)
                // Это обеспечивает максимальную скорость: мы всегда берем последнее определение.
                m_symbols[StringId(entry.m_nameID)] = Symbol {
                    const_cast<void*>(entry.m_entryPtr),
                    StringId(entry.m_typeId),
                    modulePathId
                };
            }
        }

        return true;
    }

    /**
     * @brief Выгрузка модуля и очистка его символов
     */
    void unload_module(const std::string& path) {
        StringId modulePathId(path);
        if (!m_modules.contains(modulePathId)) return;

        // 1. Удаляем все символы, принадлежащие этому модулю
        // std::erase_if доступен в C++20 и выше
        std::erase_if(m_symbols, [modulePathId](const auto& item) {
            return item.second.ownerModulePath == modulePathId;
        });

        // 2. Удаляем сам объект BinaryFile (сработает деструктор и unique_ptr очистит память)
        m_modules.erase(modulePathId);
        
        std::cout << "[Globals] Module " << path << " unloaded." << std::endl;
    }

    /**
     * @brief Самый быстрый поиск символа (Runtime Hot Path)
     * @return Указатель на данные или nullptr
     */
    [[nodiscard]] inline void* find_symbol_ptr(StringId name) const noexcept {
        auto it = m_symbols.find(name);
        if (it != m_symbols.end()) {
            return it->second.ptr;
        }
        return nullptr;
    }

    /**
     * @brief Поиск символа с проверкой типа
     */
    template<typename T>
    [[nodiscard]] T* get_as(StringId name, StringId expectedType) const noexcept {
        auto it = m_symbols.find(name);
        if (it != m_symbols.end() && it->second.typeId == expectedType) {
            return static_cast<T*>(it->second.ptr);
        }
        return nullptr;
    }

    void clear_all() {
        m_symbols.clear();
        m_modules.clear(); // Полная очистка памяти всех модулей
    }


    // Возвращает дефиницию определенного типа
    void* lookup(StringId name, StringId type_id, bool throw_error = false) {
        auto it = m_symbols.find(name);
        if (it != m_symbols.end()) {
                if (it->second.typeId == type_id) 
                    return  it->second.ptr;
            if (throw_error)
                throw std::runtime_error(fmt::format("Expected global {} with type {}, found {}",  
                    name.to_cstring(), type_id.to_cstring(),  it->second.typeId.to_cstring()));
        }
        if (throw_error)
            throw std::runtime_error(fmt::format("Undefined global {}", name.to_cstring()));
        return nullptr;
    }

private:
    Globals() = default;

    // Плоская таблица для O(1) доступа
    std::unordered_map<StringId, Symbol> m_symbols;

    // Контейнер модулей, владеющий их памятью (RAII)
    std::unordered_map<StringId, BinaryFile> m_modules;
};

} // namespace carbon