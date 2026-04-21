#pragma once
#include "CommonTypes.hpp"
#include "DCHeader.hpp"
#include "DCScript.hpp"
#include "common/carbon/lib/SIDBase.hpp"
#include "common/carbon/vm/Instructions.hpp"
#include "lib/ByteUtils.hpp"


#include <memory>
#include <string>
#include <map>
#include <set>

namespace carbon {

	struct location {
		const std::byte* m_ptr = nullptr;

		location() noexcept {};
		location(const void* ptr) noexcept : m_ptr(reinterpret_cast<const std::byte*>(ptr)) {};

		[[nodiscard]] location &from(const location& rhs, const i32 offset = 0) noexcept {
			m_ptr = rhs.get<std::byte*>() + offset;
			return *this;
		}

		template<typename T>
		[[nodiscard]] const T* as(const i32 offset = 0) const noexcept {
			return reinterpret_cast<const T*>(m_ptr + offset);
		}

		template<typename T>
		[[nodiscard]] const T& get(const i32 offset = 0) const noexcept {
			return *reinterpret_cast<const T*>(m_ptr + offset);
		}

		[[nodiscard]] p64 num() const noexcept {
			return reinterpret_cast<p64>(m_ptr);
		}

		[[nodiscard]] location aligned() const noexcept {
			return location(m_ptr - num() % 8);
		}

		[[nodiscard]] location operator+(const u64 rhs) const noexcept {
			return location(m_ptr + rhs);
		}

		[[nodiscard]] location operator-(const u64 rhs) const noexcept {
			return location(m_ptr - rhs);
		}

		[[nodiscard]] bool operator>(const location &rhs) const noexcept {
			return reinterpret_cast<p64>(m_ptr) > reinterpret_cast<p64>(rhs.m_ptr);
		}

		[[nodiscard]] bool operator>=(const location &rhs) const noexcept {
			return reinterpret_cast<p64>(m_ptr) >= reinterpret_cast<p64>(rhs.m_ptr);
		}

		[[nodiscard]] bool is_aligned() const noexcept {
			return reinterpret_cast<p64>(m_ptr) % 8 == 0;
		}
	};


    /**
     * Symbol type used in the symbol's table
     */
    enum class symbol_type {
        B8,
        I32,
        F32,
        SS,
        HASH,
        LAMBDA,
        UNKNOWN
    };

    /**
     * Symbol used in the symbol's table
     */
    struct symbol {
        symbol_type type;
        sid64 id;
        union {
            i32* i32_ptr;
            f32* f32_ptr;
            bool* b8_ptr;
            StateScript* ss_ptr;
            ScriptLambda* lambda_ptr;
            uint64_t* hash_ptr;
            DCEntry raw_entry;
        };
    };

    class BinaryFile
    {

    public:
        BinaryFile() = default;

        BinaryFile(std::filesystem::path path, const u64 size, byte_uptr&& bytes, DC_Header* dcheader) noexcept
            : m_path(std::move(path)), m_dcheader(dcheader), m_size(size), m_bytes(std::move(bytes)) {};

        // 1. Запрещаем копирование (Rule of Five)
        BinaryFile(const BinaryFile&) = delete;
        BinaryFile& operator=(const BinaryFile&) = delete;

        // 2. Разрешаем перемещение (обязательно для std::expected и return)
        BinaryFile(BinaryFile&&) noexcept = default;
        BinaryFile& operator=(BinaryFile&&) noexcept = default;

        // 3. Деструктор — полагаемся на стандартный (default)
        ~BinaryFile() = default;


        [[nodiscard]] static std::expected<BinaryFile, std::string> from_path(const std::filesystem::path& path) noexcept;
        [[nodiscard]] static std::expected<BinaryFile, std::string> from_buffer(const std::filesystem::path& path, byte_uptr bytes, size_t size) noexcept;
        [[nodiscard]] bool save(const std::filesystem::path& path) noexcept;

        std::filesystem::path m_path;
        const DC_Header* m_dcheader = nullptr;
        const StateScript* m_dcscript = nullptr;
        std::size_t m_size = 0;
        byte_uptr m_bytes;
        byte_uptr m_pointedAtTable;
        location m_strings;
        location m_relocTable;
        std::map<sid64, const std::string> m_sidCache;
        std::set<p64> m_emittedStructs;

        [[nodiscard]] bool is_file_ptr(const location) const noexcept;
        [[nodiscard]] bool gets_pointed_at(const location) const noexcept;
        [[nodiscard]] bool is_string(const location) const noexcept;
        [[nodiscard]] byte_uptr get_unmapped() const;

        std::string inspect() { return ""; }


    private:
        void read_reloc_table() noexcept;
        void replace_newlines_in_stringtable() noexcept;
    };

}
