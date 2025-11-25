#include "binary_file_pool.hpp"
#include "types.hpp"
#include "module.hpp"
#include "ptr.hpp"

namespace vm {


    // Инициализация пула - внутри выделяем больше на INTERNAL_OFFSET
    bool BinaryFilePool::initialize(u32 total_size) {
        if (pool_base) shutdown();

        // Выделяем дополнительную память для смещения
        pool_base = new u8[total_size + INTERNAL_OFFSET];
        if (!pool_base) return false;

        pool_size = total_size + INTERNAL_OFFSET; // Реальный размер
        current_offset = INTERNAL_OFFSET; // Начинаем после смещения
        allocations.clear();
        std::memset(pool_base, 0, pool_size);

        g_module_pool_base = pool_base;

        return true;
    }

    // Выделение памяти в пуле
    void* BinaryFilePool::allocate(u32 size, Module* owner_module, StringId module_name) {
        if (!pool_base || size == 0) return nullptr;

        u32 aligned_size = align_size(size);

        // Проверяем место, компактифицируем если нужно
        if (current_offset + aligned_size > pool_size) {
            if (!compactify()) return nullptr;
            if (current_offset + aligned_size > pool_size) return nullptr;
        }

        void* addr = pool_base + current_offset;
        allocations.emplace_back(addr, aligned_size, owner_module, module_name);
        current_offset += aligned_size;
        auto binary_file = reinterpret_cast<BinaryFile*>(addr);
        owner_module->on_pool_relocation(binary_file);
        return addr;
    }

    // Освобождение памяти модуля
    bool BinaryFilePool::deallocate(StringId module_name) {
        auto it = std::find_if(allocations.begin(), allocations.end(),
            [module_name](const Allocation& alloc) {
                return alloc.module_name == module_name;
            });

        if (it != allocations.end()) {
            if (it->owner_module) {
                it->owner_module->on_pool_deaelocation(nullptr);
                it->owner_module = nullptr;
            }
            allocations.erase(it);
            return true;
        }
        return false;
    }

    // Компактификация памяти
    bool BinaryFilePool::compactify() {
        if (!pool_base || allocations.empty()) {
            current_offset = INTERNAL_OFFSET;
            return true;
        }

        u32 new_offset = INTERNAL_OFFSET;
        std::vector<Allocation> new_allocations;

        for (auto& alloc : allocations) {
            void* new_addr = pool_base + new_offset;

            if (alloc.address != new_addr) {
                std::memmove(new_addr, alloc.address, alloc.size);
                alloc.address = new_addr;
                auto binary_file = reinterpret_cast<BinaryFile*>(new_addr);
                alloc.owner_module->on_pool_relocation(binary_file);
            }

            new_allocations.push_back(alloc);
            new_offset += alloc.size;
        }

        allocations = std::move(new_allocations);
        current_offset = new_offset;
        g_module_pool_base = pool_base;
        return true;
    }

    // Освобождение пула
    void BinaryFilePool::shutdown() {
        for (auto& alloc : allocations) {
            if (alloc.owner_module) {
                alloc.owner_module->on_pool_relocation(nullptr);
				alloc.owner_module = nullptr;
            }
        }

        delete[] pool_base;
        pool_base = nullptr;
        pool_size = 0;
        current_offset = 0;
        allocations.clear();

        g_module_pool_base = nullptr;
        fmt::print("Shutdown completed successfully\n");
    }
}