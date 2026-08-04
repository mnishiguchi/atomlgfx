# SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
#
# SPDX-License-Identifier: Apache-2.0

defmodule AtomLGFX.BinaryBatch.Diagnostics do
  @moduledoc false

  alias AtomLGFX.BinaryBatch.Codec
  alias AtomLGFX.Errors
  alias AtomLGFX.OpSchema

  @push_rotate_zoom_list_record_size 12

  @doc """
  バイナリーバッチ命令列の簡潔な診断概要を返します。
  """
  @spec summary(iodata()) :: {:ok, map()} | {:error, term()}
  def summary(commands) do
    command_binary = Codec.batch(commands)

    with {:ok, decoded_commands} <- Codec.decode(command_binary) do
      {:ok, summarize_decoded_commands(command_binary, decoded_commands)}
    end
  end

  @doc """
  バイナリーバッチの診断概要を返し、不正な場合は `ArgumentError` を送出します。
  """
  @spec summary!(iodata()) :: map()
  def summary!(commands) do
    case summary(commands) do
      {:ok, summary} -> summary
      {:error, reason} -> raise ArgumentError, Errors.format_error(reason)
    end
  end

  @doc """
  バイナリーバッチ命令列の構造化された診断報告を返します。
  """
  @spec diagnose(iodata()) :: {:ok, map()} | {:error, map()}
  def diagnose(commands) do
    command_binary = Codec.batch(commands)

    if command_binary == <<>> do
      {:error, build_diagnosis(command_binary, [], :empty_batch)}
    else
      case Codec.decode_with_partial(command_binary) do
        {:ok, decoded_commands} ->
          {:ok, summarize_valid_diagnosis(command_binary, decoded_commands)}

        {:error, reason, decoded_commands} ->
          {:error, build_diagnosis(command_binary, decoded_commands, reason)}
      end
    end
  end

  @doc """
  正常なバイナリーバッチ診断報告を返し、不正な場合は `ArgumentError` を送出します。
  """
  @spec diagnose!(iodata()) :: map()
  def diagnose!(commands) do
    case diagnose(commands) do
      {:ok, diagnosis} -> diagnosis
      {:error, diagnosis} -> raise ArgumentError, Map.fetch!(diagnosis, :message)
    end
  end

  @doc """
  `summary/1` の測定値を使用して2つのバイナリーバッチ命令列を比較します。

  第1引数を基準、第2引数を候補として扱います。差分値は `候補 - 基準` です。容量削減を確認しやすいよう、`:batch_bytes_savings` だけは `基準 - 候補` で計算します。

  試験、記録、ADR に基づく性能分析での利用を想定しています。ネイティブドライバーは呼び出さず、描画の高速経路にも影響しません。
  """
  @spec compare(iodata(), iodata()) ::
          {:ok, map()} | {:error, {:baseline, term()}} | {:error, {:candidate, term()}}
  def compare(baseline_commands, candidate_commands) do
    case summary(baseline_commands) do
      {:ok, baseline_summary} ->
        case summary(candidate_commands) do
          {:ok, candidate_summary} ->
            {:ok,
             %{
               baseline: baseline_summary,
               candidate: candidate_summary,
               delta: compare_summary_delta(baseline_summary, candidate_summary)
             }}

          {:error, reason} ->
            {:error, {:candidate, reason}}
        end

      {:error, reason} ->
        {:error, {:baseline, reason}}
    end
  end

  @doc """
  2つのバイナリーバッチ命令列を比較し、不正な場合は `ArgumentError` を送出します。
  """
  @spec compare!(iodata(), iodata()) :: map()
  def compare!(baseline_commands, candidate_commands) do
    case compare(baseline_commands, candidate_commands) do
      {:ok, comparison} ->
        comparison

      {:error, {side, reason}} ->
        raise ArgumentError, "#{side} batch invalid: #{Errors.format_error(reason)}"
    end
  end

  @doc """
  呼び出し側が指定した診断上限に対して、バイナリーバッチ命令列を検査します。

  試験、生成フレームの安全柵、性能記録での利用を想定しています。`summary/1` を使用するため、ネイティブドライバーは呼び出さず、描画の高速経路にも影響しません。

  対応する上限:

  - `:max_batch_bytes`
  - `:max_command_count`
  - `:max_scalar_count`
  - `:max_render_private_count`
  - `:max_dynamic_payload_bytes`
  - `:max_fixed_overhead_bytes`
  - `:max_bytes_per_command_x1000`
  - `:max_bytes_per_logical_scalar_x1000`
  - `:max_dynamic_payload_ratio_x1000`
  - `:max_packed_list_record_ratio_x1000`
  - `:max_packed_list_instances_per_command_x1000`
  - `:min_packed_list_count`
  - `:min_packed_list_instance_count`
  - `:min_packed_list_instances_per_command_x1000`

  すべての上限を満たす場合は `{:ok, report}`、いずれかを超えた場合は `{:error, {:budget_exceeded, report}}` を返します。
  """
  @spec check_budget(iodata(), map() | keyword()) ::
          {:ok, map()} | {:error, term()} | {:error, {:budget_exceeded, map()}}
  def check_budget(commands, limits) when is_map(limits) or is_list(limits) do
    with {:ok, normalized_limits} <- normalize_budget_limits(limits),
         {:ok, summary} <- summary(commands) do
      violations = budget_violations(summary, normalized_limits)

      report = %{
        ok?: violations == [],
        summary: summary,
        limits: normalized_budget_limits_map(normalized_limits),
        violations: violations
      }

      if violations == [] do
        {:ok, report}
      else
        {:error, {:budget_exceeded, report}}
      end
    end
  end

  def check_budget(_commands, limits) do
    {:error, {:invalid_budget_limits, limits}}
  end

  @doc """
  診断上限に対してバイナリーバッチ命令列を検査し、不正な場合は `ArgumentError` を送出します。
  """
  @spec check_budget!(iodata(), map() | keyword()) :: map()
  def check_budget!(commands, limits) do
    case check_budget(commands, limits) do
      {:ok, report} ->
        report

      {:error, {:budget_exceeded, report}} ->
        raise ArgumentError, format_budget_exceeded(report)

      {:error, {:invalid_budget_limit, key, value}} ->
        raise ArgumentError,
              "invalid binary batch budget limit #{inspect(key)}: #{inspect(value)}"

      {:error, {:invalid_budget_limits, value}} ->
        raise ArgumentError, "invalid binary batch budget limits: #{inspect(value)}"

      {:error, reason} ->
        raise ArgumentError, Errors.format_error(reason)
    end
  end

  @budget_limit_specs %{
    max_batch_bytes: {:max, :batch_bytes},
    max_command_count: {:max, :command_count},
    max_scalar_count: {:max, :scalar_count},
    max_render_private_count: {:max, :render_private_count},
    max_dynamic_payload_bytes: {:max, :dynamic_payload_bytes},
    max_fixed_overhead_bytes: {:max, :fixed_overhead_bytes},
    max_bytes_per_command_x1000: {:max, :bytes_per_command_x1000},
    max_bytes_per_logical_scalar_x1000: {:max, :bytes_per_logical_scalar_x1000},
    max_dynamic_payload_ratio_x1000: {:max, :dynamic_payload_ratio_x1000},
    max_packed_list_record_ratio_x1000: {:max, :packed_list_record_ratio_x1000},
    max_packed_list_instances_per_command_x1000: {:max, :packed_list_instances_per_command_x1000},
    min_packed_list_count: {:min, :packed_list_count},
    min_packed_list_instance_count: {:min, :packed_list_instance_count},
    min_packed_list_instances_per_command_x1000: {:min, :packed_list_instances_per_command_x1000}
  }

  defp normalize_budget_limits(limits) when is_map(limits) do
    limits
    |> Map.to_list()
    |> normalize_budget_limit_entries([])
  end

  defp normalize_budget_limits(limits) when is_list(limits) do
    normalize_budget_limit_entries(limits, [])
  end

  defp normalize_budget_limit_entries([], acc), do: {:ok, :lists.reverse(acc)}

  defp normalize_budget_limit_entries([{limit_key, value} | rest], acc)
       when is_atom(limit_key) and is_integer(value) and value >= 0 do
    case Map.fetch(@budget_limit_specs, limit_key) do
      {:ok, {direction, metric}} ->
        normalize_budget_limit_entries(rest, [{limit_key, direction, metric, value} | acc])

      :error ->
        {:error, {:invalid_budget_limit, limit_key, value}}
    end
  end

  defp normalize_budget_limit_entries([{limit_key, value} | _rest], _acc) do
    {:error, {:invalid_budget_limit, limit_key, value}}
  end

  defp normalize_budget_limit_entries([entry | _rest], _acc) do
    {:error, {:invalid_budget_limit, entry, nil}}
  end

  defp normalized_budget_limits_map(normalized_limits) do
    normalized_limits
    |> Enum.map(fn {limit_key, _direction, _metric, value} -> {limit_key, value} end)
    |> Map.new()
  end

  defp budget_violations(summary, normalized_limits) do
    normalized_limits
    |> Enum.reduce([], fn limit, acc ->
      case budget_violation(summary, limit) do
        nil -> acc
        violation -> [violation | acc]
      end
    end)
    |> :lists.reverse()
  end

  defp budget_violation(summary, {limit_key, direction, metric, limit_value}) do
    actual_value = Map.fetch!(summary, metric)

    cond do
      is_nil(actual_value) ->
        %{
          limit: limit_key,
          metric: metric,
          direction: direction,
          actual: nil,
          limit_value: limit_value,
          reason: :metric_unavailable
        }

      direction == :max and actual_value > limit_value ->
        %{
          limit: limit_key,
          metric: metric,
          direction: :max,
          actual: actual_value,
          limit_value: limit_value,
          over_by: actual_value - limit_value
        }

      direction == :min and actual_value < limit_value ->
        %{
          limit: limit_key,
          metric: metric,
          direction: :min,
          actual: actual_value,
          limit_value: limit_value,
          under_by: limit_value - actual_value
        }

      true ->
        nil
    end
  end

  defp format_budget_exceeded(%{violations: violations}) do
    formatted_violations =
      violations
      |> Enum.map(&format_budget_violation/1)
      |> Enum.join(", ")

    "binary batch exceeds budget: #{formatted_violations}"
  end

  defp format_budget_violation(%{
         reason: :metric_unavailable,
         metric: metric,
         limit: limit_key,
         limit_value: limit_value
       }) do
    "#{metric} unavailable for #{limit_key} #{limit_value}"
  end

  defp format_budget_violation(%{
         direction: :max,
         metric: metric,
         actual: actual_value,
         limit: limit_key,
         limit_value: limit_value
       }) do
    "#{metric} #{actual_value} > #{limit_key} #{limit_value}"
  end

  defp format_budget_violation(%{
         direction: :min,
         metric: metric,
         actual: actual_value,
         limit: limit_key,
         limit_value: limit_value
       }) do
    "#{metric} #{actual_value} < #{limit_key} #{limit_value}"
  end

  @comparison_delta_keys [
    :batch_bytes,
    :command_count,
    :scalar_count,
    :render_private_count,
    :dynamic_payload_bytes,
    :packed_list_record_bytes,
    :packed_list_count,
    :packed_list_instance_count,
    :fixed_overhead_bytes,
    :bytes_per_command_x1000,
    :bytes_per_logical_scalar_x1000,
    :dynamic_payload_ratio_x1000,
    :packed_list_record_ratio_x1000,
    :packed_list_instances_per_command_x1000
  ]

  defp compare_summary_delta(baseline_summary, candidate_summary) do
    @comparison_delta_keys
    |> Enum.reduce(%{}, fn key, acc ->
      baseline_value = Map.fetch!(baseline_summary, key)
      candidate_value = Map.fetch!(candidate_summary, key)

      Map.put(acc, key, optional_delta(baseline_value, candidate_value))
    end)
    |> Map.put(
      :batch_bytes_savings,
      Map.fetch!(baseline_summary, :batch_bytes) - Map.fetch!(candidate_summary, :batch_bytes)
    )
    |> Map.put(
      :batch_bytes_savings_ratio_x1000,
      ratio_x1000(
        Map.fetch!(baseline_summary, :batch_bytes) - Map.fetch!(candidate_summary, :batch_bytes),
        Map.fetch!(baseline_summary, :batch_bytes)
      )
    )
    |> Map.put(
      :candidate_batch_bytes_ratio_x1000,
      ratio_x1000(
        Map.fetch!(candidate_summary, :batch_bytes),
        Map.fetch!(baseline_summary, :batch_bytes)
      )
    )
  end

  defp optional_delta(nil, _candidate_value), do: nil
  defp optional_delta(_baseline_value, nil), do: nil
  defp optional_delta(baseline_value, candidate_value), do: candidate_value - baseline_value

  defp summarize_decoded_commands(command_binary, decoded_commands) do
    decoded_commands
    |> Enum.reduce(initial_summary(command_binary), &accumulate_summary_command/2)
    |> finalize_summary()
  end

  defp initial_summary(command_binary) do
    %{
      batch_bytes: byte_size(command_binary),
      command_count: 0,
      ops: %{},
      scalar_count: 0,
      render_private_count: 0,
      dynamic_payload_bytes: 0,
      packed_list_record_bytes: 0,
      packed_list_count: 0,
      packed_list_instance_count: 0,
      clip_count: 0,
      text_count: 0,
      sprite_state_count: 0,
      sprite_push_count: 0,
      push_rotate_zoom_count: 0,
      push_rotate_zoom_list_count: 0,
      push_rotate_zoom_instance_count: 0,
      display_count: 0,
      target_count: 0,
      fixed_overhead_bytes: 0,
      bytes_per_command_x1000: nil,
      bytes_per_logical_scalar_x1000: nil,
      dynamic_payload_ratio_x1000: 0,
      packed_list_record_ratio_x1000: 0,
      packed_list_instances_per_command_x1000: nil
    }
  end

  defp finalize_summary(summary) do
    batch_bytes = Map.fetch!(summary, :batch_bytes)
    command_count = Map.fetch!(summary, :command_count)
    scalar_count = Map.fetch!(summary, :scalar_count)
    dynamic_payload_bytes = Map.fetch!(summary, :dynamic_payload_bytes)
    packed_list_record_bytes = Map.fetch!(summary, :packed_list_record_bytes)
    packed_list_count = Map.fetch!(summary, :packed_list_count)
    packed_list_instance_count = Map.fetch!(summary, :packed_list_instance_count)

    summary
    |> Map.put(:fixed_overhead_bytes, max(batch_bytes - dynamic_payload_bytes, 0))
    |> Map.put(:bytes_per_command_x1000, ratio_x1000(batch_bytes, command_count))
    |> Map.put(:bytes_per_logical_scalar_x1000, ratio_x1000(batch_bytes, scalar_count))
    |> Map.put(:dynamic_payload_ratio_x1000, ratio_x1000(dynamic_payload_bytes, batch_bytes))
    |> Map.put(
      :packed_list_record_ratio_x1000,
      ratio_x1000(packed_list_record_bytes, batch_bytes)
    )
    |> Map.put(
      :packed_list_instances_per_command_x1000,
      ratio_x1000(packed_list_instance_count, packed_list_count)
    )
  end

  defp ratio_x1000(_numerator, denominator) when denominator in [0, nil], do: nil

  defp ratio_x1000(numerator, denominator)
       when is_integer(numerator) and is_integer(denominator) and denominator > 0 do
    div(numerator * 1000, denominator)
  end

  defp accumulate_summary_command(command, summary) do
    op = Map.fetch!(command, :op)

    summary
    |> Map.update!(:command_count, &(&1 + 1))
    |> Map.update!(:ops, &increment_count(&1, op))
    |> accumulate_render_private_count(command)
    |> accumulate_summary_category(command)
  end

  defp accumulate_render_private_count(summary, %{opcode: opcode}) do
    if opcode in render_private_opcode_values() do
      Map.update!(summary, :render_private_count, &(&1 + 1))
    else
      summary
    end
  end

  defp accumulate_render_private_count(summary, _command), do: summary

  defp render_private_opcode_values do
    Codec.__render_private_opcodes__()
    |> Keyword.values()
  end

  defp accumulate_dynamic_payload_bytes(summary, bytes) when is_integer(bytes) and bytes >= 0 do
    Map.update!(summary, :dynamic_payload_bytes, &(&1 + bytes))
  end

  defp accumulate_packed_list(summary, instance_count, record_size)
       when is_integer(instance_count) and instance_count >= 0 and is_integer(record_size) and
              record_size >= 0 do
    record_bytes = instance_count * record_size

    summary
    |> Map.update!(:packed_list_count, &(&1 + 1))
    |> Map.update!(:packed_list_instance_count, &(&1 + instance_count))
    |> Map.update!(:packed_list_record_bytes, &(&1 + record_bytes))
    |> accumulate_dynamic_payload_bytes(record_bytes)
  end

  defp accumulate_summary_category(summary, %{op: op})
       when op in [
              :fill_screen,
              :clear,
              :draw_pixel,
              :draw_fast_vline,
              :draw_fast_hline,
              :draw_line,
              :draw_rect,
              :fill_rect,
              :draw_round_rect,
              :fill_round_rect,
              :draw_circle,
              :fill_circle,
              :draw_ellipse,
              :fill_ellipse,
              :draw_arc,
              :fill_arc,
              :draw_bezier,
              :draw_triangle,
              :fill_triangle
            ] do
    Map.update!(summary, :scalar_count, &(&1 + 1))
  end

  defp accumulate_summary_category(summary, %{op: op})
       when op in [:set_clip_rect, :clear_clip_rect] do
    Map.update!(summary, :clip_count, &(&1 + 1))
  end

  defp accumulate_summary_category(summary, %{op: op, text_len: text_len})
       when op in [:draw_string, :print, :println] do
    summary
    |> Map.update!(:text_count, &(&1 + 1))
    |> accumulate_dynamic_payload_bytes(text_len)
  end

  defp accumulate_summary_category(summary, %{op: op})
       when op in [
              :set_text_font_preset,
              :set_text_size,
              :set_text_datum,
              :set_text_wrap,
              :set_cursor,
              :set_text_color
            ] do
    Map.update!(summary, :text_count, &(&1 + 1))
  end

  defp accumulate_summary_category(summary, %{op: op})
       when op in [:set_palette_color, :set_pivot] do
    Map.update!(summary, :sprite_state_count, &(&1 + 1))
  end

  defp accumulate_summary_category(summary, %{op: :push_sprite}) do
    Map.update!(summary, :sprite_push_count, &(&1 + 1))
  end

  defp accumulate_summary_category(summary, %{op: :push_rotate_zoom}) do
    summary
    |> Map.update!(:push_rotate_zoom_count, &(&1 + 1))
    |> Map.update!(:sprite_push_count, &(&1 + 1))
  end

  defp accumulate_summary_category(summary, %{op: :push_rotate_zoom_list, instances: instances}) do
    instance_count = length(instances)

    summary
    |> Map.update!(:push_rotate_zoom_list_count, &(&1 + 1))
    |> Map.update!(:push_rotate_zoom_instance_count, &(&1 + instance_count))
    |> accumulate_packed_list(instance_count, @push_rotate_zoom_list_record_size)
  end

  defp accumulate_summary_category(summary, %{op: :display}) do
    Map.update!(summary, :display_count, &(&1 + 1))
  end

  defp accumulate_summary_category(summary, %{op: :target}) do
    Map.update!(summary, :target_count, &(&1 + 1))
  end

  defp accumulate_summary_category(summary, _command), do: summary

  defp summarize_valid_diagnosis(command_binary, decoded_commands) do
    summarize_decoded_commands(command_binary, decoded_commands)
    |> Map.put(:valid?, true)
    |> Map.put(:message, "binary batch is valid")
    |> Map.put(:error, nil)
    |> Map.put(:failed_index, nil)
    |> Map.put(:failed_opcode, nil)
    |> Map.put(:failed_op, nil)
    |> Map.put(:decoded_command_count, length(decoded_commands))
    |> Map.put(:last_decoded_command, List.last(decoded_commands))
  end

  defp build_diagnosis(command_binary, decoded_commands, reason) do
    {failed_index, failed_opcode} = failed_command_location(reason)

    summarize_decoded_commands(command_binary, decoded_commands)
    |> Map.put(:valid?, false)
    |> Map.put(:message, Errors.format_error(reason))
    |> Map.put(:error, reason)
    |> Map.put(:failed_index, failed_index)
    |> Map.put(:failed_opcode, failed_opcode)
    |> Map.put(:failed_op, opcode_name(failed_opcode))
    |> Map.put(:decoded_command_count, length(decoded_commands))
    |> Map.put(:last_decoded_command, List.last(decoded_commands))
  end

  defp failed_command_location({:batch_failed, index, opcode, _reason}), do: {index, opcode}
  defp failed_command_location({:batch_failed, {index, opcode, _reason}}), do: {index, opcode}
  defp failed_command_location(_reason), do: {nil, nil}

  defp opcode_name(nil), do: nil

  defp opcode_name(opcode) do
    case render_private_opcode_name(opcode) do
      nil ->
        case OpSchema.name(opcode) do
          {:ok, name} -> name
          :error -> nil
        end

      name ->
        name
    end
  end

  defp render_private_opcode_name(opcode) do
    find_render_private_opcode_name(Codec.__render_private_opcodes__(), opcode)
  end

  defp find_render_private_opcode_name([], _opcode), do: nil

  defp find_render_private_opcode_name([{name, value} | _rest], opcode) when value == opcode,
    do: name

  defp find_render_private_opcode_name([_entry | rest], opcode) do
    find_render_private_opcode_name(rest, opcode)
  end

  defp increment_count(counts, key) do
    Map.update(counts, key, 1, &(&1 + 1))
  end
end
