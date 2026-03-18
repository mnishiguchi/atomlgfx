# SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
#
# SPDX-License-Identifier: Apache-2.0

defmodule LGFXPort.Images do
  @moduledoc false

  import LGFXPort.Guards

  alias LGFXPort.Protocol

  def draw_jpg(port, x, y, jpeg, target \\ 0)
      when i16(x) and i16(y) and is_binary(jpeg) and target_any(target) do
    with :ok <- validate_non_empty_jpeg(jpeg),
         :ok <- validate_binary_size_within_limit(port, jpeg, :draw_jpg) do
      Protocol.call_ok(port, :drawJpg, target, 0, [x, y, jpeg], Protocol.long_timeout())
    end
  end

  def draw_jpg(
        port,
        x,
        y,
        max_width,
        max_height,
        off_x,
        off_y,
        scale_x1024,
        scale_y1024,
        jpeg,
        target \\ 0
      )
      when i16(x) and i16(y) and
             u16(max_width) and
             u16(max_height) and
             i16(off_x) and i16(off_y) and
             i32(scale_x1024) and scale_x1024 > 0 and
             i32(scale_y1024) and scale_y1024 > 0 and
             is_binary(jpeg) and
             target_any(target) do
    with :ok <- validate_non_empty_jpeg(jpeg),
         :ok <- validate_binary_size_within_limit(port, jpeg, :draw_jpg) do
      Protocol.call_ok(
        port,
        :drawJpg,
        target,
        0,
        [x, y, max_width, max_height, off_x, off_y, scale_x1024, scale_y1024, jpeg],
        Protocol.long_timeout()
      )
    end
  end

  def draw_jpg_scaled(port, x, y, max_width, max_height, off_x, off_y, scale, jpeg, target \\ 0)
      when i16(x) and i16(y) and
             u16(max_width) and
             u16(max_height) and
             i16(off_x) and i16(off_y) and
             is_number(scale) and scale > 0 and
             is_binary(jpeg) and
             target_any(target) do
    draw_jpg_scaled(port, x, y, max_width, max_height, off_x, off_y, scale, scale, jpeg, target)
  end

  def draw_jpg_scaled(
        port,
        x,
        y,
        max_width,
        max_height,
        off_x,
        off_y,
        scale_x,
        scale_y,
        jpeg,
        target
      )
      when i16(x) and i16(y) and
             u16(max_width) and
             u16(max_height) and
             i16(off_x) and i16(off_y) and
             is_number(scale_x) and scale_x > 0 and
             is_number(scale_y) and scale_y > 0 and
             is_binary(jpeg) and
             target_any(target) do
    scale_x1024 = max(1, round(scale_x * 1024))
    scale_y1024 = max(1, round(scale_y * 1024))

    draw_jpg(
      port,
      x,
      y,
      max_width,
      max_height,
      off_x,
      off_y,
      scale_x1024,
      scale_y1024,
      jpeg,
      target
    )
  end

  def push_image_rgb565(port, x, y, width, height, pixels, stride_pixels \\ 0, target \\ 0)
      when i16(x) and i16(y) and
             u16(width) and
             u16(height) and
             is_binary(pixels) and
             u16(stride_pixels) and
             target_any(target) do
    with :ok <- validate_non_empty_image_dims(width, height),
         :ok <- validate_even_pixel_binary(pixels),
         {:ok, stride} <- normalize_stride_pixels(width, stride_pixels),
         :ok <- validate_pixel_binary_size(pixels, stride, height) do
      push_image_rgb565_transfer(
        port,
        x,
        y,
        width,
        height,
        pixels,
        stride_pixels,
        stride,
        target
      )
    else
      :skip -> :ok
      {:error, reason} -> {:error, reason}
    end
  end

  defp validate_non_empty_jpeg(<<>>), do: {:error, :empty_jpeg}
  defp validate_non_empty_jpeg(_jpeg), do: :ok

  defp validate_binary_size_within_limit(port, payload, op_name) when is_binary(payload) do
    payload_size = byte_size(payload)

    case Protocol.max_binary_bytes(port) do
      {:ok, max_binary_bytes} when is_integer(max_binary_bytes) and max_binary_bytes > 0 ->
        if payload_size <= max_binary_bytes do
          :ok
        else
          {:error, {:binary_too_large, op_name, payload_size, max_binary_bytes}}
        end

      _ ->
        :ok
    end
  end

  defp validate_non_empty_image_dims(width, height) when width == 0 or height == 0, do: :skip
  defp validate_non_empty_image_dims(_width, _height), do: :ok

  defp validate_even_pixel_binary(pixels) when rem(byte_size(pixels), 2) != 0 do
    {:error, {:pixels_size_not_even, byte_size(pixels)}}
  end

  defp validate_even_pixel_binary(_pixels), do: :ok

  defp normalize_stride_pixels(width, 0), do: {:ok, width}

  defp normalize_stride_pixels(width, stride) when stride < width do
    {:error, {:bad_stride, stride, width}}
  end

  defp normalize_stride_pixels(_width, stride), do: {:ok, stride}

  defp validate_pixel_binary_size(pixels, stride, height) do
    min_bytes = stride * height * 2

    if byte_size(pixels) < min_bytes do
      {:error, {:pixels_size_too_small, min_bytes, byte_size(pixels)}}
    else
      :ok
    end
  end

  defp push_image_rgb565_transfer(
         port,
         x,
         y,
         width,
         height,
         pixels,
         stride_pixels,
         stride,
         target
       ) do
    min_bytes = stride * height * 2

    case Protocol.max_binary_bytes(port) do
      {:ok, max_binary_bytes} when is_integer(max_binary_bytes) and max_binary_bytes > 0 ->
        if min_bytes <= max_binary_bytes do
          push_image_rgb565_raw(port, x, y, width, height, pixels, stride_pixels, target)
        else
          push_image_rgb565_chunked(
            port,
            x,
            y,
            width,
            height,
            pixels,
            stride,
            max_binary_bytes,
            target
          )
        end

      _ ->
        push_image_rgb565_raw(port, x, y, width, height, pixels, stride_pixels, target)
    end
  end

  defp push_image_rgb565_raw(port, x, y, width, height, pixels, stride_pixels, target) do
    Protocol.call_ok(
      port,
      :pushImage,
      target,
      0,
      [x, y, width, height, stride_pixels, pixels],
      Protocol.long_timeout()
    )
  end

  defp push_image_rgb565_chunked(
         port,
         x,
         y,
         width,
         height,
         pixels,
         stride,
         max_binary_bytes,
         target
       ) do
    row_bytes = width * 2
    stride_bytes = stride * 2

    if max_binary_bytes < row_bytes do
      {:error, {:push_image_max_binary_too_small, max_binary_bytes, row_bytes}}
    else
      rows_per_chunk = max(1, div(max_binary_bytes, row_bytes))
      do_push_chunks(port, x, y, width, height, pixels, stride_bytes, rows_per_chunk, target, 0)
    end
  end

  defp do_push_chunks(
         _port,
         _x,
         _y,
         _width,
         height,
         _pixels,
         _stride_bytes,
         _rows_per_chunk,
         _target,
         row
       )
       when row >= height,
       do: :ok

  defp do_push_chunks(
         port,
         x,
         y,
         width,
         height,
         pixels,
         stride_bytes,
         rows_per_chunk,
         target,
         row
       ) do
    chunk_height = min(height - row, rows_per_chunk)
    chunk = pack_rows(pixels, stride_bytes, width * 2, row, chunk_height)

    with :ok <- push_image_rgb565_raw(port, x, y + row, width, chunk_height, chunk, 0, target) do
      do_push_chunks(
        port,
        x,
        y,
        width,
        height,
        pixels,
        stride_bytes,
        rows_per_chunk,
        target,
        row + chunk_height
      )
    end
  end

  defp pack_rows(pixels, stride_bytes, row_bytes, row_start, row_count) do
    pack_rows_iolist(pixels, stride_bytes, row_bytes, row_start, row_start + row_count, [])
    |> :lists.reverse()
    |> :erlang.iolist_to_binary()
  end

  defp pack_rows_iolist(_pixels, _stride_bytes, _row_bytes, row, row_end, acc)
       when row >= row_end,
       do: acc

  defp pack_rows_iolist(pixels, stride_bytes, row_bytes, row, row_end, acc) do
    offset = row * stride_bytes
    part = :binary.part(pixels, offset, row_bytes)
    pack_rows_iolist(pixels, stride_bytes, row_bytes, row + 1, row_end, [part | acc])
  end
end
