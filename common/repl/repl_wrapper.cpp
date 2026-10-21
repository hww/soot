#include "repl_wrapper.h"

#include "CommonTypes.hpp"
#include "util/FileUtil.hpp"
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

void Wrapper::print_welcome_message(const std::vector<std::string>& loaded_projects) {
    std::string message;
    auto orange = fg(fmt::color::orange) | fmt::emphasis::bold;
    auto cyan = fg(fmt::color::cyan);
    auto gray = fg(fmt::color::gray);

    auto add_line = [&](int logo_idx, const std::string& text) {
        // Убрали "{}", теперь всё корректно
        message += fmt::format(orange, fmt::runtime(logo[logo_idx]));
        message += text + "\n";
    };

    add_line(0, "");
    add_line(1, "");
    
    std::string welcome = fmt::format(fmt::emphasis::bold, "  Welcome to SOOT {}.{} [{}]",
                          versions::SOOT_VERSION_MAJOR, versions::SOOT_VERSION_MINOR,
                          fmt::format(gray, "{}", BUILT_SHA));
    
    if (!username.empty() && username != "#f" && username != "unknown") {
        welcome += fmt::format(fg(fmt::color::light_green), " {}", username);
    }
    add_line(2, welcome + "!");

    std::string platform = (repl_config.game_version == SootPlatform::Default) 
        ? fmt::format("     [{}]: ", fmt::format(orange, "default"))
        : fmt::format("     [{}]: ", fmt::format(fg(fmt::color::magenta), "undefined"));
    
    add_line(3, platform + fmt::format(gray, "{}", str_util::join(loaded_projects, ",")));
    add_line(4, fmt::format("  Project Path: {}", 
                fmt::format(gray, "{}", file_util::get_path(file_util::PathType::PROJECT).string())));
    add_line(5, "");

    std::string nrepl = "  nREPL: ";
    nrepl += (!nrepl_alive) 
        ? fmt::format(fg(fmt::color::red), "DISABLED")
        : fmt::format(fg(fmt::color::light_green), "Listening on {}", repl_config.get_nrepl_port());
    add_line(6, nrepl);

    add_line(7, fmt::format("  Source File Search Dirs: [{}]", 
                fmt::format(gray, "{}", str_util::join(repl_config.asm_file_search_dirs, ","))));
    add_line(8, fmt::format("    {} or {} for basic help", fmt::format(cyan, "(repl-help)"), fmt::format(cyan, "(repl-keybinds)")));
    add_line(9, fmt::format("    {} to connect to the game", fmt::format(cyan, "(lt)")));
    add_line(10, fmt::format("    {} to recompile the active project.", fmt::format(cyan, "(mi)")));
    add_line(11, "");

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
      path = file_util::get_path(PathType::CACHE) /
             soot_platform_to_game_name(repl_config.game_version) /
           ".opengoal.repl.history";
  } else {
    path = file_util::get_path(PathType::CONFIG) / ".sot.repl.history";
  }
  file_util::create_dirs_for_file(path.string());
  repl.history_save(path.string());
}

