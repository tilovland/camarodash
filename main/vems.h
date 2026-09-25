#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint16_t rpm;
    float map_kpa;
    int16_t coolant_c;
    float lambda;
    uint8_t valid_mask;
} vems_data_t;

enum {
    VEMS_VALID_RPM = 1U << 0,
    VEMS_VALID_MAP = 1U << 1,
    VEMS_VALID_CLT = 1U << 2,
    VEMS_VALID_LAMBDA = 1U << 3,
};

typedef struct {
    uint8_t bytes[5];
    size_t index;
} vems_parser_t;

void vems_parser_init(vems_parser_t *parser);
bool vems_parser_feed(vems_parser_t *parser, uint8_t byte, vems_data_t *data);
