#pragma once

#include "string_id.hpp"

namespace vm
{


	/**
	 * @brief The Variant represent the container of for different types
	 */
	struct FVariant
	{
		//--------------------------------------------------------------------------
		// Types of the variant
		//--------------------------------------------------------------------------

		struct EType
		{
			static const StringId NIL = SID("nil");
			static const StringId I32 = SID("s32");
			static const StringId F32 = SID("f32");
			static const StringId PTR = SID("cstr");
		};

		//--------------------------------------------------------------------------
		// Properties of the variant class
		//--------------------------------------------------------------------------

		StringId          type;
		union
		{
			PTRINT        as_ptr;
			s32           as_s32;
			f32           as_f32;
		};

		//--------------------------------------------------------------------------
		// Garbage collector helpers
		//--------------------------------------------------------------------------

		// Callback before each SetAs variable
		inline void before_set();

		//--------------------------------------------------------------------------
		// Constructors
		//--------------------------------------------------------------------------


		FVariant() : type(EType::NIL), as_ptr(0) { }
		FVariant(s32 value) : type(EType::I32), as_s32(value) { }
		FVariant(f32 value) : type(EType::F32), as_f32(value) { }
		FVariant(PTRINT value) : type(EType::PTR), as_ptr(value) { }
		FVariant(PTRINT value, StringId type) : type(type), as_ptr(value) { }

		/** copy constructor */
		FVariant(const FVariant& rhs) { *this = rhs; }

		~FVariant();

		//--------------------------------------------------------------------------
		// Access to the type of the variant
		//--------------------------------------------------------------------------

		/**
		 * @brief Get type of the variant
		 */
		StringId get_type() const { return type; }

		std::string get_type_string() const { return lookup_string_safe(type); }

		// Check the type of variant 
		bool is_a(StringId t) const { return type == t; }

		// Bunch of predicates
		bool is_null() const { return is_a(EType::NIL); }
		bool is_i32() const { return is_a(EType::I32); }
		bool is_f32() const { return is_a(EType::F32); }
		bool is_ptr() const { return !(is_null() || is_i32() || is_f32()); }

		//--------------------------------------------------------------------------
		// Setters
		//--------------------------------------------------------------------------

		void set_as_null();
		void set_as_s32(const s32 value);
		void set_as_f32(const f32 value);
		void set_as_ptr(const PTRINT value);
		void set_as_ptr(const PTRINT value, StringId _type);
		void set(const FVariant& other);

		//--------------------------------------------------------------------------
		// Cast to other type
		//--------------------------------------------------------------------------

		s32 cast_to_s32() const;
		f32 cast_to_f32() const;

		// Get the value of the variable as the string
		const char* to_c_string() const;
		std::string to_string() const;

		//--------------------------------------------------------------------------
		// Assignments
		//--------------------------------------------------------------------------

		FVariant& operator=(s32 val) { set_as_s32(val); return (*this); }
		FVariant& operator=(f32 val) { set_as_f32(val); return (*this); }
		FVariant& operator=(PTRINT val) { set_as_ptr(val); return (*this);  }
		FVariant& operator=(const FVariant& other);

		//--------------------------------------------------------------------------
		// Getters
		//--------------------------------------------------------------------------

		s32           get_as_s32() const;
		f32           get_as_f32() const;
		PTRINT        get_as_ptr() const;
		bool          get_as_bool() const;

		//--------------------------------------------------------------------------
		// Math
		//--------------------------------------------------------------------------

		bool operator ==(const FVariant& other) const;
		bool operator !=(const FVariant& other) const;

		//--------------------------------------------------------------------------
		// Errors
		//--------------------------------------------------------------------------

		// return variant error and printout the message
		// if there is active context then this method will 
		// set the process to sleep
		static FVariant print_error(const char* fmt, ...);

	};

	// Use this macro to make the error message variant value
	#define VariantError(_format_, ...) FVariant::PrintError(_format_, ## __VA_ARGS__)

}