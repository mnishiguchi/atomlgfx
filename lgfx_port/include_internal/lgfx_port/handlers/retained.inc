/*
 * SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
 *
 * SPDX-License-Identifier: Apache-2.0
 */

// lgfx_port/include_internal/lgfx_port/handlers/retained.inc

#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#include "lgfx_port/handler_decode.h"
#include "lgfx_port/proto_term.h"

typedef struct
{
    uint8_t layout_id;
    uint16_t capacity;
} lgfx_create_object_buffer_args_t;

typedef struct
{
    uint8_t handle;
    const uint8_t *records;
    size_t records_len;
} lgfx_write_object_buffer_args_t;

typedef struct
{
    uint8_t handle;
} lgfx_single_retained_handle_args_t;

typedef struct
{
    uint8_t type_id;
    uint8_t object_buffer_handle;
    const uint8_t *source_handles;
    size_t source_count;
    uint16_t strip_height;
    uint32_t background_color;
    uint8_t update_policy;
    bool has_transparent;
    uint16_t transparent_value;
    uint16_t zoom_min_x1024;
    uint16_t zoom_max_x1024;
} lgfx_create_render_program_args_t;

typedef struct
{
    uint8_t handle;
    uint8_t mode;
} lgfx_start_render_program_args_t;

static bool decode_create_object_buffer_args(
    const lgfx_request_t *req,
    lgfx_create_object_buffer_args_t *out)
{
    return out
        && lgfx_decode_u8_at(req, 5, &out->layout_id)
        && lgfx_decode_u16_at(req, 6, &out->capacity)
        && out->capacity > 0u;
}

static bool decode_write_object_buffer_args(
    const lgfx_request_t *req,
    lgfx_write_object_buffer_args_t *out)
{
    if (!out || !lgfx_decode_u8_at(req, 5, &out->handle)) {
        return false;
    }

    return lgfx_decode_binary_at(req, 6, &out->records, &out->records_len);
}

static bool decode_single_retained_handle_args(
    const lgfx_request_t *req,
    lgfx_single_retained_handle_args_t *out)
{
    return out && lgfx_decode_u8_at(req, 5, &out->handle);
}

static bool decode_create_render_program_args(
    const lgfx_request_t *req,
    const lgfx_port_t *port,
    lgfx_create_render_program_args_t *out)
{
    term has_transparent_term = term_invalid_term();

    if (!out
        || !port
        || !lgfx_decode_u8_at(req, 5, &out->type_id)
        || !lgfx_decode_u8_at(req, 6, &out->object_buffer_handle)
        || !lgfx_decode_binary_at(req, 7, &out->source_handles, &out->source_count)
        || !lgfx_decode_u16_at(req, 8, &out->strip_height)
        || !lgfx_decode_u32_at(req, 9, &out->background_color)
        || !lgfx_decode_u8_at(req, 10, &out->update_policy)) {
        return false;
    }

    has_transparent_term = lgfx_req_elem(req, 11);
    if (!lgfx_decode_bool_term(port, has_transparent_term, &out->has_transparent)
        || !lgfx_decode_u16_at(req, 12, &out->transparent_value)
        || !lgfx_decode_u16_at(req, 13, &out->zoom_min_x1024)
        || !lgfx_decode_u16_at(req, 14, &out->zoom_max_x1024)) {
        return false;
    }

    return out->source_count > 0u;
}

static bool decode_start_render_program_args(
    const lgfx_request_t *req,
    lgfx_start_render_program_args_t *out)
{
    return out
        && lgfx_decode_u8_at(req, 5, &out->handle)
        && lgfx_decode_u8_at(req, 6, &out->mode);
}

static term retained_reply_from_esp_err(
    Context *ctx,
    lgfx_port_t *port,
    const lgfx_request_t *req,
    esp_err_t err,
    term invalid_state_reason)
{
    switch (err) {
        case ESP_OK:
            return term_invalid_term();
        case ESP_ERR_INVALID_ARG:
        case ESP_ERR_NOT_FOUND:
            return reply_error(ctx, port, req, port->atoms.bad_args, (int32_t) err);
        case ESP_ERR_NO_MEM:
            return reply_error(ctx, port, req, port->atoms.no_memory, (int32_t) err);
        case ESP_ERR_NOT_SUPPORTED:
            return reply_error(ctx, port, req, port->atoms.unsupported, (int32_t) err);
        case ESP_ERR_INVALID_STATE:
            return reply_error(ctx, port, req, invalid_state_reason, (int32_t) err);
        default:
            return lgfx_reply_from_esp_err_req(ctx, port, req, err);
    }
}

term lgfx_handle_createObjectBuffer(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    lgfx_create_object_buffer_args_t args = { 0 };
    uint8_t handle = 0u;

    if (!decode_create_object_buffer_args(req, &args)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }

    const esp_err_t err = lgfx_device_retained_object_buffer_create(
        args.layout_id,
        args.capacity,
        &handle);

    if (err != ESP_OK) {
        return retained_reply_from_esp_err(ctx, port, req, err, port->atoms.internal);
    }

    return reply_ok(ctx, port, req, term_from_int32((int32_t) handle));
}

