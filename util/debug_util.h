#ifndef __DEBUG_UTIL_H__
#define __DEBUG_UTIL_H__

#pragma once


#include <memory>

#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <ctime>


namespace debug {

class Logger
{
   private:
    enum Level {
        NONE = 0,
        ERROR = 1,
        WARNING = 2,
        INFO = 3,
        DEBUG = 4
    };

    int _level;

    Logger() :
        _level(ERROR)
    {
        char *log_level = getenv("TETRIS_LOGLEVEL");

        if (log_level) {
            if (strcmp(log_level, "DEBUG") == 0)
                _level = DEBUG;
            else if (strcmp(log_level, "INFO") == 0)
                _level = INFO;
            else if (strcmp(log_level, "WARNING") == 0)
                _level = WARNING;
            else if (strcmp(log_level, "ERROR") == 0)
                _level = ERROR;
        }
    }

    template <typename... Args>
    void output(const char* lvl, const char* fmt, Args... args)
    {
        time_t rawtime;
        struct tm* timeinfo;

        std::time(&rawtime);
        timeinfo = std::localtime(&rawtime);

        char timestr[80];
        std::strftime(timestr, 80, "%Y-%m-%d %H:%M:%S", timeinfo);

        char buffer[255];
        std::snprintf(buffer, 255, "%s %s: %s", lvl, timestr, fmt);

        char result[1000];
        std::snprintf(result, 1000, buffer, args...);
        std::printf("%s", result);
        std::fflush(stdout);
    }

   public:
    template <typename... Args>
    void debug(const char* fmt, Args... args)
    {
        if (_level >= DEBUG)
            output("DEBUG", fmt, args...);
    }

    template <typename... Args>
    void info(const char* fmt, Args... args)
    {
        if (_level >= INFO)
            output("INFO", fmt, args...);
    }

    template <typename... Args>
    void warning(const char* fmt, Args... args)
    {
        if (_level >= WARNING)
            output("WARNING", fmt, args...);
    }

    template <typename... Args>
    void error(const char* fmt, Args... args)
    {
        if (_level >= ERROR)
            output("ERROR", fmt, args...);
    }

    template <typename... Args>
    void always(const char* fmt, Args... args)
    {
        char result[1000];
        std::snprintf(result, 1000, fmt, args...);
        std::printf("%s", result);
        std::fflush(stdout);
    }

   private:
    static std::shared_ptr<Logger> instance;

   public:
    static std::shared_ptr<Logger> get()
    {
        if (!instance)
            instance = std::shared_ptr<Logger>(new Logger());

        return instance;
    }
};

using LoggerPtr = std::shared_ptr<Logger>;

#define LOGGER debug::Logger::get()

template <typename BaseComp>
struct CompRepr
{
    constexpr const static char* repr = "??";
};

template <typename T>
struct CompRepr<std::greater<T>>
{
    constexpr const static char* repr = ">";
};

template <typename T>
struct CompRepr<std::greater_equal<T>>
{
    constexpr const static char* repr = ">=";
};

template <typename T>
struct CompRepr<std::less<T>>
{
    constexpr const static char* repr = "<";
};

template <typename T>
struct CompRepr<std::less_equal<T>>
{
    constexpr const static char* repr = "<=";
};

template <typename T>
struct CompRepr<std::equal_to<T>>
{
    constexpr const static char* repr = "==";
};

template <typename T>
struct CompRepr<std::not_equal_to<T>>
{
    constexpr const static char* repr = "!=";
};

} /* namespace debug */

#endif /* __DEBUG_UTIL_H__ */
