defmodule SampleApp.ClipSmoke do
  @moduledoc false

  @bg 0x000000
  @fg 0xFFFFFF
  @dim 0x909090
  @frame 0x404040
  @lcd_fill 0x103060
  @lcd_accent 0xFFD040
  @sprite_fill 0x204010
  @sprite_frame 0x80FF80

  @sprite_target 20

  def run(port, w, h) when is_integer(w) and w > 0 and is_integer(h) and h > 0 do
    {clip_x, clip_y, clip_w, clip_h} = lcd_clip_rect(w, h)

    try do
      with :ok <- draw_lcd_clip_smoke(port, w, h, clip_x, clip_y, clip_w, clip_h),
           :ok <- maybe_draw_sprite_clip_smoke(port, w, h) do
        IO.puts("clip_smoke ok")
        :ok
      else
        {:error, reason} = err ->
          IO.puts("clip_smoke failed: #{LGFXPort.format_error(reason)}")
          err
      end
    after
      _ = LGFXPort.clear_clip_rect(port, 0)
      _ = safe_delete_sprite(port, @sprite_target)
    end
  end

  defp draw_lcd_clip_smoke(port, w, h, clip_x, clip_y, clip_w, clip_h) do
    status_y = max_i(0, h - 14)
    cross_y = clip_y + div(clip_h, 2)
    cross_x = clip_x + div(clip_w, 2)

    with :ok <- LGFXPort.clear_clip_rect(port, 0),
         :ok <- LGFXPort.fill_screen(port, @bg),
         :ok <- LGFXPort.draw_rect(port, 0, 0, w, h, @frame, 0),
         :ok <- LGFXPort.draw_string_bg(port, 4, 2, @fg, @bg, 1, "CLIP SMOKE", 0),
         :ok <- LGFXPort.set_text_color(port, @dim, nil, 0),
         :ok <- LGFXPort.draw_string(port, 4, 14, "LCD target clip rect", 0),
         :ok <- LGFXPort.draw_rect(port, clip_x, clip_y, clip_w, clip_h, @frame, 0),
         :ok <- LGFXPort.set_clip_rect(port, clip_x, clip_y, clip_w, clip_h, 0),
         :ok <- LGFXPort.fill_rect(port, 0, 0, w, h, @lcd_fill, 0),
         :ok <- LGFXPort.draw_fast_hline(port, 0, cross_y, w, @lcd_accent, 0),
         :ok <- LGFXPort.draw_fast_vline(port, cross_x, 0, h, @lcd_accent, 0),
         :ok <- LGFXPort.set_text_color(port, @fg, nil, 0),
         :ok <- LGFXPort.draw_string(port, clip_x + 4, clip_y + 4, "lcd clipped", 0),
         :ok <- LGFXPort.clear_clip_rect(port, 0),
         :ok <- LGFXPort.draw_rect(port, clip_x, clip_y, clip_w, clip_h, @lcd_accent, 0),
         :ok <- LGFXPort.set_text_color(port, @dim, nil, 0),
         :ok <- LGFXPort.draw_string(port, 4, status_y, "lcd clip set/clear ok", 0) do
      :ok
    end
  end

  defp maybe_draw_sprite_clip_smoke(port, w, h) do
    case LGFXPort.supports_sprite?(port) do
      {:ok, true} ->
        draw_sprite_clip_smoke(port, w, h)

      {:ok, false} ->
        IO.puts("clip_smoke sprite path skipped: sprite unsupported")
        :ok

      {:error, reason} ->
        {:error, reason}
    end
  end

  defp draw_sprite_clip_smoke(port, w, h) do
    sprite_w = max_i(32, min_i(80, div(w, 3)))
    sprite_h = max_i(24, min_i(48, div(h, 4)))

    dst_x = max_i(4, w - sprite_w - 4)
    dst_y = max_i(24, h - sprite_h - 4)

    clip_x = 6
    clip_y = 6
    clip_w = max_i(8, sprite_w - 12)
    clip_h = max_i(8, sprite_h - 12)

    with :ok <- safe_delete_sprite(port, @sprite_target),
         :ok <- LGFXPort.create_sprite(port, sprite_w, sprite_h, 16, @sprite_target),
         :ok <- LGFXPort.clear(port, @bg, @sprite_target),
         :ok <- LGFXPort.draw_rect(port, 0, 0, sprite_w, sprite_h, @frame, @sprite_target),
         :ok <- LGFXPort.set_clip_rect(port, clip_x, clip_y, clip_w, clip_h, @sprite_target),
         :ok <- LGFXPort.fill_rect(port, 0, 0, sprite_w, sprite_h, @sprite_fill, @sprite_target),
         :ok <-
           LGFXPort.draw_fast_hline(
             port,
             0,
             clip_y + div(clip_h, 2),
             sprite_w,
             @sprite_frame,
             @sprite_target
           ),
         :ok <-
           LGFXPort.draw_fast_vline(
             port,
             clip_x + div(clip_w, 2),
             0,
             sprite_h,
             @sprite_frame,
             @sprite_target
           ),
         :ok <- LGFXPort.clear_clip_rect(port, @sprite_target),
         :ok <-
           LGFXPort.draw_rect(
             port,
             clip_x,
             clip_y,
             clip_w,
             clip_h,
             @sprite_frame,
             @sprite_target
           ),
         :ok <- LGFXPort.push_sprite(port, @sprite_target, dst_x, dst_y),
         :ok <- LGFXPort.set_text_color(port, @dim, nil, 0),
         :ok <- LGFXPort.draw_string(port, 4, max_i(0, dst_y - 12), "sprite target clip rect", 0) do
      :ok
    else
      {:error, reason} ->
        {:error, reason}
    end
  end

  defp safe_delete_sprite(port, sprite_target) do
    case LGFXPort.delete_sprite(port, sprite_target) do
      :ok -> :ok
      {:error, _} -> :ok
    end
  end

  defp lcd_clip_rect(w, h) do
    clip_x = max_i(8, div(w, 8))
    clip_y = max_i(24, div(h, 6))
    clip_w = max_i(16, w - clip_x * 2)
    clip_h = max_i(16, h - clip_y * 2)
    {clip_x, clip_y, clip_w, clip_h}
  end

  defp min_i(a, b) when a <= b, do: a
  defp min_i(_a, b), do: b

  defp max_i(a, b) when a >= b, do: a
  defp max_i(_a, b), do: b
end
