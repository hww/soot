#include "repl_wrapper.h"

#include "CommonTypes.hpp"
#include "util/FileUtil.hpp"
#include "util/ProjectConfig.hpp"
#include "util/JsonUtil.hpp"
#include "util/StringUtil.hpp"
#include "versions/version.h"

#include "fmt/format.h"
#include "fmt/color.h"
#include "fmt/core.h"
#include "third_party/replxx/include/replxx.hxx"
#include "versions/revision.h"

#ifndef FMT_CONSTEVAL
#define FMT_CONSTEVAL
#endif
#include <util/Log.hpp>

using namespace file_util;

namespace REPL {
void Wrapper::clear_screen() {
  repl.clear_screen();
}
static constexpr std::string_view logo[] = {
  "       ___       ",
  "      /  /\\      ",
  "     /  /:/_     ",
  "    /  /:/ /\\    ",
  "   /  /:/ /::\\   ",
  "  /__/:/ /:/\\:\\  ",
  "  \\  \\:\\/:/~/:/  ",
  "   \\  \\::/ /:/   ",
  "    \\__\\/ /:/    ",
  "      /__/:/     ",
  "      \\__\\/      ",
  "                 "
};

void Wrapper::print_welcome_message(const WelcomeInfo              &info,
                                    const std::vector<std::string> &loaded_projects) {
    std::string message;
    auto        orange = fg(fmt::color::orange) | fmt::emphasis::bold;
    auto        cyan = fg(fmt::color::cyan);
    auto        gray = fg(fmt::color::gray);

    auto add_line = [&](int logo_idx, const std::string &text) {
        message += fmt::format(orange, fmt::runtime(logo[logo_idx]));
        message += text + "\n";
    };

    add_line(0, "");
    add_line(1, "");

    std::string welcome = fmt::format(fmt::emphasis::bold, "  Welcome to {} {}.{} [{}]", info.title,
                                      versions::SOOT_VERSION_MAJOR, versions::SOOT_VERSION_MINOR,
                                      fmt::format(gray, "{}", BUILT_SHA));
    if (!username.empty() && username != "#f" && username != "unknown") {
        welcome += fmt::format(fg(fmt::color::light_green), " {}", username);
    }
    add_line(2, welcome + "!");

    if (!info.subtitle.empty()) { add_line(3, fmt::format("  {}", info.subtitle)); }

    add_line(4, fmt::format(
                    "  Project Path: {}",
                    fmt::format(gray, "{}", common::get_path(common::PathType::PROJECT).string())));

    std::string nrepl = "  nREPL: ";
    nrepl += (!nrepl_alive) ? fmt::format(fg(fmt::color::red), "DISABLED")
                            : fmt::format(fg(fmt::color::light_green), "Listening on {}",
                                          repl_config.get_nrepl_port());
    add_line(5, nrepl);

    if (!loaded_projects.empty()) {
        add_line(6, fmt::format("  Projects: {}",
                                fmt::format(gray, "{}", str_util::join(loaded_projects, ","))));
    } else {
        add_line(6, "");
    }

    if (!repl_config.asm_file_search_dirs.empty()) {
        add_line(7,
                 fmt::format("  Source File Search Dirs: [{}]",
                             fmt::format(gray, "{}",
                                         str_util::join(repl_config.asm_file_search_dirs, ","))));
    }

    int line = 8;
    for (const auto &[cmd, desc] : info.hints) {
        add_line(line++, fmt::format("    {} {}", fmt::format(cyan, "{}", cmd), desc));
    }
    add_line(line, "");

    fmt::print("{}", message);
}

void Wrapper::print_to_repl(const std::string& str) {
  repl.print(str.data());
}

void Wrapper::set_history_max_size(size_t len) {
  repl.set_max_history_size(len);
}

const char* Wrapper::readline(const std::string& prompt) {
  return repl.input(prompt);
}

void Wrapper::add_to_history(const std::string& line) {
  repl.history_add(line);
}

void Wrapper::save_history() {
  fs::path path;
  if (repl_config.per_game_history) {
      path = common::get_path(common::PathType::CACHE) /
             soot_platform_to_game_name(repl_config.game_version) /
           ".opengoal.repl.history";
  } else {
      path = common::get_path(common::PathType::CONFIG) / ".sot.repl.history";
  }
  file_util::create_dirs_for_file(path.string());
  repl.history_save(path.string());
}

void Wrapper::load_history() {
    fs::path path;
    if (repl_config.per_game_history) {
        path = common::get_path(common::PathType::CACHE) /
               soot_platform_to_game_name(repl_config.game_version) / ".opengoal.repl.history";
    } else {
        path = common::get_path(common::PathType::CONFIG) / ".sot.repl.history";
    }

    // Создать директорию, если её нет
    file_util::create_dirs_for_file(path.string());

    if (fs::exists(path)) {
        repl.history_load(path.string());
        lg::debug("Loaded REPL history from {}", path.string());
    } else {
        // Файла нет. Это нормально при первом запуске.
        // Логируем в файл (debug), чтобы не шуметь в консоли.
        lg::debug("No REPL history at {} (first run?)", path.string());
    }
}

std::pair<std::string, bool> Wrapper::get_current_repl_token(std::string const& context) {
  // Find the current token
  std::string token = "";
  for (auto c = context.crbegin(); c != context.crend(); c++) {
    if (std::isspace(*c)) {
      break;
    } else {
      token = *c + token;
    }
  }

  // If there is a preceeding '(' remove it
  if (!token.empty() && token.at(0) == '(') {
    token.erase(0, 1);
    return {token, true};
  }
  return {token, false};
}

void Wrapper::print_keybind_help() {
  fmt::print(fmt::emphasis::bold, "\nREPL KeyBinds:\n");
  for (const auto& bind : repl_config.keybinds) {
    fmt::print("{}\n", fmt::styled(bind.string(), fmt::fg(fmt::color::cyan)));
    fmt::print("{}\n", fmt::styled(bind.description, fmt::fg(fmt::color::gray)));
  }
}

replxx::Replxx::key_press_handler_t Wrapper::commit_text_action(std::string text_to_commit) {
  return [this, text_to_commit](char32_t code) {
    repl.set_state(
        replxx::Replxx::State(text_to_commit.c_str(), static_cast<int>(text_to_commit.size())));
    return repl.invoke(replxx::Replxx::ACTION::COMMIT_LINE, code);
  };
}

void Wrapper::init_settings() {
  // NOTE - a nice popular project that uses replxx
  // - https://github.com/ClickHouse/ClickHouse/blob/master/base/base/ReplxxLineReader.cpp#L366
  repl.set_word_break_characters(" \t");
  repl.set_complete_on_empty(false);
  repl.set_indent_multiline(true);
  repl.enable_bracketed_paste();
  // Setup default keybinds
  for (const auto& bind : repl_config.keybinds) {
    char32_t code;
    switch (bind.modifier) {
      case KeyBind::Modifier::CTRL:
        code = replxx::Replxx::KEY::control(bind.key.at(0));
        break;
      case KeyBind::Modifier::SHIFT:
        code = replxx::Replxx::KEY::shift(bind.key.at(0));
        break;
      case KeyBind::Modifier::META:
        code = replxx::Replxx::KEY::meta(bind.key.at(0));
        break;
    }
    repl.bind_key(code, commit_text_action(bind.command));
  }
}

void Wrapper::reload_startup_file() {
  startup_file = load_user_startup_file(username, repl_config.game_version);
}

std::string find_repl_username() {
  // Two options - either:
  // 1. look for the `user.txt` file, which should only contain the username
  // 2. if this is absent AND there is a single folder inside the "user" folder, use that as the
  // username
    auto user_dir = common::get_path(common::PathType::PROJECT) / "user";
  auto dirs = file_util::list_dirs(user_dir.string());
  if (dirs.size() == 1) {
    return dirs.at(0).filename().string();
  }

  std::regex allowed_chars("(^[0-9a-zA-Z\\-\\.\\!\\?<>]*$)");
  if (file_util::exists((user_dir / "user.txt").string())) {
    auto text = file_util::read_text(user_dir / "user.txt");
    text = str_util::trim(text);
    if (!text.empty() && std::regex_match(text, allowed_chars)) {
      return text;
    }
  }

  return "unknown";
}

fs::path get_startup_file_path(const std::string& username, const SootPlatform game_version) {
  (void)username;
  // Сначала проверяем специфичный для версии файл в CONFIG
  auto game_specific_path = common::get_path(common::PathType::CONFIG) / 
                            fmt::format("startup-{}.sot", soot_platform_to_game_name(game_version));
  if (file_util::exists(game_specific_path)) {
    return game_specific_path;
  }
  
  // Затем проверяем общий startup файл в CONFIG
  auto config_path = common::get_path(common::PathType::CONFIG) / "startup.sot";
  if (file_util::exists(config_path)) {
    return config_path;
  }
  
  // Затем проверяем в PROJECT (если есть проект-специфичный)
  auto project_path = common::get_path(common::PathType::PROJECT) / "startup.sot";
  if (file_util::exists(project_path)) {
    return project_path;
  }
  
  // Иначе возвращаем путь по умолчанию в CONFIG (файла может не быть)
  return config_path;
}

StartupFile load_user_startup_file(const std::string& username, const SootPlatform game_version) {
  // Check for a `startup.gc` file, each line will be executed on the REPL on startup
  auto startup_file_path = get_startup_file_path(username, game_version);
  StartupFile startup_file;
  if (file_util::exists(startup_file_path.string())) {
    auto data = file_util::read_text(startup_file_path);
    auto startup_cmds = str_util::split(data);
    bool found_run_on_listen_line = false;
    for (const auto& cmd : startup_cmds) {
      if (found_run_on_listen_line) {
        startup_file.run_after_listen.push_back(cmd);
      } else {
        startup_file.run_before_listen.push_back(cmd);
      }
      if (str_util::contains(cmd, "og:run-below-on-listen")) {
        found_run_on_listen_line = true;
      }
    }
  }
  return startup_file;
}

REPL::Config load_repl_config(const std::string& username,
                              const SootPlatform game_version,
                              const int nrepl_port) {
    auto repl_config_path = common::get_path(common::PathType::PROJECT) / "goal_src" / "user" /
                            username / "repl-config.json";
  REPL::Config loaded_config(game_version);
  if (file_util::exists(repl_config_path.string())) {
    try {
      auto repl_config_data =
          parse_commented_json(file_util::read_text(repl_config_path), "repl-config.json");
      from_json(repl_config_data, loaded_config);
      loaded_config.temp_nrepl_port = nrepl_port;
      return loaded_config;
    } catch (std::exception& e) {
      // do nothing
    }
  }
  loaded_config.temp_nrepl_port = nrepl_port;
  return loaded_config;
}
}  // namespace REPL
