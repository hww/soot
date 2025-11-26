#include <iostream>
#include <filesystem>
#include <iostream>
#include <fstream>
#include <regex>

#include "common/util/args.hpp"
#include "../vm/vm_include.hpp"

using namespace std;


constexpr char PROJECT_MANE[] = "The Game Scripting VM";
constexpr char TOOL_NAME[] = "sider";
constexpr char TOOL_VERSION[] = "v1.0a";

const char* g_out_file_name = nullptr;
int g_verbose_mode = 0;

inline bool check_file_exists(const std::string& name) {
  struct stat buffer{};
  return (stat(name.c_str(), &buffer) == 0);
}

bool parse_file(const char* file) {
  const std::filesystem::path filePath(file);

  // Verify the existence of source file
  if (!check_file_exists(filePath.generic_string())) {
    std::cerr << "File '" << filePath << "' is not found" << std::endl;
    return false;
  }
  // Read and parse the source file
  if (g_verbose_mode)
    std::cout << "Reading file '" << filePath << "'..." << std::endl;
  std::ifstream inStream(filePath);
  std::stringstream buffer;
  buffer << inStream.rdbuf();
  auto str = buffer.str();
  // Search all string ids
  std::regex r(R"foo(SID[[:space:]]*\([[:space:]]*"([a-zA-Z0-9 ]+)"[[:space:]]*\))foo",
               std::regex::extended);
  std::smatch sm{};
  string::const_iterator searchStart(str.cbegin());
  while (std::regex_search(searchStart, str.cend(), sm, r)) {
    auto str = sm[1].str();
    if (g_verbose_mode)
      std::cout << "Reading file '" << filePath << "'..." << std::endl;
    vm::define_string(str);
    searchStart = sm.suffix().first;
  }
  inStream.close();
  return true;
}

int main(int argc, const char** argv) {

  if (argc == 1) {
    std::cout << TOOL_NAME << " missing operand\n";
    std::cout << "Try '" << TOOL_NAME << " --help' for more information.";
    return 0;
  }

  FArgs arguments{};
  arguments.init(argc, argv);
  vector<string> file_list{};
  const auto now = std::chrono::system_clock::now();
  // Parse arguments 
  for (auto i = 1; i < arguments.get_size(); i++) {
    if (arguments.is_option(i)) {
      if (arguments.check(i, "-I") || arguments.check(i, "--include")) {

      } else if (arguments.check(i, "-o") || arguments.check(i, "--output")) {
        // The option: -o 
        i++;
        if (g_out_file_name != nullptr) {
          std::cerr << "Do not use more than one -o option";
          return -1;
        }
        g_out_file_name = arguments[i];
      } else if (arguments.check(i, "-i") || arguments.check(i, "--input")) {
        // The option: -i 
        i++;
        if (check_file_exists(arguments[i])) {
          vm::load_strings_file(arguments[i]);
        } else {
          std::cerr << "The input file '" << arguments[i] << "' is not exist\n";
          return -1;
        }
      } else if (arguments.check(i, "-v") || arguments.check(i, "--g_verbose_mode")) {
        // The option: -v 
        g_verbose_mode = 1;
      } else if (arguments.check(i, "--TOOL_VERSION")) {
        // The option: --TOOL_VERSION 
        std::cout << TOOL_NAME << " (" << PROJECT_MANE << ") " << TOOL_VERSION << std::endl;
        std::cout << "Copyright© " << std::format("{:%Y}", now) <<
            " Free Software Foundation, Inc\n";
        std::cout <<
            "License GPLv3 + : GNU GPL version 3 or later < https ://gnu.org/licenses/gpl.html>\n";
        std::cout << "This is free software : you are free to change and redistribute it.\n";
        std::cout << "There is NO WARRANTY, to the extent permitted by law.\n";
        std::cout << std::endl;
        std::cout << "Written by Written by Valeriya Pudova\n";
      } else if (arguments.check(i, "--help")) {
        // The option: --help
        std::cout << "Usage: " << TOOL_NAME << " FILE1 FILE2\n";
        std::cout << "or:    " << TOOL_NAME << " OPTION\n";
        std::cout << "--help          Display this help and exit\n";
        std::cout << "--version       Output version information and exit\n";
        std::cout << "\nOptions:\n";
        std::cout << "-i, --input     The source database\n";
        std::cout << "-o, --output    Output database\n";
        std::cout << "-o -            The output file " -
            " will force to write the database on the screen\n";
        std::cout << R"(Examples:)";
        std::cout << TOOL_NAME <<
            "-i FILE1 -o FILE2 FILE3 FILE4\n  Create the database FILE2 from the database FILE1\n";
        std::cout << "                                 and the (.h,.cpp) files FILE3 and FILE4\n";
        std::cout << TOOL_NAME <<
            "-i FILE1 -i FILE2 -o FILE3\n     Create the database FILE3 from the database FILE1 and FILE2\n";
      }
    } else {
      // There was no option, then it is C++ source file name
      // parse it an update the database
      file_list.emplace_back(arguments[i]);
    }
  }
  // The parsing of C++ files
  for (auto& i : file_list) {
    if (!parse_file(i.c_str())) {
      std::cerr << "Unexpected error on import file '" << i << "'\n";
      return -1;
    }
  }
  // Write database file
  if (g_out_file_name != nullptr) {
    if (0 == strcmp(g_out_file_name, "-"))
      vm::print_strings();
    else
      vm::save_strings_file(g_out_file_name);
  } else {
    vm::print_strings();
  }
}
