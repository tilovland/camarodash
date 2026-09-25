#include "vems.h"

#include <string.h>

void vems_parser_init(vems_parser_t *parser)
{
    memset(parser, 0, sizeof(*parser));
}

static bool is_channel(uint8_t byte)
{
    return byte >= 0x01 && byte <= 0x20;
}

static bool apply_channel(uint8_t channel, uint16_t raw, vems_data_t *data)
{
    switch (channel) {
    case 0x01:
        data->rpm = raw;
        data->valid_mask |= VEMS_VALID_RPM;
        return true;
    case 0x02:
        data->map_kpa = raw * 0.1f;
        data->valid_mask |= VEMS_VALID_MAP;
        return true;
    case 0x04:
        data->coolant_c = (int16_t)raw - 40;
        data->valid_mask |= VEMS_VALID_CLT;
        return true;
    case 0x06:
        data->lambda = raw * 0.001f;
        data->valid_mask |= VEMS_VALID_LAMBDA;
        return true;
    default:
        return false;
    }
}

bool vems_parser_feed(vems_parser_t *parser, uint8_t byte, vems_data_t *data)
{
    if (parser == NULL || data == NULL) {
        return false;
    }

    if (parser->index == 0) {
        if (is_channel(byte)) {
            parser->bytes[0] = byte;
            parser->index = 1;
        }
        return false;
    }

    if (parser->index == 1) {
        if (byte == 0xA3) {
            parser->bytes[1] = byte;
            parser->index = 2;
        } else {
            /* The current byte may itself be the start of the next packet. */
            parser->index = 0;
            if (is_channel(byte)) {
                parser->bytes[0] = byte;
                parser->index = 1;
            }
        }
        return false;
    }

    parser->bytes[parser->index++] = byte;
    if (parser->index < sizeof(parser->bytes)) {
        return false;
    }

    parser->index = 0;
    const uint8_t checksum = (uint8_t)(parser->bytes[0] + parser->bytes[1] +
                                       parser->bytes[2] + parser->bytes[3]);
    if (checksum != parser->bytes[4]) {
        return false;
    }

    const uint16_t raw = ((uint16_t)parser->bytes[2] << 8) | parser->bytes[3];
    return apply_channel(parser->bytes[0], raw, data);
}
