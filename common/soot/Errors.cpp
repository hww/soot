#include "common/soot/Errors.hpp"
#include "common/soot/Object.hpp"
#include "common/soot/Reader.hpp"
#include <fmt/ranges.h>

namespace soot {

// Основной метод для получения красивого отчета
std::string EvalException::full_report(Reader &reader) const {
    std::string out;
    auto        red = "\033[1;31m", dim = "\033[2;37m", reset = "\033[0m";

    out += fmt::format("\n{}─── ERROR REPORT ──────────────────────────────────{}\n", red, reset);
    out += fmt::format("{}Error: {}{}\n", red, message, reset);

    // Функция для поиска ближайших деталей в базе данных
    auto find_best_info = [&](int start_index) {
        // Сначала проверяем саму форму ошибки (если start_index == -1)
        if (start_index == -1) {
            std::string res = reader.get_db().get_info_for(form);
            if (res != "?")
                return std::make_pair(res, env);
            start_index = 0; // Если не нашли, начинаем со стека
        }

        // Ищем по трейсу вниз
        for (int j = start_index; j < (int)trace.size(); ++j) {
            std::string res = reader.get_db().get_info_for(trace[j].form);
            if (res != "?")
                return std::make_pair(res, trace[j].env);
        }
        return std::make_pair(std::string("?"), std::shared_ptr<EnvironmentObject>(nullptr));
    };

    // 1. ПЕЧАТАЕМ ГЛАВНЫЙ БЛОК (Эпицентр)
    auto [main_source, main_env] = find_best_info(-1);
    if (main_source != "?") {
        out += "\n" + main_source;
    } else {
        out += fmt::format("\n    {}\n", form.print());
    }
    // Переменные для эпицентра печатаем всегда
    out += format_env_vars(main_env) + "\n";

    out += fmt::format("\n{}Traceback (most recent call first):{}\n", dim, reset);

    // 2. ИДЕМ ПО ТРЕЙСУ
    for (int i = 0; i < (int)trace.size(); ++i) {
        const auto &frame = trace[i];
        auto        info = reader.get_db().get_short_info_for(frame.form);

        out += fmt::format("  [{:02d}] ", i);
        if (!frame.message.empty())
            out += fmt::format("{}: ", frame.message);

        // Печатаем базовую строку кадра (как в твоем логе)
        if (info && info->line_number >= 0) {
            out += fmt::format("{}at {}:{}{}", dim, info->filename, info->line_number, reset);
        } else {
            std::string s = frame.form.print();
            if (s.length() > 40)
                s = s.substr(0, 37) + "...";
            out += s;
        }

        // ЕСЛИ НУЖНЫ ДЕТАЛИ (флаг или это первый кадр)
        if (frame.show_details) {
            // Ищем детали для ЭТОГО кадра или ниже
            auto [detail_source, detail_env] = find_best_info(i);

            if (detail_source != "?") {
                // Если нашли детали (пусть даже от соседа ниже), показываем их
                // Но сначала переменные текущего кадра
                out += format_env_vars(frame.env);

                // Печатаем код со стрелочкой (из detail_source)
                // Отрезаем заголовок "at file:line", так как он уже есть выше, оставляем только код
                out += "\n      " + detail_source;
            } else {
                // Если деталей нет вообще нигде ниже, просто покажем переменные
                out += format_env_vars(frame.env);
            }
        }
        out += "\n";
    }

    return out;
}
std::string EvalException::format_env_vars(const std::shared_ptr<EnvironmentObject> &env) const {
    // 1. Проверки на вход
    if (!env || env->is_global)
        return "{}";

    // Используем m_used_entries, если есть доступ, или просто проверяем наличие данных
    const auto &entries = env->vars.get_all_entries();

    std::string out = "  \033[2;37m {";
    bool        first = true;
    int         found_count = 0;

    for (const auto &entry : entries) {
        // Пропускаем пустые слоты хеш-таблицы
        if (entry.key == nullptr) {
            continue;
        }

        if (found_count >= 6) { // Лимит для компактности
            out += ", ...";
            break;
        }

        if (!first)
            out += ", ";

        // Ключ - это const char*. В твоем мапе это имя символа.
        // Безопасно формируем имя
        std::string name_str(entry.key);

        // Значение - печатаем через метод объекта
        std::string val_str;
        try {
            val_str = entry.value.is_null() ? "null" : entry.value.print();
        } catch (...) {
            val_str = "???";
        }

        // Обрезаем слишком длинные значения (например, огромные списки)
        if (val_str.length() > 25) {
            val_str = val_str.substr(0, 22) + "...";
        }

        out += name_str + "=" + val_str;

        first = false;
        found_count++;
    }

    out += "}\033[0m";

    // Если реально ничего не нашли (таблица пустая), возвращаем пустую строку
    return out;
}
} // namespace soot