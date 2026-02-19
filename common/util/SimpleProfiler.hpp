// simple_profiler.h
#pragma once
#include <algorithm>
#include <chrono>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

struct ProfileEvent {
    std::string name;
    long long   start;
    long long   end;
    size_t      thread_id;

    ProfileEvent(const char *n, long long s, size_t tid)
        : name(n), start(s), end(0), thread_id(tid) {}
};

class SimpleProfiler {
    std::vector<ProfileEvent> events;

  public:
    void begin(const char *name) {
        events.emplace_back(name, now(), get_tid());
    }

    void end() {
        // Ищем последнее незакрытое событие
        for (int i = events.size() - 1; i >= 0; --i) {
            if (events[i].end == 0) {
                events[i].end = now();
                break;
            }
        }
    }

    void dump(const char *filename) {
        std::ofstream f(filename);
        if (!f.is_open())
            return;

        f << "{\"traceEvents\":[";

        bool first = true;
        for (const auto &e : events) {
            if (e.end == 0)
                continue; // пропускаем незакрытые

            if (!first)
                f << ",";
            first = false;

            // BEGIN событие
            f << "{";
            f << "\"name\":\"" << e.name << "\",";
            f << "\"ph\":\"B\",";
            f << "\"ts\":" << e.start << ",";
            f << "\"pid\":1,";
            f << "\"tid\":" << e.thread_id;
            f << "},";

            // END событие
            f << "{";
            f << "\"ph\":\"E\",";
            f << "\"ts\":" << e.end << ",";
            f << "\"pid\":1,";
            f << "\"tid\":" << e.thread_id;
            f << "}";
        }

        f << "]}";
        f.close();
    }

    void clear() {
        events.clear();
    }

  private:
    static long long now() {
        return std::chrono::steady_clock::now().time_since_epoch().count();
    }

    static size_t get_tid() {
        return std::hash<std::thread::id>{}(std::this_thread::get_id());
    }
};

// Макросы для удобства
#define CONCAT_IMPL(a, b)   a##b
#define CONCAT(a, b)        CONCAT_IMPL(a, b)
#define PROFILE_BLOCK(name) ProfileBlock CONCAT(block, __LINE__)(name)

// Класс для RAII
class ProfileBlock {
  public:
    ProfileBlock(const char *name) {
        prof.begin(name);
    }
    ~ProfileBlock() {
        prof.end();
    }
    static SimpleProfiler prof;
};
