/**
 * @file logUtils.h
 *
 * @date 2025-03-02
 *
 * @brief This file contains the declarations for the logUtil tool.
 *        This tool is used to log data throughout the system for nominal and off nominal events.
 *
 * @use: LOG_INFO(INFO, "This is useful information");
 *       LOG_MSG(ERROR, "value out of range. board_temp_1 = %d", board_temp_1);
 *       etc...
 *
 * @copyright Neros Technologies Copyright (c) 2025
 *
 */

#pragma once

#ifndef LOG_UTIL_H
#define LOG_UTIL_H

#include <stdio.h>
#include <string>

// #define LOG_OUTPUT_MAVLINK
// #define LOG_OUTPUT_STDOUT

/*---------------------------------------------------------------------------
 * Log‑level definitions
 * (lower value  = more critical)
 *---------------------------------------------------------------------------*/
#define LOG_LEVEL_FATAL    0    /* Fatal errors */
#define LOG_LEVEL_ERROR    1    /* Errors */
#define LOG_LEVEL_WARN     2    /* Warnings and other non-fatal issues */
#define LOG_LEVEL_INFO     3    /* General information */
#define LOG_LEVEL_VERBOSE  4    /* More detailed information that is not normally needed */
#define LOG_LEVEL_DEBUG    5    /* Debugging only */

#define FATAL       LOG_LEVEL_FATAL
#define ERROR       LOG_LEVEL_ERROR
#define WARN        LOG_LEVEL_WARN
#define INFO        LOG_LEVEL_INFO
#define VERBOSE     LOG_LEVEL_VERBOSE
#define DBG         LOG_LEVEL_DEBUG

#define MAVLINK_LOG_ADDRESS 1
#define MAVLINK_STAT_ADDRESS 0

/*---------------------------------------------------------------------------
 * User‑selectable threshold
 *  - default: INFO
 *  - override with  -DLOG_LEVEL=LOG_LEVEL_VERBOSE   (or any level above)
 *---------------------------------------------------------------------------*/
#ifndef LOG_LEVEL
#  define LOG_LEVEL LOG_LEVEL_INFO
#endif

#ifndef LOG_MAX_MSG_LEN
#  define LOG_MAX_MSG_LEN 128u           /* change if you need longer lines */
#endif

/*---------------------------------------------------------------------------
 * Internal helper – emits one macro per level
 *---------------------------------------------------------------------------*/
#define _LOG_LEVEL_ENABLED(lvl)   (LOG_LEVEL >= (lvl))

#if defined(LOG_OUTPUT_STDOUT) || defined(LOG_OUTPUT_MAVLINK)
#  define _LOG_HAS_ANY_BACKEND 1
#else
#  define _LOG_HAS_ANY_BACKEND 0
#endif

#ifdef LOG_OUTPUT_STDOUT
#  define _LOG_BACKEND_STDOUT(str)                                        \
        do { fputs(str, stderr); fputc('\n', stderr); } while (0)
#else
#  define _LOG_BACKEND_STDOUT(str)  ((void)0)
#endif

#ifdef LOG_OUTPUT_MAVLINK
#  ifndef MAVLINK_LOG_ADDRESS
#    error "MAVLINK_LOG_ADDRESS must be defined when LOG_OUTPUT_MAVLINK is used"
#  endif
#  define _LOG_BACKEND_MAVLINK(str)                                       \
        do {                                                              \
            mavNerosDebugStatsPack(MAVLINK_LOG_ADDRESS,                   \
                                   MAVLINK_LOG_ADDRESS,                   \
                                   0, (str), (uint16_t)strlen(str));      \
        } while (0)
#else
#  define _LOG_BACKEND_MAVLINK(str)  ((void)0)
#endif

