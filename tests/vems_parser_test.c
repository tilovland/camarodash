#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>

#include "vems.h"

static bool feed_packet(vems_parser_t *parser, vems_data_t *data,
                        uint8_t channel, uint16_t value, bool valid_checksum)
{
    uint8_t packet[] = {
        channel,
        0xA3,
        (uint8_t)(value >> 8),
        (uint8_t)value,
        0,
    };
    packet[4] = (uint8_t)(packet[0] + packet[1] + packet[2] + packet[3]);
    if (!valid_checksum) {
        packet[4]++;
    }

    bool updated = false;
    for (size_t i = 0; i < sizeof(packet); ++i) {
        updated |= vems_parser_feed(parser, packet[i], data);
    }
    return updated;
}

int main(void)
{
    vems_parser_t parser;
    vems_data_t data = {0};
    vems_parser_init(&parser);

    assert(feed_packet(&parser, &data, 0x01, 6789, true));
    assert(data.rpm == 6789);
    assert(data.valid_mask == VEMS_VALID_RPM);

    assert(feed_packet(&parser, &data, 0x02, 1013, true));
    assert(fabsf(data.map_kpa - 101.3f) < 0.01f);

    assert(feed_packet(&parser, &data, 0x04, 130, true));
    assert(data.coolant_c == 90);

    assert(feed_packet(&parser, &data, 0x06, 985, true));
    assert(fabsf(data.lambda - 0.985f) < 0.001f);
    assert(data.valid_mask == (VEMS_VALID_RPM | VEMS_VALID_MAP |
                               VEMS_VALID_CLT | VEMS_VALID_LAMBDA));

    const uint16_t previous_rpm = data.rpm;
    assert(!feed_packet(&parser, &data, 0x01, 1234, false));
    assert(data.rpm == previous_rpm);

    /* Noise and a false start must not prevent the next packet from parsing. */
    const uint8_t noise[] = {0xFF, 0x00, 0x01, 0x02};
    for (size_t i = 0; i < sizeof(noise); ++i) {
        vems_parser_feed(&parser, noise[i], &data);
    }
    assert(feed_packet(&parser, &data, 0x01, 2500, true));
    assert(data.rpm == 2500);

    puts("VEMS parser tests passed");
    return 0;
}
