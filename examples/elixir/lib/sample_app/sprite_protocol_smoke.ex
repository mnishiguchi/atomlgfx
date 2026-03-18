# SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
#
# SPDX-License-Identifier: Apache-2.0

defmodule SampleApp.SpriteProtocolSmoke do
  @moduledoc false

  import Bitwise

  @t_short 5_000

  @proto_ver 1
  @cap_sprite 1 <<< 0
  @cap_palette 1 <<< 4

  @f_color_index 1 <<< 1
  @f_transparent_index 1 <<< 4

  @sprite_target 1
  @sprite_w 8
  @sprite_h 8
  @palette_sprite_depth 8

  @zoom_1x 1024

  # pushSprite wire format (protocol v1, destination-aware):
  # - Non-transparent: [dst_target, x, y]
  # - Transparent:     [dst_target, x, y, transparent_value]
  #
  # pushRotateZoom wire format (protocol v1, destination-aware):
  # - Non-transparent: [dst_target, x, y, angle_cdeg, zoom_x1024, zoom_y1024]
  # - Transparent:     [dst_target, x, y, angle_cdeg, zoom_x1024, zoom_y1024, transparent_value]
  #
  # Where:
  # - src sprite handle is the request header Target (1..254)
  # - dst_target: 0 (LCD) or 1..254 (sprite)
  # - angle_cdeg: centi-degrees (0.01 deg). Example: 90deg => 9000
  # - zoom_x1024: 1024 = 1.0x
  # - transparent_value is RGB565 by default, or a palette index when the
  #   transparent-index flag is set
  #
  # This smoke test assumes the port is already initialized (LGFXPort.init/1 + LGFXPort.display/1).
  # In SampleApp, run it after boot_for_display_with_dims/1.
  def run(port), do: run(port, &LGFXPort.raw_call/6)

  def run(port, raw_call) when is_function(raw_call, 6) do
    reset_note_once_flags()

    with {:ok, caps} <- check_get_caps_sprite_capacity(port, raw_call),
         :ok <- check_sprite_only_ops_reject_target_zero(port, raw_call),
         :ok <- check_sprite_lifecycle_and_blits(port, raw_call),
         :ok <- maybe_check_palette_lifecycle_and_indexed_blits(port, raw_call, caps.feature_bits) do
      IO.puts("sprite protocol smoke ok")
      :ok
    else
      {:error, reason} = err ->
        IO.puts("sprite protocol smoke failed: #{inspect(reason)}")
        err
    end
  end

  # -----------------------------------------------------------------------------
  # 0) Basic capability sanity
  # -----------------------------------------------------------------------------
  defp check_get_caps_sprite_capacity(port, raw_call) do
    case raw_call.(port, :getCaps, 0, 0, [], @t_short) do
      {:ok, {:caps, proto_ver, _max_binary_bytes, max_sprites, feature_bits}}
      when is_integer(proto_ver) and is_integer(max_sprites) and is_integer(feature_bits) ->
        cond do
          proto_ver != @proto_ver ->
            {:error, {:proto_ver_mismatch, @proto_ver, proto_ver}}

          (feature_bits &&& @cap_sprite) == 0 ->
            {:error, {:cap_sprite_missing, feature_bits}}

          max_sprites <= 0 ->
            {:error, {:sprite_capacity_not_available, max_sprites}}

          true ->
            {:ok,
             %{
               proto_ver: proto_ver,
               max_sprites: max_sprites,
               feature_bits: feature_bits
             }}
        end

      {:ok, payload} ->
        {:error, {:bad_caps_payload, payload}}

      {:error, reason} ->
        {:error, {:get_caps_failed, reason}}
    end
  end

  # -----------------------------------------------------------------------------
  # 1) Sprite-only ops must reject target=0 (LCD target)
  # -----------------------------------------------------------------------------
  defp check_sprite_only_ops_reject_target_zero(port, raw_call) do
    with :ok <- check_create_sprite_target_zero_bad_target(port, raw_call),
         :ok <- check_push_sprite_target_zero_bad_target(port, raw_call),
         :ok <- check_push_rotate_zoom_target_zero_bad_target(port, raw_call) do
      :ok
    end
  end

  defp check_create_sprite_target_zero_bad_target(port, raw_call) do
    case raw_call.(port, :createSprite, 0, 0, [@sprite_w, @sprite_h], @t_short) do
      {:error, :bad_target} -> :ok
      {:error, reason} -> {:error, {:createSprite_target0_expected_bad_target, reason}}
      {:ok, result} -> {:error, {:createSprite_target0_unexpected_ok, result}}
    end
  end

  defp check_push_sprite_target_zero_bad_target(port, raw_call) do
    # pushSprite is sprite-only: header target must be a sprite handle (1..254).
    # args: [dst_target, x, y]
    args = [0, 0, 0]

    case raw_call.(port, :pushSprite, 0, 0, args, @t_short) do
      {:error, :bad_target} -> :ok
      {:error, reason} -> {:error, {:pushSprite_target0_expected_bad_target, reason}}
      {:ok, result} -> {:error, {:pushSprite_target0_unexpected_ok, result}}
    end
  end

  defp check_push_rotate_zoom_target_zero_bad_target(port, raw_call) do
    # pushRotateZoom is sprite-only: header target must be a sprite handle (1..254).
    # args: [dst_target, x, y, angle_cdeg, zoom_x1024, zoom_y1024]
    args = [0, 0, 0, 0, @zoom_1x, @zoom_1x]

    case raw_call.(port, :pushRotateZoom, 0, 0, args, @t_short) do
      {:error, :bad_target} ->
        :ok

      {:error, :bad_op} ->
        note_once(
          :push_rotate_zoom_unavailable,
          "sprite protocol smoke note: pushRotateZoom not available; skipping pushRotateZoom checks"
        )

        :ok

      {:error, :unsupported} ->
        note_once(
          :push_rotate_zoom_unavailable,
          "sprite protocol smoke note: pushRotateZoom unsupported; skipping pushRotateZoom checks"
        )

        :ok

      {:error, reason} ->
        {:error, {:pushRotateZoom_target0_expected_bad_target, reason}}

      {:ok, result} ->
        {:error, {:pushRotateZoom_target0_unexpected_ok, result}}
    end
  end

  # -----------------------------------------------------------------------------
  # 2) Create -> draw -> blit -> rotate/zoom -> delete
  # -----------------------------------------------------------------------------
  defp check_sprite_lifecycle_and_blits(port, raw_call) do
    with :ok <- check_create_sprite(port, raw_call, @sprite_target),
         :ok <- check_sprite_dimensions(port, raw_call, @sprite_target),
         :ok <- check_draw_into_sprite(port, raw_call, @sprite_target),
         :ok <- check_set_pivot(port, raw_call, @sprite_target),
         :ok <- check_push_sprite_to_lcd(port, raw_call, @sprite_target),
         :ok <- check_push_rotate_zoom_to_lcd(port, raw_call, @sprite_target),
         :ok <- check_delete_sprite(port, raw_call, @sprite_target) do
      :ok
    else
      {:error, _reason} = err ->
        _ = maybe_cleanup_sprite(port, raw_call, @sprite_target)
        err
    end
  end

  defp check_create_sprite(port, raw_call, sprite_target) do
    case raw_call.(port, :createSprite, sprite_target, 0, [@sprite_w, @sprite_h], @t_short) do
      {:ok, _result} -> :ok
      {:error, reason} -> {:error, {:createSprite_failed, reason}}
    end
  end

  defp check_create_sprite_with_depth(port, raw_call, sprite_target, color_depth) do
    case raw_call.(
           port,
           :createSprite,
           sprite_target,
           0,
           [@sprite_w, @sprite_h, color_depth],
           @t_short
         ) do
      {:ok, _result} ->
        :ok

      {:error, reason} ->
        {:error, {:createSprite_with_depth_failed, sprite_target, color_depth, reason}}
    end
  end

  defp check_sprite_dimensions(port, raw_call, sprite_target) do
    # Some driver builds do not expose width/height for sprite targets yet.
    # If unsupported, do not fail the smoke.
    case raw_call.(port, :width, sprite_target, 0, [], @t_short) do
      {:error, :unsupported} ->
        :ok

      {:error, reason} ->
        {:error, {:sprite_dimensions_failed, reason}}

      {:ok, w} when is_integer(w) and w > 0 ->
        case raw_call.(port, :height, sprite_target, 0, [], @t_short) do
          {:error, :unsupported} ->
            :ok

          {:error, reason} ->
            {:error, {:sprite_dimensions_failed, reason}}

          {:ok, h} when is_integer(h) and h > 0 ->
            :ok

          {:ok, payload} ->
            {:error, {:sprite_dimensions_bad_payload, payload}}
        end

      {:ok, payload} ->
        {:error, {:sprite_dimensions_bad_payload, payload}}
    end
  end

  defp check_draw_into_sprite(port, raw_call, sprite_target) do
    # Use a few target-aware primitives to ensure the sprite accepts drawing.
    with {:ok, _} <- raw_call.(port, :clear, sprite_target, 0, [0x000000], @t_short),
         {:ok, _} <-
           raw_call.(
             port,
             :fillRect,
             sprite_target,
             0,
             [0, 0, @sprite_w, @sprite_h, 0x002244],
             @t_short
           ),
         {:ok, _} <-
           raw_call.(
             port,
             :drawRect,
             sprite_target,
             0,
             [0, 0, @sprite_w, @sprite_h, 0xFFFFFF],
             @t_short
           ),
         {:ok, _} <- raw_call.(port, :drawPixel, sprite_target, 0, [1, 1, 0xFF0000], @t_short) do
      :ok
    else
      {:error, reason} ->
        {:error, {:draw_into_sprite_failed, reason}}

      {:ok, payload} ->
        {:error, {:draw_into_sprite_bad_payload, payload}}
    end
  end

  defp check_set_pivot(port, raw_call, sprite_target) do
    case raw_call.(
           port,
           :setPivot,
           sprite_target,
           0,
           [div(@sprite_w, 2), div(@sprite_h, 2)],
           @t_short
         ) do
      {:ok, _result} -> :ok
      {:error, reason} -> {:error, {:setPivot_failed, reason}}
    end
  end

  defp check_push_sprite_to_lcd(port, raw_call, sprite_target) do
    # Sprite -> LCD blit:
    # header Target = src sprite handle
    # args = [dst_target, x, y] where dst_target=0 => LCD
    case raw_call.(port, :pushSprite, sprite_target, 0, [0, 4, 4], @t_short) do
      {:ok, _result} -> :ok
      {:error, reason} -> {:error, {:pushSprite_failed, reason}}
    end
  end

  defp check_push_rotate_zoom_to_lcd(port, raw_call, sprite_target) do
    # args = [dst_target, x, y, angle_cdeg, zoom_x1024, zoom_y1024] with dst_target=0 => LCD
    args = [0, 28, 4, 0, @zoom_1x, @zoom_1x]

    case raw_call.(port, :pushRotateZoom, sprite_target, 0, args, @t_short) do
      {:error, :bad_op} ->
        note_once(
          :push_rotate_zoom_unavailable,
          "sprite protocol smoke note: pushRotateZoom not available; skipping pushRotateZoom checks"
        )

        :ok

      {:error, :unsupported} ->
        note_once(
          :push_rotate_zoom_unavailable,
          "sprite protocol smoke note: pushRotateZoom unsupported; skipping pushRotateZoom checks"
        )

        :ok

      {:ok, _result} ->
        :ok

      {:error, reason} ->
        {:error, {:pushRotateZoom_failed, reason}}
    end
  end

  defp check_delete_sprite(port, raw_call, sprite_target) do
    case raw_call.(port, :deleteSprite, sprite_target, 0, [], @t_short) do
      {:ok, _result} -> :ok
      {:error, reason} -> {:error, {:deleteSprite_failed, reason}}
    end
  end

  # -----------------------------------------------------------------------------
  # 3) Palette + indexed-color sprite path
  # -----------------------------------------------------------------------------
  defp maybe_check_palette_lifecycle_and_indexed_blits(port, raw_call, feature_bits) do
    if cap_set?(feature_bits, @cap_palette) do
      check_palette_lifecycle_and_indexed_blits(port, raw_call)
    else
      note_once(
        :palette_unavailable,
        "sprite protocol smoke note: palette capability missing; skipping palette/indexed-color checks"
      )

      :ok
    end
  end

  defp check_palette_lifecycle_and_indexed_blits(port, raw_call) do
    with :ok <-
           check_create_sprite_with_depth(port, raw_call, @sprite_target, @palette_sprite_depth),
         :ok <- check_create_palette(port, raw_call, @sprite_target),
         :ok <- check_set_palette_color(port, raw_call, @sprite_target, 0, 0x000000),
         :ok <- check_set_palette_color(port, raw_call, @sprite_target, 1, 0x00FF00),
         :ok <- check_set_pivot(port, raw_call, @sprite_target),
         :ok <- check_indexed_draw_into_sprite(port, raw_call, @sprite_target),
         :ok <- check_indexed_push_sprite_to_lcd(port, raw_call, @sprite_target),
         :ok <- check_indexed_push_rotate_zoom_to_lcd(port, raw_call, @sprite_target),
         :ok <- check_delete_sprite(port, raw_call, @sprite_target) do
      :ok
    else
      {:error, _reason} = err ->
        _ = maybe_cleanup_sprite(port, raw_call, @sprite_target)
        err
    end
  end

  defp check_create_palette(port, raw_call, sprite_target) do
    case raw_call.(port, :createPalette, sprite_target, 0, [], @t_short) do
      {:ok, _result} -> :ok
      {:error, reason} -> {:error, {:createPalette_failed, reason}}
    end
  end

  defp check_set_palette_color(port, raw_call, sprite_target, palette_index, rgb888) do
    case raw_call.(
           port,
           :setPaletteColor,
           sprite_target,
           0,
           [palette_index, rgb888],
           @t_short
         ) do
      {:ok, _result} -> :ok
      {:error, reason} -> {:error, {:setPaletteColor_failed, palette_index, rgb888, reason}}
    end
  end

  defp check_indexed_draw_into_sprite(port, raw_call, sprite_target) do
    with {:ok, _} <- raw_call.(port, :clear, sprite_target, @f_color_index, [0], @t_short),
         {:ok, _} <-
           raw_call.(
             port,
             :fillRect,
             sprite_target,
             @f_color_index,
             [2, 2, 4, 4, 1],
             @t_short
           ),
         {:ok, _} <-
           raw_call.(port, :drawPixel, sprite_target, @f_color_index, [1, 1, 1], @t_short) do
      :ok
    else
      {:error, reason} ->
        {:error, {:indexed_draw_into_sprite_failed, reason}}

      {:ok, payload} ->
        {:error, {:indexed_draw_into_sprite_bad_payload, payload}}
    end
  end

  defp check_indexed_push_sprite_to_lcd(port, raw_call, sprite_target) do
    # Transparent value is palette index 0 because the transparent-index flag is set.
    case raw_call.(
           port,
           :pushSprite,
           sprite_target,
           @f_transparent_index,
           [0, 4, 20, 0],
           @t_short
         ) do
      {:ok, _result} -> :ok
      {:error, reason} -> {:error, {:pushSprite_indexed_transparent_failed, reason}}
    end
  end

  defp check_indexed_push_rotate_zoom_to_lcd(port, raw_call, sprite_target) do
    args = [0, 28, 20, 0, @zoom_1x, @zoom_1x, 0]

    case raw_call.(port, :pushRotateZoom, sprite_target, @f_transparent_index, args, @t_short) do
      {:error, :bad_op} ->
        note_once(
          :push_rotate_zoom_unavailable,
          "sprite protocol smoke note: pushRotateZoom not available; skipping pushRotateZoom checks"
        )

        :ok

      {:error, :unsupported} ->
        note_once(
          :push_rotate_zoom_unavailable,
          "sprite protocol smoke note: pushRotateZoom unsupported; skipping pushRotateZoom checks"
        )

        :ok

      {:ok, _result} ->
        :ok

      {:error, reason} ->
        {:error, {:pushRotateZoom_indexed_transparent_failed, reason}}
    end
  end

  defp maybe_cleanup_sprite(port, raw_call, sprite_target) do
    case raw_call.(port, :deleteSprite, sprite_target, 0, [], @t_short) do
      {:ok, _} -> :ok
      {:error, _} -> :ok
    end
  end

  # -----------------------------------------------------------------------------
  # One-time notes (avoid duplicate log lines in a single smoke run)
  # -----------------------------------------------------------------------------
  defp reset_note_once_flags do
    :erlang.erase({__MODULE__, :note_once, :push_rotate_zoom_unavailable})
    :erlang.erase({__MODULE__, :note_once, :palette_unavailable})
    :ok
  end

  defp note_once(tag, message) when is_atom(tag) and is_binary(message) do
    key = {__MODULE__, :note_once, tag}

    case :erlang.get(key) do
      true ->
        :ok

      _ ->
        IO.puts(message)
        :erlang.put(key, true)
        :ok
    end
  end

  defp cap_set?(feature_bits, cap_bit) do
    (feature_bits &&& cap_bit) != 0
  end
end
