# SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
#
# SPDX-License-Identifier: Apache-2.0

defmodule AtomLGFX.Cache do
  @moduledoc false

  @min_target 0
  @max_target 254

  def get_open_config(port) do
    case get(open_config_cache_key(port)) do
      value when is_list(value) -> {:ok, value}
      _ -> {:ok, []}
    end
  end

  def remember_open_config(port, normalized_open_config) when is_list(normalized_open_config) do
    put(open_config_cache_key(port), normalized_open_config)
    :ok
  end

  def get_max_binary_bytes(port) do
    case get(max_binary_bytes_cache_key(port)) do
      value when is_integer(value) and value > 0 -> {:ok, value}
      _ -> :error
    end
  end

  def put_max_binary_bytes(port, max_binary_bytes)
      when is_integer(max_binary_bytes) and max_binary_bytes > 0 do
    put(max_binary_bytes_cache_key(port), max_binary_bytes)
    :ok
  end

  # Stored as normalized descriptors, for example:
  # - {{:rgb888, 0x112233}, nil}
  # - {{:index, 3}, {:rgb888, 0x000000}}
  # - {{:index, 1}, {:index, 0}}
  def get_text_color(port, target), do: get(text_color_cache_key(port, target))
  def put_text_color(port, target, value), do: put(text_color_cache_key(port, target), value)

  def get_text_size(port, target), do: get(text_size_cache_key(port, target))
  def put_text_size(port, target, value), do: put(text_size_cache_key(port, target), value)

  def get_text_font_selection(port, target),
    do: get(text_font_selection_cache_key(port, target))

  def put_text_font_selection(port, target, value),
    do: put(text_font_selection_cache_key(port, target), value)

  def erase_text_cache(port, target) do
    erase(text_color_cache_key(port, target))
    erase(text_size_cache_key(port, target))
    erase(text_font_selection_cache_key(port, target))
    :ok
  end

  def reset_runtime_cache(port) do
    erase(max_binary_bytes_cache_key(port))
    reset_runtime_cache_targets(port, @min_target)
    :ok
  end

  def get(key), do: :erlang.get(key)
  def put(key, value), do: :erlang.put(key, value)
  def erase(key), do: :erlang.erase(key)

  defp reset_runtime_cache_targets(_port, target) when target > @max_target, do: :ok

  defp reset_runtime_cache_targets(port, target) do
    erase_text_cache(port, target)
    reset_runtime_cache_targets(port, target + 1)
  end

  defp open_config_cache_key(port), do: {:lgfx_open_config, port}
  defp max_binary_bytes_cache_key(port), do: {:lgfx_max_binary_bytes, port}
  defp text_color_cache_key(port, target), do: {:lgfx_text_color, port, target}
  defp text_size_cache_key(port, target), do: {:lgfx_text_size, port, target}
  defp text_font_selection_cache_key(port, target), do: {:lgfx_text_font_selection, port, target}
end
