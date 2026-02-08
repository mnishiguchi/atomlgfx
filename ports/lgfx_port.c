// AtomVM port driver: lgfx_port (LovyanGFX bridge, MVP)
//
// Protocol
// - Request: <<opcode:u8, payload:binary>>
// - Reply OK:    <<0x00, payload:binary>>
// - Reply Error: <<0x01, code:u8>>

#include "lgfx_port.h"
#include "lgfx_device.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include <context.h>
#include <globalcontext.h>
#include <mailbox.h>
#include <port.h>
#include <portnifloader.h>
#include <term.h>

#include <trace.h>

#include "esp_err.h"
#include "esp_heap_caps.h"
#include "esp_log.h"

#define TAG "lgfx_port"

#ifndef LGFX_PORT_DEBUG
#define LGFX_PORT_DEBUG 1
#endif

#if LGFX_PORT_DEBUG
#define DLOGI(...) ESP_LOGI(TAG, __VA_ARGS__)
#define DLOGW(...) ESP_LOGW(TAG, __VA_ARGS__)
#define DLOGE(...) ESP_LOGE(TAG, __VA_ARGS__)
#else
#define DLOGI(...) ((void) 0)
#define DLOGW(...) ((void) 0)
#define DLOGE(...) ((void) 0)
#endif

enum
{
    OPCODE_PING = 0x01,
    OPCODE_INIT = 0x10,
    OPCODE_FILL_SCREEN = 0x11,
    OPCODE_DRAW_TEXT = 0x12
};

static term make_error(Context *ctx, uint8_t code)
{
    const size_t out_len = 2;

    port_ensure_available(ctx, term_binary_heap_size(out_len));

    term bin = term_create_uninitialized_binary(out_len, &ctx->heap, ctx->global);
    uint8_t *out = (uint8_t *) term_binary_data(bin);

    out[0] = 0x01;
    out[1] = code;

    return bin;
}

static term make_ok_with_payload(Context *ctx, const uint8_t *payload, size_t payload_len)
{
    const size_t out_len = 1 + payload_len;

    port_ensure_available(ctx, term_binary_heap_size(out_len));

    term bin = term_create_uninitialized_binary(out_len, &ctx->heap, ctx->global);
    uint8_t *out = (uint8_t *) term_binary_data(bin);

    out[0] = 0x00;
    if (payload_len > 0 && payload) {
        memcpy(out + 1, payload, payload_len);
    }

    return bin;
}

static bool read_u16be(const uint8_t *p, size_t len, size_t off, uint16_t *out)
{
    if (off + 2 > len) {
        return false;
    }
    *out = ((uint16_t) p[off] << 8) | (uint16_t) p[off + 1];
    return true;
}

static bool read_i16be(const uint8_t *p, size_t len, size_t off, int16_t *out)
{
    uint16_t tmp;
    if (!read_u16be(p, len, off, &tmp)) {
        return false;
    }
    *out = (int16_t) tmp;
    return true;
}

static void log_heap_stats(const char *label)
{
#if LGFX_PORT_DEBUG
    uint32_t internal = (uint32_t) heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    uint32_t psram = (uint32_t) heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    DLOGI("%s heap: free internal=%u psram=%u", label, (unsigned) internal, (unsigned) psram);
#else
    (void) label;
#endif
}

