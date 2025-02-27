/**
 * @file logger.h
 * @brief High-performance thread-safe logging system
 *
 * This file provides a thread-safe logging system with multiple log levels,
 * file and console output, and customizable formatting.
 */

#ifndef LOGGER_H
#define LOGGER_H

#include <stdio.h>
#include <stdarg.h>
#include <stdbool.h>
#include <time.h>
#include <pthread.h>

// Log levels
typedef enum {
    LOG_TRACE = 0,
    LOG_DEBUG,
    LOG_INFO,
    LOG_WARN,
    LOG_ERROR,
    LOG_FATAL,
    LOG_LEVEL_COUNT
} log_level_t;

// Log output destination
typedef enum {
    LOG_OUTPUT_CONSOLE = 1,
    LOG_OUTPUT_FILE = 2,
    LOG_OUTPUT_SYSLOG = 4,
    LOG_OUTPUT_CALLBACK = 8,
    LOG_OUTPUT_ALL = 15
} log_output_t;

// Log colors
typedef enum {
    LOG_COLOR_RESET = 0,
    LOG_COLOR_BLACK,
    LOG_COLOR_RED,
    LOG_COLOR_GREEN,
    LOG_COLOR_YELLOW,
    LOG_COLOR_BLUE,
    LOG_COLOR_MAGENTA,
    LOG_COLOR_CYAN,
    LOG_COLOR_WHITE,
    LOG_COLOR_BRIGHT_BLACK,
    LOG_COLOR_BRIGHT_RED,
    LOG_COLOR_BRIGHT_GREEN,
    LOG_COLOR_BRIGHT_YELLOW,
    LOG_COLOR_BRIGHT_BLUE,
    LOG_COLOR_BRIGHT_MAGENTA,
    LOG_COLOR_BRIGHT_CYAN,
    LOG_COLOR_BRIGHT_WHITE,
    LOG_COLOR_COUNT
} log_color_t;

// Log callback function type
typedef void (*log_callback_fn)(log_level_t level, const char* file, int line, const char* func, const char* message);

// Logger configuration
typedef struct {
    log_level_t level;                  // Current log level
    log_output_t outputs;               // Output destinations
    FILE* file;                         // Log file handle
    const char* file_path;              // Log file path
    bool use_colors;                    // Use colors in console output
    bool show_timestamp;                // Show timestamp in log messages
    bool show_level;                    // Show log level in log messages
    bool show_file_line;                // Show file and line in log messages
    bool show_function;                 // Show function name in log messages
    log_callback_fn callback;           // Callback function
    pthread_mutex_t mutex;              // Mutex for thread safety
} logger_t;

// Global logger instance
extern logger_t g_logger;

/**
 * @brief Initialize the logger
 * 
 * @param level Minimum log level to output
 * @param outputs Output destinations (can be OR'ed)
 * @param file_path Path to log file (if LOG_OUTPUT_FILE is set)
 * @return true if initialization succeeded, false otherwise
 */
bool logger_init(log_level_t level, log_output_t outputs, const char* file_path);

/**
 * @brief Shutdown the logger and free resources
 */
void logger_shutdown(void);

/**
 * @brief Set the log level
 * 
 * @param level New log level
 */
void logger_set_level(log_level_t level);

/**
 * @brief Set the log outputs
 * 
 * @param outputs New output destinations (can be OR'ed)
 */
void logger_set_outputs(log_output_t outputs);

/**
 * @brief Set the log file path
 * 
 * @param file_path Path to log file
 * @return true if file was opened successfully, false otherwise
 */
bool logger_set_file(const char* file_path);

/**
 * @brief Set whether to use colors in console output
 * 
 * @param use_colors true to use colors, false otherwise
 */
void logger_set_colors(bool use_colors);

/**
 * @brief Set the log callback function
 * 
 * @param callback Callback function
 */
void logger_set_callback(log_callback_fn callback);

/**
 * @brief Log a message with the specified level
 * 
 * @param level Log level
 * @param file Source file
 * @param line Line number
 * @param func Function name
 * @param fmt Format string
 * @param ... Format arguments
 */
void logger_log(log_level_t level, const char* file, int line, const char* func, const char* fmt, ...);

/**
 * @brief Log a message with variable arguments
 * 
 * @param level Log level
 * @param file Source file
 * @param line Line number
 * @param func Function name
 * @param fmt Format string
 * @param args Variable argument list
 */
void logger_logv(log_level_t level, const char* file, int line, const char* func, const char* fmt, va_list args);

/**
 * @brief Get the name of a log level
 * 
 * @param level Log level
 * @return Name of the log level
 */
const char* logger_level_name(log_level_t level);

/**
 * @brief Get the ANSI color code for a log level
 * 
 * @param level Log level
 * @return ANSI color code
 */
const char* logger_level_color(log_level_t level);

/**
 * @brief Get the ANSI color code for a color
 * 
 * @param color Color
 * @return ANSI color code
 */
const char* logger_color_code(log_color_t color);

// Convenience macros for logging
#define LOG_TRACE(...) logger_log(LOG_TRACE, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define LOG_DEBUG(...) logger_log(LOG_DEBUG, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define LOG_INFO(...)  logger_log(LOG_INFO,  __FILE__, __LINE__, __func__, __VA_ARGS__)
#define LOG_WARN(...)  logger_log(LOG_WARN,  __FILE__, __LINE__, __func__, __VA_ARGS__)
#define LOG_ERROR(...) logger_log(LOG_ERROR, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define LOG_FATAL(...) logger_log(LOG_FATAL, __FILE__, __LINE__, __func__, __VA_ARGS__)

// Conditional logging macros
#define LOG_TRACE_IF(cond, ...) do { if (cond) LOG_TRACE(__VA_ARGS__); } while (0)
#define LOG_DEBUG_IF(cond, ...) do { if (cond) LOG_DEBUG(__VA_ARGS__); } while (0)
#define LOG_INFO_IF(cond, ...)  do { if (cond) LOG_INFO(__VA_ARGS__);  } while (0)
#define LOG_WARN_IF(cond, ...)  do { if (cond) LOG_WARN(__VA_ARGS__);  } while (0)
#define LOG_ERROR_IF(cond, ...) do { if (cond) LOG_ERROR(__VA_ARGS__); } while (0)
#define LOG_FATAL_IF(cond, ...) do { if (cond) LOG_FATAL(__VA_ARGS__); } while (0)

// Assertion macro with logging
#define LOG_ASSERT(cond, ...) \
    do { \
        if (!(cond)) { \
            LOG_ERROR("Assertion failed: %s", #cond); \
            LOG_ERROR(__VA_ARGS__); \
            abort(); \
        } \
    } while (0)

#endif /* LOGGER_H */ 