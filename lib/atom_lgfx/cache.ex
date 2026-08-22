# SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
#
# SPDX-License-Identifier: Apache-2.0

defmodule AtomLGFX.Cache do
  @moduledoc false

  @min_target 0
  @max_target 254

  def get_open_config(handle) do
    case get(open_config_cache_key(handle)) do
      value when is_list(value) -> {:ok, value}
      _ -> {:ok, []}
    end
  end

  def remember_open_config(handle, normalized_open_config) when is_list(normalized_open_config) do
    put(open_config_cache_key(handle), normalized_open_config)
    :ok
  end

  def get_max_binary_bytes(handle) do
    case get(max_binary_bytes_cache_key(handle)) do
      value when is_integer(value) and value > 0 -> {:ok, value}
      _ -> :error
    end
  end

  def put_max_binary_bytes(handle, max_binary_bytes)
      when is_integer(max_binary_bytes) and max_binary_bytes > 0 do
    put(max_binary_bytes_cache_key(handle), max_binary_bytes)
    :ok
  end

  # Stored as normalized descriptors, for example:
  # - {{:rgb565, 0xF81F}, nil}
  # - {{:index, 3}, {:rgb565, 0x0000}}
  # - {{:index, 1}, {:index, 0}}
  def get_text_color(handle, target), do: get(text_color_cache_key(handle, target))
  def put_text_color(handle, target, value), do: put(text_color_cache_key(handle, target), value)

  def get_text_size(handle, target), do: get(text_size_cache_key(handle, target))
  def put_text_size(handle, target, value), do: put(text_size_cache_key(handle, target), value)

  def get_text_font_selection(handle, target),
    do: get(text_font_selection_cache_key(handle, target))

  def put_text_font_selection(handle, target, value),
    do: put(text_font_selection_cache_key(handle, target), value)

  def erase_text_cache(handle, target) do
    erase(text_color_cache_key(handle, target))
    erase(text_size_cache_key(handle, target))
    erase(text_font_selection_cache_key(handle, target))
    :ok
  end

  def reset_runtime_cache(handle) do
    erase(max_binary_bytes_cache_key(handle))
    reset_runtime_cache_targets(handle, @min_target)
    :ok
  end

  def get(key), do: :erlang.get(key)
  def put(key, value), do: :erlang.put(key, value)
  def erase(key), do: :erlang.erase(key)

  defp reset_runtime_cache_targets(_handle, target) when target > @max_target, do: :ok

  defp reset_runtime_cache_targets(handle, target) do
    erase_text_cache(handle, target)
    reset_runtime_cache_targets(handle, target + 1)
  end

  defp open_config_cache_key(handle), do: {:lgfx_open_config, handle}
  defp max_binary_bytes_cache_key(handle), do: {:lgfx_max_binary_bytes, handle}
  defp text_color_cache_key(handle, target), do: {:lgfx_text_color, handle, target}
  defp text_size_cache_key(handle, target), do: {:lgfx_text_size, handle, target}

  defp text_font_selection_cache_key(handle, target),
    do: {:lgfx_text_font_selection, handle, target}
end