void Wrapper::load_history() {
    fs::path path;
    if (repl_config.per_game_history) {
        path = file_util::get_path(PathType::CACHE) /
               soot_platform_to_game_name(repl_config.game_version) / ".opengoal.repl.history";
    } else {
        path = file_util::get_path(PathType::CONFIG) / ".sot.repl.history";
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
void Wrapper::print_help_message() {
    // ============================================================
    // Заголовок
    // ============================================================
    fmt::print(fmt::emphasis::bold, "\n╔══════════════════════════════════════════════════╗\n");
    fmt::print(fmt::emphasis::bold, "║              SOOTC REPL — HELP                   ║\n");
    fmt::print(fmt::emphasis::bold, "╚══════════════════════════════════════════════════╝\n");

    fmt::print(fmt::emphasis::bold | fg(fmt::color::yellow),
               "\nNOTE: the legacy ':command' syntax is no longer supported.\n");
    fmt::print(fg(fmt::color::dim_gray),
               "      Every REPL control is now an ordinary S-expression:\n");
    fmt::print(fg(fmt::color::dim_gray),
               "      (e), (reload), (help), (clear), (sc-run ...), etc.\n");

    // ============================================================
    // Session control
    // ============================================================
    fmt::print(fmt::emphasis::bold, "\n── Session control ────────────────────────────────\n");

    fmt::print(fmt::emphasis::bold | fg(fmt::color::cyan), "(e)\n");
    fmt::print(" - Exit the compiler\n");

    fmt::print(fmt::emphasis::bold | fg(fmt::color::cyan), "(reload)\n");
    fmt::print(" - Reload the user profile and reinitialise the environment\n");

    fmt::print(fmt::emphasis::bold | fg(fmt::color::cyan), "(clear)\n");
    fmt::print(" - Clear the screen\n");

    fmt::print(fmt::emphasis::bold | fg(fmt::color::cyan), "(help)\n");
    fmt::print(" - Show this message\n");

    // ============================================================
    // Environment inspection
    // ============================================================
    fmt::print(fmt::emphasis::bold, "\n── Environment inspection ─────────────────────────\n");

    fmt::print(fmt::emphasis::bold | fg(fmt::color::lime_green), "(inspect-env)\n");
    fmt::print(" - Dump the compiler environment (*soot-env*): every macro,\n");
    fmt::print("   function and special form visible to the compiler\n");

    fmt::print(fmt::emphasis::bold | fg(fmt::color::lime_green), "(inspect-env-soots)\n");
    fmt::print(" - Dump the interpreter environment (*global-env*): lib.st\n");
    fmt::print("   macros and interpreter built-ins\n");

    fmt::print(fmt::emphasis::bold | fg(fmt::color::lime_green), "(has-macro \"name\")\n");
    fmt::print(" - Report whether `name` is a macro registered in *soot-env*\n");

    fmt::print(fmt::emphasis::bold | fg(fmt::color::lime_green), "(list-symbols)\n");
    fmt::print(" - List every symbol registered in the global StringId registry\n");

    fmt::print(fmt::emphasis::bold | fg(fmt::color::lime_green), "(sizes)\n");
    fmt::print(" - Print the sizes of all Carbon VM structures\n");

    // ============================================================
    // Execution
    // ============================================================
    fmt::print(fmt::emphasis::bold, "\n── Execution ──────────────────────────────────────\n");

    fmt::print(fmt::emphasis::bold | fg(fmt::color::magenta), "(sc-run name arg...)\n");
    fmt::print(" - Call a compiled function in the active runtime.\n");
    fmt::print(fg(fmt::color::dim_gray),
               "   Argument types are inferred: integer -> i64, float -> f32,\n");
    fmt::print(fg(fmt::color::dim_gray), "   string -> std::string, symbol -> StringId\n");
    fmt::print(fg(fmt::color::dim_gray), "   Example: (sc-run gcd 48 18)\n");

    fmt::print(fmt::emphasis::bold | fg(fmt::color::magenta), "(sc-eval expr)\n");
    fmt::print(" - Evaluate a SOOT expression in *global-env* and print the result.\n");
    fmt::print(fg(fmt::color::dim_gray), "   Example: (sc-eval (+ 1 2 3))\n");

    // ============================================================
    // Compiler parameters
    // ============================================================
    fmt::print(fmt::emphasis::bold, "\n── Compiler parameters ────────────────────────────\n");

    fmt::print(fmt::emphasis::bold | fg(fmt::color::orange), "(arg-set \"key\" value)\n");
    fmt::print(" - Set a mutable compiler parameter. Takes effect immediately.\n");
    fmt::print(fg(fmt::color::dim_gray), "   Mutable keys:\n");
    fmt::print(fg(fmt::color::dim_gray),
               "     \"print-listing\"  #t|#f   print binary listing after each compile\n");
    fmt::print(fg(fmt::color::dim_gray), "     \"print-ir\"       #t|#f   print IR\n");
    fmt::print(fg(fmt::color::dim_gray), "     \"print-ast\"      #t|#f   print AST\n");
    fmt::print(fg(fmt::color::dim_gray),
               "     \"print-asm\"      #t|#f   print generated assembly\n");
    fmt::print(fg(fmt::color::dim_gray),
               "     \"log-level\"      \"debug\"|\"info\"|\"warn\"|\"error\"\n");

    fmt::print(fmt::emphasis::bold | fg(fmt::color::orange), "(arg-get \"key\")\n");
    fmt::print(" - Read a compiler parameter. Unknown keys return (unset).\n");
    fmt::print(fg(fmt::color::dim_gray), "   Read-only keys (settable only from the CLI):\n");
    fmt::print(fg(fmt::color::dim_gray), "     \"sidbase\", \"runtime\", \"user-profile\"\n");

    // ============================================================
    // Session persistence
    // ============================================================
    fmt::print(fmt::emphasis::bold, "\n── Session persistence ────────────────────────────\n");

    fmt::print(fmt::emphasis::bold | fg(fmt::color::cyan), "(save-session [\"name\"])\n");
    fmt::print(" - Compile every top-level form entered in this session and save:\n");
    fmt::print(fg(fmt::color::dim_gray), "     <name>.bin  — compiled binary\n");
    fmt::print(fg(fmt::color::dim_gray), "     <name>.h    — C header for user-defined types\n");
    fmt::print(fg(fmt::color::dim_gray), "   `name` defaults to the current session name.\n");

    // ============================================================
    // Language forms you can type
    // ============================================================
    fmt::print(fmt::emphasis::bold, "\n── Language forms ─────────────────────────────────\n");

    fmt::print(fg(fmt::color::dim_gray), "   (define x 10)              define a variable\n");
    fmt::print(fg(fmt::color::dim_gray), "   (set! x 20)                mutate a variable\n");
    fmt::print(fg(fmt::color::dim_gray), "   (defun name (args) body)   define a function\n");
    fmt::print(fg(fmt::color::dim_gray), "   (defmacro name (args) body) define a goal macro\n");
    fmt::print(fg(fmt::color::dim_gray), "   (if test then else)        conditional\n");
    fmt::print(fg(fmt::color::dim_gray),
               "   (cond (test expr) ...)     multi-branch conditional\n");
    fmt::print(fg(fmt::color::dim_gray), "   (let ((v e)...) body)      local bindings\n");
    fmt::print(fg(fmt::color::dim_gray), "   (lambda (args) body)       anonymous function\n");
    fmt::print(fg(fmt::color::dim_gray), "   (deftype name (parent) (fields...))  define a type\n");
    fmt::print(fg(fmt::color::dim_gray), "   (defenum name (entries...))         define an enum\n");
    fmt::print(fg(fmt::color::dim_gray),
               "   (require \"path/to/file.sc\")         load another module\n");

    // ============================================================
    // Modes
    // ============================================================
    fmt::print(fmt::emphasis::bold, "\n── Modes ──────────────────────────────────────────\n");
    fmt::print(fg(fmt::color::dim_gray), "   sc >   COMPILE_ONLY    compile every input\n");
    fmt::print(fg(fmt::color::dim_gray), "   si >   INTERPRET_ONLY  interpret every input\n");
    fmt::print(fg(fmt::color::dim_gray),
               "   sci>   HYBRID          interpret, fall back to compile\n");

    // ============================================================
    // Footer
    // ============================================================
    fmt::print(fmt::emphasis::bold, "\n══════════════════════════════════════════════════\n\n");
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
  auto user_dir = file_util::get_path(file_util::PathType::PROJECT) / "user";
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
  auto game_specific_path = file_util::get_path(file_util::PathType::CONFIG) / 
                            fmt::format("startup-{}.sot", soot_platform_to_game_name(game_version));
  if (file_util::exists(game_specific_path)) {
    return game_specific_path;
  }
  
  // Затем проверяем общий startup файл в CONFIG
  auto config_path = file_util::get_path(file_util::PathType::CONFIG) / "startup.sot";
  if (file_util::exists(config_path)) {
    return config_path;
  }
  
  // Затем проверяем в PROJECT (если есть проект-специфичный)
  auto project_path = file_util::get_path(file_util::PathType::PROJECT) / "startup.sot";
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
  auto repl_config_path =
      file_util::get_path(file_util::PathType::PROJECT) / "goal_src" / "user" / username / "repl-config.json";
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