term lgfx_handle_writeObjectBuffer(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    lgfx_write_object_buffer_args_t args = { 0 };

    if (!decode_write_object_buffer_args(req, &args)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }

    const esp_err_t err = lgfx_device_retained_object_buffer_write(
        args.handle,
        args.records,
        args.records_len);

    if (err != ESP_OK) {
        return retained_reply_from_esp_err(ctx, port, req, err, port->atoms.internal);
    }

    return reply_ok(ctx, port, req, port->atoms.ok);
}

term lgfx_handle_deleteObjectBuffer(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    lgfx_single_retained_handle_args_t args = { 0 };

    if (!decode_single_retained_handle_args(req, &args)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }

    const esp_err_t err = lgfx_device_retained_object_buffer_delete(args.handle);
    if (err != ESP_OK) {
        return retained_reply_from_esp_err(
            ctx,
            port,
            req,
            err,
            port->atoms.resource_busy);
    }

    return reply_ok(ctx, port, req, port->atoms.ok);
}

term lgfx_handle_createRenderProgram(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    lgfx_create_render_program_args_t args = { 0 };
    uint8_t handle = 0u;

    if (!decode_create_render_program_args(req, port, &args)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }

    const esp_err_t err = lgfx_device_retained_render_program_create(
        args.type_id,
        args.object_buffer_handle,
        args.source_handles,
        args.source_count,
        args.strip_height,
        args.background_color,
        args.has_transparent,
        args.transparent_value,
        args.update_policy,
        args.zoom_min_x1024,
        args.zoom_max_x1024,
        &handle);

    if (err != ESP_OK) {
        return retained_reply_from_esp_err(ctx, port, req, err, port->atoms.internal);
    }

    return reply_ok(ctx, port, req, term_from_int32((int32_t) handle));
}

term lgfx_handle_startRenderProgram(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    lgfx_start_render_program_args_t args = { 0 };

    if (!decode_start_render_program_args(req, &args)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }

    const esp_err_t err = lgfx_device_retained_render_program_start(args.handle, args.mode);
    if (err != ESP_OK) {
        return retained_reply_from_esp_err(
            ctx,
            port,
            req,
            err,
            port->atoms.renderer_running);
    }

    return reply_ok(ctx, port, req, port->atoms.ok);
}

term lgfx_handle_stopRenderProgram(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    lgfx_single_retained_handle_args_t args = { 0 };

    if (!decode_single_retained_handle_args(req, &args)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }

    const esp_err_t err = lgfx_device_retained_render_program_stop(args.handle);
    if (err != ESP_OK) {
        return retained_reply_from_esp_err(ctx, port, req, err, port->atoms.internal);
    }

    return reply_ok(ctx, port, req, port->atoms.ok);
}

term lgfx_handle_getRenderProgramStats(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    lgfx_single_retained_handle_args_t args = { 0 };
    lgfx_retained_render_program_stats_t stats = {};

    if (!decode_single_retained_handle_args(req, &args)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }

    const esp_err_t err = lgfx_device_retained_render_program_stats(args.handle, &stats);
    if (err != ESP_OK) {
        return retained_reply_from_esp_err(ctx, port, req, err, port->atoms.internal);
    }

    term elems[11];
    elems[0] = port->atoms.render_program_stats;
    elems[1] = stats.running ? port->atoms.true_ : port->atoms.false_;
    elems[2] = term_from_int32((int32_t) stats.frame_count);
    elems[3] = term_from_int32((int32_t) stats.object_count);
    elems[4] = term_from_int32((int32_t) stats.drawn_count);
    elems[5] = term_from_int32((int32_t) stats.culled_count);
    elems[6] = term_from_int32((int32_t) stats.strip_height);
    elems[7] = term_from_int32((int32_t) stats.last_frame_us);
    elems[8] = term_from_int32((int32_t) stats.last_update_us);
    elems[9] = term_from_int32((int32_t) stats.last_draw_us);
    elems[10] = term_from_int32((int32_t) stats.last_present_us);

    term payload = lgfx_make_tuple(ctx, 11, elems);
    if (term_is_invalid_term(payload)) {
        return reply_error(ctx, port, req, port->atoms.no_memory, (int32_t) ESP_ERR_NO_MEM);
    }

    return reply_ok(ctx, port, req, payload);
}

term lgfx_handle_destroyRenderProgram(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    lgfx_single_retained_handle_args_t args = { 0 };

    if (!decode_single_retained_handle_args(req, &args)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }

    const esp_err_t err = lgfx_device_retained_render_program_destroy(args.handle);
    if (err != ESP_OK) {
        return retained_reply_from_esp_err(ctx, port, req, err, port->atoms.internal);
    }

    return reply_ok(ctx, port, req, port->atoms.ok);
}
