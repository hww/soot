#include <iostream>
#include <filesystem>
#include <iostream>
#include <fstream>

#include "vm/vm_include.hpp"

using namespace vm;

inline bool check_file_exists(const std::string& name) {
	struct stat buffer{};
	return (stat(name.c_str(), &buffer) == 0);
}

int main(int argc, char** argv)
{
    std::cout << "vm Compiler\n";
	if (argv[1] == nullptr)
	{
		std::cerr << "Expected filename" << std::endl;
		return -1;
	}
	std::filesystem::path filePath(argv[1]);

	
	try
	{
		// Verify the existence of source file
		if (!check_file_exists(filePath.generic_string())) {
			std::cerr << "File '" << filePath << "' is not found" << std::endl;
			return -1;
		}
		// Read and parse the source file
		std::ifstream inStream(filePath);
		auto data = vm::assembler::Compile(inStream);

		
		// Make the output file name
		std::filesystem::path path = filePath.parent_path();
		std::filesystem::path name = filePath.filename();
		name = name.replace_extension("bin");
		std::filesystem::path outPath = path / name;
		// write the file 
		std::ofstream os(outPath);
		data->save_file(os);
	}
	catch (const FException& ex)
	{
		std::cerr << filePath << " : " << ex.what() << std::endl;
	}
	return 0;
}