#if _LOG_HAS_ANY_BACKEND
#  define _LOG_EMIT(tag, fmt, ...)                                        \
        do {                                                              \
            char output_string[LOG_MAX_MSG_LEN] = {0};                          \
            (void)snprintf(output_string, sizeof(output_string),          \
                           tag ": " fmt, ##__VA_ARGS__);                  \
            _LOG_BACKEND_STDOUT(output_string);                           \
            _LOG_BACKEND_MAVLINK(output_string);                          \
        } while (0)
#else
#  define _LOG_DISPATCH(str)  ((void)0)
#endif  /* _LOG_HAS_ANY_BACKEND */

/*-------------------------------------------------------------------------* 
 * Public logging macros                                               *
 *-------------------------------------------------------------------------*/
#if _LOG_LEVEL_ENABLED(LOG_LEVEL_FATAL) && _LOG_HAS_ANY_BACKEND
#  define LOG_FATAL(fmt, ...)   _LOG_EMIT("F",    fmt, ##__VA_ARGS__)
#else
#  define LOG_FATAL(...)        ((void)0)
#endif

#if _LOG_LEVEL_ENABLED(LOG_LEVEL_ERROR) && _LOG_HAS_ANY_BACKEND
#  define LOG_ERROR(fmt, ...)   _LOG_EMIT("E",    fmt, ##__VA_ARGS__)
#else
#  define LOG_ERROR(...)        ((void)0)
#endif

#if _LOG_LEVEL_ENABLED(LOG_LEVEL_WARN)  && _LOG_HAS_ANY_BACKEND
#  define LOG_WARN(fmt, ...)    _LOG_EMIT("W",     fmt, ##__VA_ARGS__)
#else
#  define LOG_WARN(...)         ((void)0)
#endif

#if _LOG_LEVEL_ENABLED(LOG_LEVEL_INFO)  && _LOG_HAS_ANY_BACKEND
#  define LOG_INFO(fmt, ...)    _LOG_EMIT("I",     fmt, ##__VA_ARGS__)
#else
#  define LOG_INFO(...)         ((void)0)
#endif

#if _LOG_LEVEL_ENABLED(LOG_LEVEL_VERBOSE) && _LOG_HAS_ANY_BACKEND
#  define LOG_VERBOSE(fmt, ...) _LOG_EMIT("V",  fmt, ##__VA_ARGS__)
#else
#  define LOG_VERBOSE(...)      ((void)0)
#endif

#if _LOG_LEVEL_ENABLED(LOG_LEVEL_DEBUG) && _LOG_HAS_ANY_BACKEND
#  define LOG_DEBUG(fmt, ...) _LOG_EMIT("D",  fmt, ##__VA_ARGS__)
#else
#  define LOG_DEBUG(...)        ((void)0)
#endif

/*-------------------------------------------------------------------------*
 * But they were, all of them, deceived, for another Macro was made...     *
 *-------------------------------------------------------------------------*/
#if _LOG_HAS_ANY_BACKEND
#  define LOG_MSG(lvl, fmt, ...)                                                        \
        do {                                                                            \
            /* compile‑time filter keeps dead branches out of the binary */             \
            if ((lvl) <= LOG_LEVEL) {                                                   \
                if      ((lvl) == LOG_LEVEL_FATAL)    LOG_FATAL(fmt, ##__VA_ARGS__);    \
                else if ((lvl) == LOG_LEVEL_ERROR)    LOG_ERROR(fmt, ##__VA_ARGS__);    \
                else if ((lvl) == LOG_LEVEL_WARN)     LOG_WARN(fmt, ##__VA_ARGS__);     \
                else if ((lvl) == LOG_LEVEL_INFO)     LOG_INFO(fmt, ##__VA_ARGS__);     \
                else if ((lvl) == LOG_LEVEL_VERBOSE)  LOG_VERBOSE(fmt, ##__VA_ARGS__);  \
                else if ((lvl) == LOG_LEVEL_DEBUG)    LOG_DEBUG(fmt, ##__VA_ARGS__);    \
                else ((void)0);                                                         \
            }                                                                           \
        } while (0)
#else
#  define LOG_MSG(...)      ((void)0)
#endif


#endif /* LOG_UTIL_H */
