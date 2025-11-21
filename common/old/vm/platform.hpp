#pragma once

#include <cstdint>
#include <string>
#include <format>

namespace vm
{


	// --------------------------------------------------------------------
	// Basic types
	// --------------------------------------------------------------------

	typedef wchar_t TCHAR;
	typedef uint8_t u8;
	typedef uint16_t u16;
	typedef uint32_t u32;
	typedef uint64_t u64;
	typedef int8_t s8;
	typedef int16_t s16;
	typedef int32_t s32;
	typedef int64_t s64;
	typedef float f32;

	/** for an integer that may hold a pointer (NEVER assume the size of PTRINT). */
	using PTRINT = std::uintptr_t;

	struct u128 
	{
		union
		{
			u64 du64[2];
			s64 ds64[2];
			u32 du32[4];
			s32 ds32[4];
			u16 du16[8];
			s16 ds16[8];
			u8 du8[16];
			s8 ds8[16];
			float f[4];
		};
	};

	static_assert(sizeof(u128) == 16, "u128");

	// --------------------------------------------------------------------
	// Casting structures
	// --------------------------------------------------------------------

	union U32float {
		s32 as_int32;
		u32 as_uint32;
		float as_float;
	};

	union U64float {
		s64 as_int64;
		u64 as_uint64;
		double as_double;
	};

	/**
	 * @brief Swap bytes in the 16 bits word
	 * @param value - the 16 bits value as 0x12
	 * @return the value after bytes swapped as 0x21
	 */
	inline u16 swap_u16(u16 value)
	{
		return (value & 0x00FF) << 8 | (value & 0xFF00) >> 8;
	}

	/**
	 * @brief Swap bytes in the 32 bits word
	 * @param value - the 32 bits value as 0x1234
	 * @return the value after bytes swapped as 0x4321
	 */
	inline u32 swap_u32(u32 value)
	{
		return (value & 0x000000FF) << 24 | (value & 0x0000FF00) >> 8
		| (value & 0x00FF0000) >> 8 | (value & 0xFF000000) >> 24;
	}
	/**
	 * @brief Swap bytes in the 32 bits floating point value
	 * @param value - the 32 bits value as 0x1234
	 * @return the value after bytes swapped as 0x4321
	 */
	inline float swap_f32(float value)
	{
		U32float u;
		u.as_float = value;
		u.as_uint32 = swap_u32(u.as_uint32);
		return u.as_float;
	}
	// --------------------------------------------------------------------
	// Global constants and enums
	// --------------------------------------------------------------------
	/**
	 * For the index is not valid state use this value
	 */
	enum { INDEX_NONE = -1 };

	/**
	 * The argument used to make a constructor as lightweight
	 * value(ENoInit);
	 */
	enum ENoInit { NoInit };

	// --------------------------------------------------------------------
	// Global macro definitions
	// --------------------------------------------------------------------

	#define ML(s) L#s
    #define LOG_WARNING(...) printf(__VA_ARGS__)

	// --------------------------------------------------------------------
	// Global errors 
	// --------------------------------------------------------------------

	/**
	 * @brief Make an exception when we cast the 64 bits values to the 32 bits
	 */
	class FOverflow32 final : public std::exception {
	public:
		FOverflow32(int64_t v) {
			message = std::format("The value id too big for u32 '{0}' : {1}\n", v, std::exception::what());
		}
		FOverflow32(u64 v) {
			message = std::format("The value id too big for s32 '{0}' : {1}\n", v, std::exception::what());
		}
		FOverflow32(double v) {
			message = std::format("The value id too big for s32 '{0}' : {1}\n", v, std::exception::what());
		}
		const char* what() const override {
			return message.c_str();
		}
		std::string message;
	};

	// --------------------------------------------------------------------
	// Safe cast types with the exception if the value is not fit
	// --------------------------------------------------------------------

	inline u32 safe_cast_u_int32(u64 v)
	{
		if (v > std::numeric_limits<u32>::max())
			throw FOverflow32(v);
		return static_cast<u32>(v);
	}
	inline s32 safe_cast_to_int32(s64 v)
	{
		if (v > std::numeric_limits<s32>::max() || v < std::numeric_limits<s32>::min())
			throw FOverflow32(v);
		return static_cast<s32>(v);
	}
	inline float safe_cast_to_float(double v)
	{
		constexpr auto max = static_cast<double>(std::numeric_limits<float>::max());
		constexpr auto min = static_cast<double>(-std::numeric_limits<float>::max());
		if (v > max || v < min)
			throw FOverflow32(v);
		return static_cast<float>(v);
	}
	// --------------------------------------------------------------------
	// The engine types
	// --------------------------------------------------------------------

	struct Vector
	{
		/** @brief zero vector */
		static const Vector ZERO;
		/** @brief the vector's elements */
		float x,y,z,w;
	};
	struct Vector3 { float x,y,z; };
	struct Quaternion { float x, y, z, w; };
	struct BoundingVolume
	{
		Vector3 position;
		Vector3 rotation;
	};
	struct Color3 { u8 r, g, b; };
	struct Color4 { u8 r, g, b, a; };

	// --------------------------------------------------------------------
	// The enum fags
	// --------------------------------------------------------------------

/**
 * @brief Declare methods for bitwise operations with enum type.
 * @param T - The enum type
 * @usage ENUM_FLAG_OPERATORS(EFileOptions);
 */
#define ENUM_FLAG_OPERATORS(T)                                                                                                                                            \
    inline T operator~ (T a) { return static_cast<T>( ~static_cast<std::underlying_type<T>::type>(a) ); }                                                                       \
    inline T operator| (T a, T b) { return static_cast<T>( static_cast<std::underlying_type<T>::type>(a) | static_cast<std::underlying_type<T>::type>(b) ); }                   \
    inline T operator& (T a, T b) { return static_cast<T>( static_cast<std::underlying_type<T>::type>(a) & static_cast<std::underlying_type<T>::type>(b) ); }                   \
    inline T operator^ (T a, T b) { return static_cast<T>( static_cast<std::underlying_type<T>::type>(a) ^ static_cast<std::underlying_type<T>::type>(b) ); }                   \
    inline T& operator|= (T& a, T b) { return reinterpret_cast<T&>( reinterpret_cast<std::underlying_type<T>::type&>(a) |= static_cast<std::underlying_type<T>::type>(b) ); }   \
    inline T& operator&= (T& a, T b) { return reinterpret_cast<T&>( reinterpret_cast<std::underlying_type<T>::type&>(a) &= static_cast<std::underlying_type<T>::type>(b) ); }   \
    inline T& operator^= (T& a, T b) { return reinterpret_cast<T&>( reinterpret_cast<std::underlying_type<T>::type&>(a) ^= static_cast<std::underlying_type<T>::type>(b) ); }


/**
 * \brief Convert strong enum value to integer
 * \tparam E - The enum type
 * \param e - The enum value
 * \return The integer value
 * \udage std::cout << foo(to_underlying(b::B2)) << std::endl;
 */
template <typename E>
constexpr typename std::underlying_type<E>::type to_underlying(E e) noexcept {
	return static_cast<typename std::underlying_type<E>::type>(e);
}
#define ENUM_CAST_TO_INT(T) 

}
