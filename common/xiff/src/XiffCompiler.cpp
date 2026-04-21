#include <fstream>
#include <fmt/core.h>
#include "fmt/format.h"
#include "fmt/color.h"
#include "xiff/XiffCompiler.hpp"
#include "common/util/FileUtil.hpp"
#include "common/sooti/Export.hpp"

XiffCompiler::XiffCompiler(script::Interpreter& shared_interp) : m_interp(shared_interp) {}

bool XiffCompiler::load_library(const std::string& lib_name) {
    // Используем твой новый FileHub для поиска файла
    auto path = file_util::find_config_file(lib_name);
    if (path.empty()) {
        fmt::print(stderr, "XIFF Error: Library not found: {}\n", lib_name);
        return false;
    }
    
    m_interp.eval_string(path.string(), path.string());
    return true;
}

void XiffCompiler::scan_file(const fs::path& asm_path) {
    if (!file_util::exists(asm_path)) return;

    std::ifstream file(asm_path);
    std::string line;
    std::string soot_buffer;

    auto expression = fmt::format("(xiff-define-file \"{}\")\n", asm_path.string());
    auto result = m_interp.eval_string(expression, "xiff");

    while (std::getline(file, line)) {
        size_t pos = line.find("; xiff");
        if (pos == std::string::npos) pos = line.find("; soot");

        if (pos != std::string::npos) {
            soot_buffer += line.substr(pos + 6) + " ";
        } else if (!soot_buffer.empty()) {
            m_interp.eval_string(soot_buffer, asm_path.string());
            soot_buffer.clear();
        }
    }
    m_scanned_files.push_back(asm_path);
}

bool XiffCompiler::finalize_and_inject() {

    for (const auto& f : m_scanned_files) {   
        std::string target_file;

        auto get_targets_exp = fmt::format("(xiff-get-targets \"{}\")\n", f.string());
        auto get_targets_res = m_interp.eval_string(get_targets_exp, "finalize_and_inject");
        
        if (!get_targets_res.is_array()) {
            fmt::print(stderr, "[ERR] [XiffCompiler] finalize_and_inject : Lisp generator did not return a array for all targets.\n");
            return false;
        }
        
        auto targets = get_targets_res.as_array();

        for (int i=0;i<targets->size();i++) {
            auto target = targets->get(i);
            if (!target.is_array()) {
                fmt::print(stderr, "[ERR] [XiffCompiler] finalize_and_inject : Lisp generator did not return a array for target.\n");
                return false;
            }          

            auto target_array = target.as_array();
            if (target_array->size() < 2) {
                fmt::print(stderr, "[ERR] [XiffCompiler] finalize_and_inject : Expected the target with type and path.\n");
                return false;                  
            }
            
            auto type = target_array->get(0);
            auto path = target_array->get(0);
            if (!type.is_string() || !path.is_string()) {
                fmt::print(stderr, "[ERR] [XiffCompiler] finalize_and_inject : Expected strings in target's type and path path.\n");
                return false;                  
            }

            auto type_str = type.as_string();
            auto path_str = path.as_string();
            if (type_str->length()==0 || path_str->length() == 0) {
                fmt::print(stderr, "[ERR] [XiffCompiler] finalize_and_inject : Expected non blank strings in target's type and path path.\n");
                return false;                  
            }

            // Вызываем Lisp функцию, которую мы определили в xiff-core.sot
            // Предположим, она называется (xiff-generate-content) и возвращает строку
            auto generate_exp = fmt::format("(xiff-generate \"{}\" \"{}\")\n", type_str->data, path_str->data);
            auto generate_res = m_interp.eval_string(generate_exp, "finalize_and_inject");
            
            if (!generate_res.is_string()) {
                fmt::print(stderr, "[ERR] [XiffCompiler] xiff-generate did not return a string.\n");
                return false;
            }

            auto res = m_injector.inject(path_str->data, "xiff", generate_res.as_string()->data, f.string());
            if (!res.success) {
                fmt::print(stderr, "[ERR] [XiffCompiler] xiff-generate injection is not sucessfull.\n");
                return false;
            }
        }
    }
    return true;
}


void XiffCompiler::eval_string(const std::string& line, const std::string& file) {
    // Выполнение Lisp кода (только если не built-in команда)
    auto source_file = (file.empty()) ? "xiff" : file;

    try {
        auto result = m_interp.eval_string(line, source_file);
        fmt::print(fg(fmt::color::green), "=> {}\n", result.print());
    }
    catch (const std::exception& e) {
        fmt::print(fg(fmt::color::red), "Error: {}\n", e.what());
    }
}
