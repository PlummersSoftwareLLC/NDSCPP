#pragma once
using namespace std;
using namespace chrono;

// Global Definitions
//
// This file contains global definitions and includes that are used throughout the project.

#include <string>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h> // For colored console output
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
   return static_cast<double>(duration_cast<microseconds>(steady_clock::now().time_since_epoch()).count()) / 1000.0;
}

inline void delay(int ms)
{
    this_thread::sleep_for(milliseconds(ms));
}
