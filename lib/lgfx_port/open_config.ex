# SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
#
# SPDX-License-Identifier: Apache-2.0

defmodule LGFXPort.OpenConfig do
  @moduledoc false

  import LGFXPort.Guards

  @max_open_config_i32 0x7FFF_FFFF

  @panel_driver_atoms [:ili9488, :ili9341, :ili9341_2, :st7789]
  @spi_host_atoms [:spi2_host, :spi3_host]

  @open_option_rules [
    panel_driver: :panel_driver,
    width: :positive_u16,
    height: :positive_u16,
    offset_x: :i32,
    offset_y: :i32,
    offset_rotation: :rotation,
    readable: :boolean,
    invert: :boolean,
    rgb_order: :boolean,
    dlen_16bit: :boolean,
    lcd_spi_mode: :spi_mode,
    lcd_freq_write_hz: :positive_i32,
    lcd_freq_read_hz: :positive_i32,
    lcd_dma_channel: :dma_channel,
    lcd_spi_3wire: :boolean,
    lcd_use_lock: :boolean,
    lcd_bus_shared: :boolean,
    spi_sclk_gpio: :gpio,
    spi_mosi_gpio: :gpio,
    spi_miso_gpio: :gpio_or_disabled,
    lcd_spi_host: :spi_host,
    lcd_cs_gpio: :gpio,
    lcd_dc_gpio: :gpio,
    lcd_rst_gpio: :gpio_or_disabled,
    lcd_pin_busy: :gpio_or_disabled,
    touch_cs_gpio: :gpio_or_disabled,
    touch_irq_gpio: :gpio_or_disabled,
    touch_spi_host: :spi_host,
    touch_spi_freq_hz: :positive_i32,
    touch_offset_rotation: :rotation,
    touch_bus_shared: :boolean
  ]

  @supported_open_config_keys Keyword.keys(@open_option_rules)

  def normalize_open_config(options) when is_list(options) do
    normalize_open_options(options)
  end

  def normalize_open_config(other) do
    {:error, {:bad_open_options_shape, other}}
  end

  def normalize_open_options!(options) do
    case normalize_open_config(options) do
      {:ok, normalized_options} ->
        normalized_options

      {:error, reason} ->
        raise ArgumentError, format_open_option_error(reason)
    end
  end

  def supported_open_config_keys, do: @supported_open_config_keys

  defp normalize_open_options(options) when is_list(options) do
    with {:ok, normalized_map} <- normalize_open_option_entries(options, %{}, options) do
      {:ok, open_config_map_to_keyword(normalized_map)}
    end
  end

  defp normalize_open_option_entries([], normalized_map, _original_options) do
    {:ok, normalized_map}
  end

  defp normalize_open_option_entries(
         [{key, _value} = entry | rest],
         normalized_map,
         original_options
       )
       when is_atom(key) do
    case normalize_open_option_entry(entry, original_options) do
      {:ok, {normalized_key, normalized_value}} ->
        normalize_open_option_entries(
          rest,
          Map.put(normalized_map, normalized_key, normalized_value),
          original_options
        )

      {:error, _reason} = err ->
        err
    end
  end

  defp normalize_open_option_entries(_entries, _normalized_map, original_options) do
    {:error, {:bad_open_options_shape, original_options}}
  end

  defp normalize_open_option_entry({key, value}, _original_options) when is_atom(key) do
    case normalize_open_option_value(key, value) do
      {:ok, normalized_value} -> {:ok, {key, normalized_value}}
      {:error, _reason} = err -> err
    end
  end

  defp normalize_open_option_entry(_entry, original_options) do
    {:error, {:bad_open_options_shape, original_options}}
  end

  defp normalize_open_option_value(key, value) do
    case Keyword.fetch(@open_option_rules, key) do
      {:ok, rule} -> normalize_open_option_by_rule(rule, key, value)
      :error -> {:error, {:unknown_open_option, key}}
    end
  end

  defp normalize_open_option_by_rule(:panel_driver, _key, value)
       when value in @panel_driver_atoms,
       do: {:ok, value}

  defp normalize_open_option_by_rule(:panel_driver, key, value) do
    {:error, {:bad_open_option_value, key, value, ":ili9488, :ili9341, :ili9341_2, or :st7789"}}
  end

  defp normalize_open_option_by_rule(:positive_u16, _key, value)
       when u16(value) and value > 0,
       do: {:ok, value}

  defp normalize_open_option_by_rule(:positive_u16, key, value) do
    {:error, {:bad_open_option_value, key, value, "a positive integer in 1..65535"}}
  end

  defp normalize_open_option_by_rule(:positive_i32, _key, value)
       when positive_i32(value),
       do: {:ok, value}

  defp normalize_open_option_by_rule(:positive_i32, key, value) do
    {:error,
     {:bad_open_option_value, key, value, "a positive integer in 1..#{@max_open_config_i32}"}}
  end

  defp normalize_open_option_by_rule(:i32, _key, value) when i32(value), do: {:ok, value}

  defp normalize_open_option_by_rule(:i32, key, value) do
    {:error, {:bad_open_option_value, key, value, "a signed 32-bit integer"}}
  end

  defp normalize_open_option_by_rule(:rotation, _key, value)
       when is_integer(value) and value in 0..7,
       do: {:ok, value}

  defp normalize_open_option_by_rule(:rotation, key, value) do
    {:error, {:bad_open_option_value, key, value, "an integer in 0..7"}}
  end

  defp normalize_open_option_by_rule(:spi_mode, _key, value)
       when is_integer(value) and value in 0..3,
       do: {:ok, value}

  defp normalize_open_option_by_rule(:spi_mode, key, value) do
    {:error, {:bad_open_option_value, key, value, "an integer in 0..3"}}
  end

  defp normalize_open_option_by_rule(:boolean, _key, value) when is_boolean(value),
    do: {:ok, value}

  defp normalize_open_option_by_rule(:boolean, key, value) do
    {:error, {:bad_open_option_value, key, value, "a boolean"}}
  end

  defp normalize_open_option_by_rule(:gpio, _key, value)
       when is_integer(value) and value >= 0 and value <= 255,
       do: {:ok, value}

  defp normalize_open_option_by_rule(:gpio, key, value) do
    {:error, {:bad_open_option_value, key, value, "a GPIO integer in 0..255"}}
  end

  defp normalize_open_option_by_rule(:gpio_or_disabled, _key, value)
       when is_integer(value) and value >= -1 and value <= 255,
       do: {:ok, value}

  defp normalize_open_option_by_rule(:gpio_or_disabled, key, value) do
    {:error, {:bad_open_option_value, key, value, "a GPIO integer in -1..255 (-1 disables)"}}
  end

  defp normalize_open_option_by_rule(:spi_host, _key, value)
       when value in @spi_host_atoms,
       do: {:ok, value}

  defp normalize_open_option_by_rule(:spi_host, key, value) do
    {:error, {:bad_open_option_value, key, value, ":spi2_host or :spi3_host"}}
  end

  defp normalize_open_option_by_rule(:dma_channel, _key, value) when value in [1, 2],
    do: {:ok, value}

  defp normalize_open_option_by_rule(:dma_channel, _key, :spi_dma_ch_auto),
    do: {:ok, :spi_dma_ch_auto}

  defp normalize_open_option_by_rule(:dma_channel, key, value) do
    {:error, {:bad_open_option_value, key, value, ":spi_dma_ch_auto, 1, or 2"}}
  end

  defp open_config_map_to_keyword(normalized_map) when is_map(normalized_map) do
    Enum.reduce(@supported_open_config_keys, [], fn key, acc ->
      case Map.fetch(normalized_map, key) do
        {:ok, value} -> [{key, value} | acc]
        :error -> acc
      end
    end)
    |> :lists.reverse()
  end

  defp format_open_option_error({:bad_open_options_shape, options}) do
    "LGFXPort.open/1 expects a keyword list or proplist with atom keys, got: #{inspect(options)}"
  end

  defp format_open_option_error({:unknown_open_option, key}) do
    "unknown LGFXPort.open/1 option #{inspect(key)}; supported keys: #{inspect(@supported_open_config_keys)}"
  end

  defp format_open_option_error({:bad_open_option_value, key, value, expected}) do
    "bad LGFXPort.open/1 value for #{inspect(key)}: #{inspect(value)} (expected #{expected})"
  end
end
