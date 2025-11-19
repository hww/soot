// FileHub.h - ÷èñòûå îáúÿâëåíèÿ
#pragma once

#include <filesystem>
#include <vector>
#include <string>
#include <cstdint>
#include <regex>

namespace file_util {

	namespace fs = std::filesystem;

	// ==================== ÎÑÍÎÂÍÛÅ ÓÒÈËÈÒÛ ====================
	std::string read_text(const fs::path& path);
	void write_text(const fs::path& path, const std::string& content);
	std::vector<uint8_t> read_binary(const fs::path& path);
	void write_binary(const fs::path& path, const std::vector<uint8_t>& data);

	// ==================== ĞÀÁÎÒÀ Ñ ÏÓÒßÌÈ ====================
	std::string get_filename(const fs::path& path);
	std::string get_stem(const fs::path& path);
	std::string get_extension(const fs::path& path);
	fs::path join_paths(const fs::path& a, const fs::path& b);
	//fs::path make_path(std::string path) { return fs::path(path); }

	// ==================== ĞÀÁÎÒÀ Ñ ÄÈĞÅÊÒÎĞÈßÌÈ ====================
	bool create_dirs(const fs::path& path);
	bool create_dirs_for_file(const fs::path& file_path);
	std::vector<fs::path> list_files(const fs::path& dir);
	std::vector<fs::path> list_dirs(const fs::path& dir);

	// ==================== ÏÎÈÑÊ ÔÀÉËÎÂ ====================
	std::vector<fs::path> find_by_extension(const fs::path& dir, const std::string& ext);
	std::vector<fs::path> find_by_extension_recursive(const fs::path& dir, const std::string& ext);
	std::vector<fs::path> find_by_pattern(const fs::path& dir, const std::regex& pattern);

	// ==================== ÏĞÎÂÅĞÊÈ È ÈÍÔÎĞÌÀÖÈß ====================
	// Ïğîñòûå îá¸ğòêè - ìîæíî inline
	inline bool exists(const fs::path& path) { return fs::exists(path); }
	inline bool is_file(const fs::path& path) { return fs::is_regular_file(path); }
	inline bool is_dir(const fs::path& path) { return fs::is_directory(path); }

	uintmax_t get_size(const fs::path& path);

	// ==================== ÎÏÅĞÀÖÈÈ Ñ ÔÀÉËÀÌÈ ====================
	void copy_file(const fs::path& from, const fs::path& to);
	void move_file(const fs::path& from, const fs::path& to);
	bool remove_file(const fs::path& path);
	uintmax_t remove_dir(const fs::path& path);

	// ========== ÎÏÅĞÀÖÈÈ Ñ ÔÀÉËÀÌÈ ÄËß İÒÎÃÎ ÏĞÎÅÊÒÀ ============
	fs::path get_project_dir();
	std::string get_file_path(const std::vector<std::string>& input);
} // namespace filehub