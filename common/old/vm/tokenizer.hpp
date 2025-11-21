#pragma once
#include <iostream>
#include <memory>
#include <string>
#include <variant>
#include "errors.hpp"

namespace vm::parser
{
	//--------------------------------------------------------------------------
	// Types of the FTokenizer
	//--------------------------------------------------------------------------

	enum class ETokenType { None, LeftParen, RightParen, Symbol, String, Number };

	enum class ECharType { LeftParen, RightParen, Quote, Escape, Space, Other };

	enum class EParseState { Init, Quote, Symbol };

	/** @brief Token contains one of the listed types */
	using FTokenData = std::variant<std::string, double, s64>;

	//--------------------------------------------------------------------------
	// Types of the variant
	//--------------------------------------------------------------------------

	std::string to_str(const FTokenData& v);

	//--------------------------------------------------------------------------
	// Single token
	//--------------------------------------------------------------------------

	struct FToken
	{
		ETokenType type = ETokenType::None;
		Location location;
		FTokenData data;
		std::string to_string() const { return to_str(data); }
	};

	//--------------------------------------------------------------------------
	// The FTokenizer class
	//--------------------------------------------------------------------------

	class FTokenizer
	{
	public:
		/**
		 * Construct the FTokenizer with value stream
		 */
		FTokenizer(std::istream& in) : in_(in), line_(1), pos_(0)
		{
		}

		bool next()
		{
			if (put_back_)
			{
				put_back_ = false;
				return true;
			}
			return get_token(in_, current_);
		}

		/** Get current token */
		const FToken& current() const
		{
			return current_;
		}

		void put_back()
		{
			put_back_ = true;
		}

		ECharType get_char_type(char ch);
		bool parse_number(const std::string& str, FToken& tok);
		bool get_token(std::istream& in, FToken& tok);
		void skip_spaces(std::istream& in, FToken& tok);

	private:
		bool get_char(std::istream& in, char& ch)
		{
			in.get(ch);
			pos_++;
			if (ch == '\n')
			{
				line_++;
				pos_ = 0;
			}
			return (in) ? true : false;
		}

		/** The stream to read **/
		std::istream& in_;
		/** */
		bool put_back_ = false;
		/** Current FToken */
		FToken current_;
		/** The line and char number in the file */
		size_t line_;
		size_t pos_;
	};
}
