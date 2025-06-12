/**
 * @file version.h
 * @brief THIS FILE SHOULD ONLY BE MODIFIED WHEN UPDATING THE ELRS
 *        RELEASE VERSION NUMBER
 * @date 2025-04-14
 *
 * @copyright Neros Technologies Copyright (c) 2025
 *
 */
#ifndef VERSION_H
#define VERSION_H

#include <stdint.h>

#define ELRS_VERSION_MAJOR 5
#define ELRS_VERSION_MINOR 0
#define ELRS_VERSION_PATCH 1
#define ELRS_VERSION_RC 2

typedef union {
    struct {
        uint8_t major;
        uint8_t minor;
        uint8_t patch;
        uint8_t rc;
    };
    uint32_t version;
} version_info_ts;

#endif // VERSION_H