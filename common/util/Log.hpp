#pragma once

#include <string>
#include <ctime>

#ifdef __linux__
#include <sys/time.h>
#endif

#include "fmt/format.h"
#include "fmt/color.h"

namespace lg {

#ifdef __linux__
	struct LogTime {
		timeval tv;
	};
#else
	struct LogTime {
		time_t tim;
	};
#endif

	/**
	 * 📊 Уровни логирования SOOT
	 * Каждый уровень включает все предыдущие
	 */
	enum class level {
		/**
		 * 🕵️‍♂️ TRACE (0) - Максимальная детализация
		 * • Каждое выражение и вызов функции
		 * • Для отладки ядра и сложных багов
		 * • Может замедлять работу
		 */
		trace = 0,

		/**
		 * 🔍 DEBUG (1) - Отладочная информация
		 * • Состояния объектов, переменных
		 * • Загрузка файлов, парсинг
		 * • Сетевые соединения
		 * • Для разработчиков
		 */
		debug = 1,

		/**
		 * 📋 INFO (2) - Информационные сообщения
		 * • Старт/остановка сервисов
		 * • Загрузка конфигурации
		 * • Пользовательские операции
		 * • Для мониторинга работы
		 */
		info = 2,

		/**
		 * ⚠️ WARN (3) - Предупреждения
		 * • Нестандартные ситуации
		 * • Устаревшие функции
		 * • Возможные проблемы
		 * • Работа продолжается
		 */
		warn = 3,

		/**
		 * ❌ ERROR (4) - Ошибки выполнения
		 * • Неудачные операции
		 * • Невалидный ввод
		 * • Проблемы с IO
		 * • Требует внимания
		 */
		error = 4,

		/**
		 * 💀 DIE (5) - Критические ошибки
		 * • Неисправимые состояния
		 * • Коррупция памяти
		 * • Аварийное завершение
		 * • После лога вызывает abort()
		 */
		die = 5,

		/**
		 * 🔇 OFF (6) - Полное отключение
		 * • Никаких логов
		 * • Максимальная производительность
		 * • Для production без отладки
		 */
		off = 6,

		/**
		 * 🚨 OFF_UNLESS_DIE (7) - Тихий режим
		 * • Логирует только DIE
		 * • Для production с мониторингом
		 * • Тихий, но спасёт при катастрофе
		 */
		off_unless_die = 7
	};

	namespace internal {
		void log_message(level log_level, LogTime& now, const char* message);
	}  // namespace internal

	// Возвращаем ротацию
	void set_file(const std::string& filename, bool should_rotate = true, bool append = false);
	void set_stdout_level(level log_level);
	void set_file_level(level log_level);
	void disable_ansi_colors();
	void initialize();
	void finish();

	template <typename... Args>
	void log(level log_level, const std::string& format, Args&&... args) {
		LogTime now;
#ifdef __linux__
		gettimeofday(&now.tv, nullptr);
#else
		now.tim = time(nullptr);
#endif
		std::string formatted_message = fmt::format(fmt::runtime(format), std::forward<Args>(args)...);
		internal::log_message(log_level, now, formatted_message.c_str());
	}

	template <typename... Args>
	void trace(const std::string& format, Args&&... args) {
		log(level::trace, format, std::forward<Args>(args)...);
	}

	template <typename... Args>
	void debug(const std::string& format, Args&&... args) {
		log(level::debug, format, std::forward<Args>(args)...);
	}

	template <typename... Args>
	void info(const std::string& format, Args&&... args) {
		log(level::info, format, std::forward<Args>(args)...);
	}

	template <typename... Args>
	void warn(const std::string& format, Args&&... args) {
		log(level::warn, format, std::forward<Args>(args)...);
	}

	template <typename... Args>
	void error(const std::string& format, Args&&... args) {
		log(level::error, format, std::forward<Args>(args)...);
	}

	template <typename... Args>
	void die(const std::string& format, Args&&... args) {
		log(level::die, format, std::forward<Args>(args)...);
	}

	void log_print(const char* message);

	template <typename... Args>
	void print(const std::string& format, Args&&... args) {
		std::string formatted_message = fmt::format(fmt::runtime(format), std::forward<Args>(args)...);
		log_print(formatted_message.c_str());
	}
	template <typename... Args>
	void print(const fmt::text_style& ts, const std::string& format, Args&&... args) {
		std::string formatted_message = fmt::vformat(ts, format, fmt::make_format_args(args...));
		log_print(formatted_message.c_str());
	}
}  // namespace lg