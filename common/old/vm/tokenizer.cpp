#include "tokenizer.hpp"


namespace vm::parser {

    /**
     * Helper type for the visitor. I got this line from the
     * microsoft's example page
     */
    template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };

    std::string to_str(const FTokenData& v)
    {
        return std::visit(overloaded{
            [](std::string arg) { return arg; },
            [](double arg) { return std::format("{0}", arg); },
            [](s64 arg) { return std::format("{0}", arg); },
            }, v);
    }

    ECharType FTokenizer::get_char_type(char ch) {
	    // ReSharper disable once CppDefaultCaseNotHandledInSwitchStatement
	    switch (ch) {
        case '(':
            return ECharType::LeftParen;
        case ')':
            return ECharType::RightParen;
        case '"':
            return ECharType::Quote;
        case '\\':
            return ECharType::Escape;
        }
        if (isspace(static_cast<unsigned char>(ch)) || ch == '\n')
            return ECharType::Space;
        return ECharType::Other;
    }

    bool FTokenizer::parse_number(const std::string& str, FToken& tok) {
        bool isFloat = false;
        // Check if this is a numerical value
        // Return with false if this is not
        // the numerical value
        for (u32 i = 0; i < str.size(); i++)
        {
            const auto c = str[i];
            if (c == '.')
            {
                isFloat = true;
                continue;
            }

            if (i != 0 && (c == '-' || c == '+'))
                return false;
            if ((c < '0' || c > '9') && (c != '.'))
                return false; // this is not a number
        }

        try {
            size_t pos = 0;
            if (isFloat) {
                float num = std::stof(str, &pos);
                pos_ += pos;
                if (pos == str.size()) {
                    tok.type = ETokenType::Number;
                    tok.data = num;
                    return true;
                }
            }
            else {
                s32 num = std::stol(str, &pos);
                pos_ += pos;
                if (pos == str.size()) {
                    tok.type = ETokenType::Number;
                    tok.data = num;
                    return true;
                }
            }
        }
        catch (const std::exception&) {
            throw FSyntaxError(tok.location, "parse_number : sytax error : expected number as the String", str);
        }
        return false;
    }
    void FTokenizer::skip_spaces(std::istream& in, FToken& tok)
    {
        char ch;
		while (in.get(ch))
		{
            pos_++;
            if (isspace(static_cast<unsigned char>(ch)))
                continue;
            if (ch == '\n') {
                line_++;
                pos_ = 0;
            }
		}
    }

    bool FTokenizer::get_token(std::istream& in, FToken& tok) {
        char ch;
        EParseState state = EParseState::Init;
        bool escape = false;
        std::string str;
        ETokenType type = ETokenType::None;
        bool starttok = true;
        while (get_char(in, ch)) {

            if (starttok) {
                if (ch == ' ' || ch == '\n') {
                    continue;
                } else {
		            tok.location.line = safe_cast_u_int32(line_);
		            tok.location.pos = safe_cast_u_int32(pos_);
		            starttok = false;
	            }
			}
            ECharType ctype = get_char_type(ch);
            if (escape) {
                ctype = ECharType::Other;
                escape = false;
            }
            else if (ctype == ECharType::Escape) {
                escape = true;
                continue;
            }
            if (state == EParseState::Quote) {
                if (ctype == ECharType::Quote) {
                    type = ETokenType::String;
                    break;
                }
                else
                    str += ch;
            }
            else if (state == EParseState::Symbol) {
                if (ctype == ECharType::Space)
                    break;
                if (ctype != ECharType::Other) {
                    in.putback(ch);
                    break;
                }
                str += ch;
            }
            else if (ctype == ECharType::Quote) {
                state = EParseState::Quote;
            }
            else if (ctype == ECharType::Other) {
                state = EParseState::Symbol;
                type = ETokenType::Symbol;
                str = ch;
            }
            else if (ctype == ECharType::LeftParen) {
                type = ETokenType::LeftParen;
                break;
            }
            else if (ctype == ECharType::RightParen) {
                type = ETokenType::RightParen;
                break;
            }
        } 
        if (type == ETokenType::None) {
            if (state == EParseState::Quote)
                throw FSyntaxError(tok.location, "get_token : syntax error : missing Quote", str);
            return false;
        }
        tok.type = type;
        if (type == ETokenType::String)
            tok.data = str;
        else if (type == ETokenType::Symbol) {
            if (!parse_number(str, tok))
                tok.data = str;
        }
        //printf("%s %d\n", tok.Location.ToStr().c_str(), tok.type);
        return true;
    }
}