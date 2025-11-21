#pragma once

#include <iomanip>
#include <iostream>
#include <list>
#include <memory>
#include <string>
#include <format>

#include "string_id.hpp"
#include "errors.hpp"
#include "crc32.hpp"

namespace vm::parser {


    void indent(std::ostream& out, int level);

    struct FStringStx;
    struct FSymbolStx;
    struct FNumberStx;
    struct FListStx;

    /** The base class for all parsed objects */
    struct FSyntax {
        virtual ~FSyntax() = default;
        /**/
        virtual bool is_string() const { return false; }
        virtual bool is_symbol() const { return false; }
        virtual bool is_number() const { return false; }
        virtual bool is_float() const { return false; }
        virtual bool is_integer() const { return false; }
        virtual bool is_list() const { return false; }
        /**/
        virtual std::shared_ptr<FStringStx> get_string_stx() { return nullptr; }
        virtual std::shared_ptr<FSymbolStx> get_symbol_stx() { return nullptr; }
        virtual std::shared_ptr<FNumberStx> get_number_stx() { return nullptr; }
        virtual std::shared_ptr<FListStx> get_list_stx() { return nullptr; }
        /**/
        virtual std::string to_string() const { return "object"; }
        virtual void write(std::ostream&) const;
        virtual void write_indented(std::ostream& out, int level) const;
        /** The object's Location in the file */
        Location loc;
    };

    /** The string object */
    struct FStringStx : FSyntax {
        /**/
        explicit FStringStx(const std::string& str) : value(str) {}
    	/**/
    	bool is_string() const override { return true; }
        /**/
        std::shared_ptr <FStringStx> get_string_stx() override { return std::make_shared<FStringStx>(*this); }
        /**/
        std::string to_string() const override { return std::format("\"{0}\"", value); }
        void write(std::ostream& out) const override { out << std::quoted(value); }
        /**/
        std::string get_string() const { return value; }
        /**/
    private:
        std::string value;
    };

    /** The Symbol object */
    struct FSymbolStx : FSyntax {
        /**/
    	explicit FSymbolStx(const std::string& str) : value(str) {}
        /**/
        bool is_symbol() const override { return true; }
        /**/
        std::shared_ptr <FSymbolStx> get_symbol_stx() override { return std::make_shared<FSymbolStx>(*this); }
        /**/
        std::string to_string() const override;
        void write(std::ostream& out) const override;
        /**/
    	std::string get_string() const { return value; }
        StringId get_string_id() const { return crc32(value); }
        /**/
    private:
        std::string value;
    };

    /** The number object */
    struct FNumberStx : FSyntax {
        /**/
        explicit FNumberStx(double num) : is_inexact(true) { value.as_float = safe_cast_to_float(num); }
        explicit FNumberStx(s64 num) : is_inexact(false) { value.as_int32 = safe_cast_to_int32(num); }
    	/**/
        bool is_number() const override { return true; }
        bool is_float() const override { return is_inexact; }
        bool is_integer() const override { return !is_inexact; }
        /**/
        std::shared_ptr <FNumberStx> get_number_stx() override { return std::make_shared<FNumberStx>(*this); }
        /**/
        std::string to_string() const override;
        void write(std::ostream& out) const override;
        /**/
        float get_float() const;
        s32 get_int() const;
        /**/
    private:
        U32float value;
        /**
         * @brief Has true value for the floating point values
         */
        bool is_inexact;
    };

    /** The scheme list */
    struct FListStx :  FSyntax {
        /**/
        FListStx() = default;
        /**/
        bool is_list() const override { return true; }
        /**/
        std::shared_ptr<FListStx>
    	get_list_stx() override { return std::make_shared <FListStx>( *this); }
        /**/
    	std::string to_string() const override;
        void write(std::ostream& out) const override;
        void write_indented(std::ostream&, int) const override;
    	/**/
        void append(const std::shared_ptr<FSyntax>& ptr);
        std::list<std::shared_ptr<FSyntax>>::const_iterator begin() const noexcept;
        std::list<std::shared_ptr<FSyntax>>::const_iterator end() const noexcept;
        std::shared_ptr<FSyntax> front() const noexcept;
        /**/
        std::list<std::shared_ptr<FSyntax>>
		get_list() const { return list; }
        std::shared_ptr<FSyntax> get_item(size_t idx) const;
        size_t size() const { return list.size(); }
        /**/
    private:
        std::list<std::shared_ptr<FSyntax>> list;
    };


    /**
     * @brief Parse the stream to the AST structure
     * @param in - The input stream
     * @return - The syntax object
     * @addindex The example using below
     *
     *     //  int main(int argc, char** argv) {
     *     std::string test_string =
     *         "((data \"quoted data\" 123 4.5)\n"
     *         " (data (!@# (4.5) \"(more\" \"data)\")))";
     *     if (argc == 2)
     *         test_string = argv[1];
     *     try {
     *         parse_string(test_string);
     *     }
     *     catch (const std::exception& ex) {
     *         std::cerr << ex.what() << '\n';
     *     }
     */
    std::shared_ptr<FSyntax> parse_stream(std::istream& in);

    /**
     * @brief Parse the string to the AST structure
     * @param str - The string with expressions
     * @return - The syntax object
     */
    std::shared_ptr<FSyntax> parse_string(const std::string& str);

    /**
     * @brief Print the AST object on screen 
     * @param stx - The syntax object
     */
    void print_syntax(std::shared_ptr<FSyntax> stx);

}