static term handle_call(Context *ctx, term req)
{
    if (!term_is_binary(req)) {
        DLOGW("handle_call: req is not a binary");
        return make_error(ctx, 0x10);
    }

    const uint8_t *data = (const uint8_t *) term_binary_data(req);
    size_t len = term_binary_size(req);

    if (len < 1) {
        DLOGW("handle_call: req binary too short (len=%u)", (unsigned) len);
        return make_error(ctx, 0x11);
    }

    const uint8_t opcode = data[0];
    DLOGI("handle_call: opcode=0x%02x len=%u", (unsigned) opcode, (unsigned) len);

    switch (opcode) {
        case OPCODE_PING: {
            static const uint8_t pong[] = { 'P', 'O', 'N', 'G' };
            return make_ok_with_payload(ctx, pong, sizeof(pong));
        }

        case OPCODE_INIT: {
            if (len < 2) {
                DLOGW("INIT: missing rotation byte (len=%u)", (unsigned) len);
                return make_error(ctx, 0x20);
            }

            const uint8_t rotation = data[1];
            DLOGI("INIT: rotation=%u", (unsigned) rotation);

            esp_err_t err = lgfx_device_init(rotation);
            if (err != ESP_OK) {
                DLOGE("lgfx_device_init failed: %d", (int) err);
                return make_error(ctx, 0x21);
            }

            const uint8_t ok = 0x01;
            return make_ok_with_payload(ctx, &ok, 1);
        }

        case OPCODE_FILL_SCREEN: {
            uint16_t color;
            if (!read_u16be(data, len, 1, &color)) {
                DLOGW("FILL_SCREEN: missing rgb565 (len=%u)", (unsigned) len);
                return make_error(ctx, 0x30);
            }

            DLOGI("FILL_SCREEN: rgb565=0x%04x", (unsigned) color);

            esp_err_t err = lgfx_device_fill_screen(color);
            if (err != ESP_OK) {
                DLOGE("fill_screen failed: %d", (int) err);
                return make_error(ctx, 0x31);
            }

            const uint8_t ok = 0x01;
            return make_ok_with_payload(ctx, &ok, 1);
        }

        case OPCODE_DRAW_TEXT: {
            int16_t x, y;
            uint16_t color;
            uint8_t size;
            uint16_t text_len;

            // opcode(1)
            // x(i16be) y(i16be) color(u16be) size(u8) len(u16be) text(bytes)
            if (!read_i16be(data, len, 1, &x)) {
                DLOGW("DRAW_TEXT: missing x");
                return make_error(ctx, 0x40);
            }
            if (!read_i16be(data, len, 3, &y)) {
                DLOGW("DRAW_TEXT: missing y");
                return make_error(ctx, 0x41);
            }
            if (!read_u16be(data, len, 5, &color)) {
                DLOGW("DRAW_TEXT: missing color");
                return make_error(ctx, 0x42);
            }

            if (len < 10) {
                DLOGW("DRAW_TEXT: header too short (len=%u)", (unsigned) len);
                return make_error(ctx, 0x43);
            }

            size = data[7];

            if (!read_u16be(data, len, 8, &text_len)) {
                DLOGW("DRAW_TEXT: missing text_len");
                return make_error(ctx, 0x44);
            }

            const size_t text_off = 10;
            if (text_off + (size_t) text_len > len) {
                DLOGW("DRAW_TEXT: text_len=%u exceeds payload (len=%u)",
                    (unsigned) text_len, (unsigned) len);
                return make_error(ctx, 0x45);
            }

            const uint8_t *text = data + text_off;

            DLOGI("DRAW_TEXT: x=%d y=%d color=0x%04x size=%u text_len=%u",
                (int) x, (int) y, (unsigned) color, (unsigned) size, (unsigned) text_len);

            esp_err_t err = lgfx_device_draw_text(x, y, color, size, text, text_len);
            if (err != ESP_OK) {
                DLOGE("draw_text failed: %d", (int) err);
                return make_error(ctx, 0x46);
            }

            const uint8_t ok = 0x01;
            return make_ok_with_payload(ctx, &ok, 1);
        }

        default:
            DLOGW("unknown opcode: 0x%02x", (unsigned) opcode);
            return make_error(ctx, 0x12);
    }
}

static NativeHandlerResult lgfx_port_native_handler(Context *ctx)
{
    for (int i = 0; i < 4; i++) {
        term msg;

        if (!mailbox_peek(ctx, &msg)) {
            break;
        }

        GenMessage gen_message;
        enum GenMessageParseResult parse_result = port_parse_gen_message(msg, &gen_message);

        if (parse_result == GenCallMessage) {
            term reply = handle_call(ctx, gen_message.req);
            port_send_reply(ctx, gen_message.pid, gen_message.ref, reply);
        } else {
            DLOGW("native_handler: ignoring non-GenCall message (parse_result=%d)", (int) parse_result);
        }

        mailbox_remove_message(&ctx->mailbox, &ctx->heap);
    }

    return NativeContinue;
}

void lgfx_port_init(GlobalContext *global)
{
    (void) global;
    ESP_LOGI(TAG, "init");
    TRACE("lgfx_port_init\n");
    log_heap_stats("init");
}

void lgfx_port_destroy(GlobalContext *global)
{
    (void) global;
    ESP_LOGI(TAG, "destroy");
    TRACE("lgfx_port_destroy\n");
    log_heap_stats("destroy");
}

Context *lgfx_port_create_port(GlobalContext *global, term opts)
{
    (void) opts;

    ESP_LOGI(TAG, "create_port");
    log_heap_stats("create_port(before)");

    Context *ctx = context_new(global);
    if (!ctx) {
        ESP_LOGE(TAG, "create_port: context_new failed");
        return NULL;
    }

    ctx->native_handler = lgfx_port_native_handler;

    log_heap_stats("create_port(after)");
    return ctx;
}

REGISTER_PORT_DRIVER(
    lgfx_port,
    lgfx_port_init,
    lgfx_port_destroy,
    lgfx_port_create_port);
