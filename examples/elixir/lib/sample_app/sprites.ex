# SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
#
# SPDX-License-Identifier: Apache-2.0

defmodule SampleApp.Sprites do
  @moduledoc false

  @rgb565_sprite_target 30
  @palette_sprite_target 31

  def run(handle, width, height)
      when is_integer(width) and width > 0 and is_integer(height) and height > 0 do
    try do
      with {:ok, true} <- AtomLGFX.supports_sprite?(handle),
           :ok <- draw_rgb565_sprite(handle, width, height),
           :ok <- safe_delete_sprite(handle, @rgb565_sprite_target),
           :ok <- maybe_draw_palette_sprite(handle, width, height) do
        IO.puts("sprites ok")
        :ok
      else
        {:ok, false} ->
          {:error, :cap_sprite_missing}

        {:error, reason} = err ->
          IO.puts("sprites failed: #{AtomLGFX.format_error(reason)}")
          err
      end
    after
      cleanup(handle)
    end
  end

  defp draw_rgb565_sprite(handle, width, height) do
    sprite_width = clamp_int(div(width, 2), 96, 160)
    sprite_height = clamp_int(div(height, 3), 56, 96)
    sprite_x = div(width - sprite_width, 2)
    sprite_y = max(36, div(height - sprite_height, 2))

    with :ok <- safe_delete_sprite(handle, @rgb565_sprite_target),
         :ok <-
           AtomLGFX.create_sprite(handle, sprite_width, sprite_height, 16, @rgb565_sprite_target),
         :ok <-
           AtomLGFX.render_sprite(
             handle,
             @rgb565_sprite_target,
             [
               {:clear, :navy},
               {:draw_rect, 0, 0, sprite_width, sprite_height, :white},
               {:fill_round_rect, 8, 8, sprite_width - 16, sprite_height - 16, 8,
                {:rgb, 0, 80, 180}},
               {:fill_circle, div(sprite_width, 2), div(sprite_height, 2),
                max(8, div(sprite_height, 4)), :orange},
               {:set_text_font_preset, :ascii},
               {:set_text_color, :white},
               {:set_cursor, 10, 10},
               {:println, "RGB565 sprite"}
             ],
             validate: true
           ),
         :ok <-
           AtomLGFX.render_lcd(
             handle,
             [
               {:fill_screen, :black},
               {:set_text_font_preset, :ascii},
               {:set_text_color, :white},
               {:set_cursor, 8, 8},
               {:println, "Render-first sprites"},
               {:push_sprite, @rgb565_sprite_target, sprite_x, sprite_y},
               {:draw_rect, sprite_x - 1, sprite_y - 1, sprite_width + 2, sprite_height + 2,
                :dark_grey},
               :display
             ],
             validate: true
           ) do
      :ok
    end
  end

  defp maybe_draw_palette_sprite(handle, width, height) do
    case AtomLGFX.supports_palette?(handle) do
      {:ok, true} -> draw_palette_sprite(handle, width, height)
      {:ok, false} -> :ok
      {:error, reason} -> {:error, reason}
    end
  end

  defp draw_palette_sprite(handle, width, height) do
    sprite_width = clamp_int(div(width, 3), 72, 120)
    sprite_height = clamp_int(div(height, 4), 44, 72)
    sprite_x = max(8, width - sprite_width - 10)
    sprite_y = max(56, height - sprite_height - 10)

    with :ok <- safe_delete_sprite(handle, @palette_sprite_target),
         :ok <-
           AtomLGFX.create_sprite(handle, sprite_width, sprite_height, 4, @palette_sprite_target),
         :ok <- AtomLGFX.create_palette(handle, @palette_sprite_target),
         :ok <-
           AtomLGFX.render_sprite(
             handle,
             @palette_sprite_target,
             [
               {:set_palette_color, 0, :black},
               {:set_palette_color, 1, :cyan},
               {:set_palette_color, 2, :yellow},
               {:set_palette_color, 3, {:rgb, 255, 64, 128}},
               {:clear, {:index, 0}},
               {:fill_rect, 0, 0, sprite_width, sprite_height, {:index, 1}},
               {:draw_rect, 0, 0, sprite_width, sprite_height, {:index, 2}},
               {:fill_circle, div(sprite_width, 2), div(sprite_height, 2),
                max(6, div(sprite_height, 4)), {:index, 3}},
               {:set_text_color, {:index, 2}, {:index, 1}},
               {:set_cursor, 6, 6},
               {:println, "PAL"}
             ],
             validate: true
           ),
         :ok <-
           AtomLGFX.render_lcd(
             handle,
             [
               {:push_sprite, @palette_sprite_target, sprite_x, sprite_y, {:index, 0}},
               :display
             ],
             validate: true
           ) do
      :ok
    end
  end

  defp cleanup(handle) do
    _ = AtomLGFX.delete_sprite(handle, @rgb565_sprite_target)
    _ = AtomLGFX.delete_sprite(handle, @palette_sprite_target)
    :ok
  end

  defp safe_delete_sprite(handle, target) do
    case AtomLGFX.delete_sprite(handle, target) do
      :ok -> :ok
      {:error, _reason} -> :ok
    end
  end

  defp clamp_int(value, min_value, max_value) when is_integer(value) do
    value
    |> max(min_value)
    |> min(max_value)
  end
end
