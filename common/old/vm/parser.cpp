
#include "parser.hpp"
#include <iostream>
#include <sstream>

#include "tokenizer.hpp"

namespace vm::parser {

	std::shared_ptr<FSyntax> parse(FTokenizer&);

	static void indent(std::ostream& out, const int level) {
        for (int i = 0; i < level; ++i)
            out << "   ";
    }

	//--------------------------------------------------------------------------
	// Syntax
	//--------------------------------------------------------------------------

    void FSyntax::write(std::ostream& out) const
    {
        out << to_string();
    }

    void FSyntax::write_indented(std::ostream& out, int level) const
    {
        indent(out, level);
        write(out);
    }

    //--------------------------------------------------------------------------
	// List
	//--------------------------------------------------------------------------

	std::list<std::shared_ptr<FSyntax>>::const_iterator FListStx::begin() const noexcept
	{
		return list.begin();
	}

	std::list<std::shared_ptr<FSyntax>>::const_iterator FListStx::end() const noexcept
	{
		return list.end();
	}

	std::shared_ptr<FSyntax> FListStx::front() const noexcept
	{
		return list.front();
	}

	std::shared_ptr<FSyntax> FListStx::get_item(const size_t idx) const
	{
		auto it = list.begin();
		for (u32 i = 0; i < idx; i++) {
			++it;
		}
		return *it;
	}

    std::string FListStx::to_string() const {
        std::string str{};
    	str += "(";
        if (!list.empty()) {
            auto i = list.begin();
            str += (*i)->to_string();
            while (++i != list.end()) {
            	str += ' ';
                str += (*i)->to_string();
            }
        }
        str += ")";
        return str;
    }

    void FListStx::write(std::ostream& out) const {
        out << "(";
        if (!list.empty()) {
            auto i = list.begin();
            (*i)->write(out);
            while (++i != list.end()) {
                out << ' ';
                (*i)->write(out);
            }
        }
        out << ")";
    }

    void FListStx::write_indented(std::ostream& out, int level) const {
        indent(out, level);
        out << "(\n";
        if (!list.empty()) {
            for (auto i = list.begin(); i != list.end(); ++i) {
                (*i)->write_indented(out, level + 1);
                out << '\n';
            }
        }
        indent(out, level);
        out << ")";
    }

    void FListStx::append(const std::shared_ptr<FSyntax>& ptr)
    {
        list.push_back(ptr);
    }

    //--------------------------------------------------------------------------
    // value
    //--------------------------------------------------------------------------

    float FNumberStx::get_float() const
	{
        if (is_inexact)
            return value.as_float;
         throw FSyntaxError(loc, "Expected floating point number", to_string());
	}

    s32 FNumberStx::get_int() const
    {
		if (is_inexact)
	        throw FSyntaxError(loc, "Expected s32 number", to_string());
        return value.as_int32;
    }

    std::string FNumberStx::to_string() const
    { 
	    if (is_inexact)
		    return std::format("{0}", value.as_float);
	    else
		    return std::format("{0}", value.as_uint32);
    }

    void FNumberStx::write(std::ostream& out) const
    {
	    if (is_inexact)
		    out << value.as_float;
	    else
		    out << value.as_uint32;
    }


    //--------------------------------------------------------------------------
	// Symbol
	//--------------------------------------------------------------------------

    std::string FSymbolStx::to_string() const
    { return std::format("{0}", value); }

    void FSymbolStx::write(std::ostream& out) const
    {
	    for (const char ch : value) {
		    out << ch;
	    }
    }

    //--------------------------------------------------------------------------
	// Parser
	//--------------------------------------------------------------------------

    std::shared_ptr<FListStx> parse_list(FTokenizer& tok) {
        std::shared_ptr<FListStx> lst = std::make_shared<FListStx>();
        lst->loc = tok.current().location;
        while (tok.next()) {
            if (tok.current().type == ETokenType::RightParen)
                return lst;
            else
                tok.put_back();
            lst->append(parse(tok));
        }
        throw FSyntaxError(lst->loc, "parse_list", "syntax error: unclosed list", lst->to_string());
    }

    std::shared_ptr<FSyntax> parse(FTokenizer& tokenizer) {
        if (!tokenizer.next())
            return std::make_shared<FListStx>();
        std::shared_ptr<FSyntax> obj;
        const FToken& tok = tokenizer.current();
        const Location loc = tok.location;
        switch (tok.type) {
        case ETokenType::String:
            obj = std::make_shared<FStringStx>(std::get<std::string>(tok.data));
            break;
        case ETokenType::Symbol:
            obj = std::make_shared<FSymbolStx>(std::get<std::string>(tok.data));
            break;
        case ETokenType::Number:
            if (std::holds_alternative<double>(tok.data))
                obj = std::make_shared<FNumberStx>(std::get<double>(tok.data));
            else
                obj = std::make_shared<FNumberStx>(std::get<s64>(tok.data));
            break;
        case ETokenType::LeftParen:
            obj = parse_list(tokenizer);
            break;
        default:
            throw FSyntaxError(loc, "parse_number : syntax_error : unexpected FToken", tok.to_string());
        }
        obj->loc = loc;
        return obj;
    }

    std::shared_ptr<FSyntax> parse_stream(std::istream& in) {
        FTokenizer tokenizer(in);
        return parse(tokenizer);
    }

    std::shared_ptr<FSyntax> parse_string(const std::string& str) {
        std::istringstream in(str);
        return parse_stream(in);
    }

    void print_syntax(const std::shared_ptr<FSyntax> stx) {
        if (stx != nullptr) {
            stx->write_indented(std::cout, 0);
            std::cout << '\n';
        }
    }

} // namespace s_expr
