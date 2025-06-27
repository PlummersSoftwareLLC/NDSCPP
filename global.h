#pragma once
using namespace std;
using namespace chrono;

#if __cplusplus < 202002L
// Not C++20 — define a minimal shim
namespace std {
    enum class endian {
        little = 0,
        big    = 1,
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
        native = little
#else
        native = big
#endif
    };
}
#else
// Ensure the standard definition is available
#include <bit>
#endif

// Global Definitions
//
// This file contains global definitions and includes that are used throughout the project.

#include <string>
#ifndef __NetBSD__
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h> // For colored console output
#else
// NetBSD stubs for spdlog functionality
namespace spdlog {
    namespace level {
        enum level_enum {
            trace = 0,
            debug = 1,
            info = 2,
            warn = 3,
            err = 4,
            critical = 5,
            off = 6
        };
    }
    
    class logger {
    public:
        // Template methods to handle variable arguments (like fmt library)
        template<typename... Args>
        void debug(const char* fmt, Args&&... args) const {}
        
        template<typename... Args>
        void info(const char* fmt, Args&&... args) const {}
        
        template<typename... Args>
        void warn(const char* fmt, Args&&... args) const {}
        
        template<typename... Args>
        void error(const char* fmt, Args&&... args) const {}
        
        template<typename... Args>
        void critical(const char* fmt, Args&&... args) const {}
        
        // Overloads for string arguments
        template<typename... Args>
        void debug(const std::string& fmt, Args&&... args) const {}
        
        template<typename... Args>
        void info(const std::string& fmt, Args&&... args) const {}
        
        template<typename... Args>
        void warn(const std::string& fmt, Args&&... args) const {}
        
        template<typename... Args>
        void error(const std::string& fmt, Args&&... args) const {}
        
        template<typename... Args>
        void critical(const std::string& fmt, Args&&... args) const {}
        
        void set_level(level::level_enum level) const {}
    };
    
    inline std::shared_ptr<logger> stdout_color_mt(const std::string& name) {
        return std::make_shared<logger>();
    }
}
#endif

#include "utilities.h"
#include "secrets.h"

extern shared_ptr<spdlog::logger> logger;

// arraysize
//
// Number of elements in a static array

#define arraysize(x) (sizeof(x)/sizeof(x[0]))

// str_snprintf
//
// A safe version of snprintf that returns a string

inline string str_snprintf(const char *fmt, size_t len, ...)
{
    string str(len, '\0');  // Create a string filled with null characters of 'len' length
    va_list args;

    va_start(args, len);
    int out_length = vsnprintf(&str[0], len + 1, fmt, args); // Write into the string's buffer directly
    va_end(args);

    // Resize the string to the actual output length, which vsnprintf returns
    if (out_length >= 0)
    {
        // vsnprintf returns the number of characters that would have been written if n had been sufficiently large
        // not counting the terminating null character.
        if (static_cast<size_t>(out_length) > len)
        {
            // The given length was not sufficient, resize and try again
            str.resize(out_length);  // Make sure the buffer can hold all data
            va_start(args, len);
            vsnprintf(&str[0], out_length + 1, fmt, args);  // Write again with the correct size
            va_end(args);
        }
        else
        {
            // The output fit into the buffer, resize to the actual length used
            str.resize(out_length);
        }
    }
    else
    {
        // If vsnprintf returns an error, clear the string
        str.clear();
    }

    return str;
}

//
// Helpful global functions
//

inline double millis()
{
   return (double)clock() / CLOCKS_PER_SEC * 1000;
}

inline void delay(int ms)
{
    this_thread::sleep_for((milliseconds)ms);
}
