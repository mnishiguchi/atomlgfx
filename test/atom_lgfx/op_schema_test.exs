# SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
#
# SPDX-License-Identifier: Apache-2.0

defmodule AtomLGFX.OpSchemaTest do
  use ExUnit.Case, async: true

  import Bitwise

  alias AtomLGFX.Generated
  alias AtomLGFX.OpSchema
  alias AtomLGFX.Protocol

  @ops_def_path Path.expand("../../lgfx_port/include_internal/lgfx_port/ops.def", __DIR__)

  @flag_values %{
    "LGFX_F_TEXT_HAS_BG" => 1 <<< 0,
    "LGFX_F_COLOR_INDEX" => 1 <<< 1,
    "LGFX_F_TEXT_FG_INDEX" => 1 <<< 2,
    "LGFX_F_TEXT_BG_INDEX" => 1 <<< 3,
    "LGFX_F_TRANSPARENT_INDEX" => 1 <<< 4
  }

  test "exposes the authoritative operation table" do
    assert OpSchema.ops() == Generated.ops()

    assert {:ok, meta} = OpSchema.meta(:fill_rect)
    assert Keyword.get(meta, :public) == true
    assert Keyword.get(meta, :raw) == false

    assert Keyword.keys(meta) == [
             :opcode,
             :public,
             :raw,
             :arg_range,
             :allowed_flags,
             :target_policy,
             :state_policy,
             :capability,
             :batchable,
             :needs_owned_payload,
             :sync_only,
             :batch_boundary_sensitive
           ]

    assert is_integer(Keyword.get(meta, :opcode))
    assert Keyword.get(meta, :arg_range) == 5..5
    assert Keyword.get(meta, :allowed_flags) == Protocol.color_index_flag()
    assert Keyword.get(meta, :target_policy) == :any
    assert Keyword.get(meta, :state_policy) == :requires_init
    assert Keyword.get(meta, :capability) == nil
    assert Keyword.get(meta, :batchable) == true
    assert Keyword.get(meta, :needs_owned_payload) == false
    assert Keyword.get(meta, :sync_only) == false
    assert Keyword.get(meta, :batch_boundary_sensitive) == false
  end

  test "keeps generated helpers aligned with the schema" do
    assert Generated.opcode(:fill_rect) == OpSchema.opcode(:fill_rect)
    assert Generated.canonical_name(:fill_rect) == OpSchema.canonical_name(:fill_rect)
    assert Generated.arg_range(:draw_bezier) == OpSchema.arg_range(:draw_bezier)
    assert Generated.allowed_flags(:push_sprite) == OpSchema.allowed_flags(:push_sprite)

    assert Generated.batchable?(:push_image) == OpSchema.batchable?(:push_image)

    assert Generated.needs_owned_payload?(:push_image) ==
             OpSchema.needs_owned_payload?(:push_image)

    assert Generated.sync_only?(:get_touch) == OpSchema.sync_only?(:get_touch)

    assert Generated.batch_boundary_sensitive?(:display) ==
             OpSchema.batch_boundary_sensitive?(:display)
  end

  test "keeps generated opcode and schema metadata aligned with ops.def" do
    operations =
      @ops_def_path
      |> File.read!()
      |> parse_operations()

    assert length(operations) == length(Generated.ops())

    operations
    |> Enum.with_index()
    |> Enum.each(fn {operation, opcode} ->
      native_name = String.to_atom(operation.operation_name)
      elixir_name = expected_elixir_name(operation.operation_name)
      expected_arg_range = expected_arg_range(operation)
      expected_allowed_flags = expected_allowed_flags(operation.allowed_flags_mask)
      expected_target_policy = expected_target_policy(operation.target_policy)
      expected_state_policy = expected_state_policy(operation.state_policy)
      expected_capability = expected_capability(operation.feature_cap_bit)
      expected_batchable = expected_bool(operation.batchable)
      expected_needs_owned_payload = expected_bool(operation.needs_owned_payload)
      expected_sync_only = expected_bool(operation.sync_only)
      expected_batch_boundary_sensitive = expected_bool(operation.batch_boundary_sensitive)

      assert Generated.name(opcode) == {:ok, elixir_name}
      assert Generated.opcode(elixir_name) == {:ok, opcode}

      assert OpSchema.name(opcode) == {:ok, elixir_name}
      assert OpSchema.opcode(elixir_name) == {:ok, opcode}
      assert OpSchema.arg_range(elixir_name) == {:ok, expected_arg_range}
      assert Generated.arg_range(elixir_name) == {:ok, expected_arg_range}
      assert Generated.allowed_flags(elixir_name) == {:ok, expected_allowed_flags}
      assert Generated.target_policy(elixir_name) == {:ok, expected_target_policy}
      assert Generated.state_policy(elixir_name) == {:ok, expected_state_policy}
      assert Generated.capability(elixir_name) == {:ok, expected_capability}

      assert OpSchema.allowed_flags(elixir_name) == {:ok, expected_allowed_flags}

      assert Generated.batchable?(elixir_name) == expected_batchable
      assert OpSchema.batchable?(elixir_name) == expected_batchable

      assert Generated.needs_owned_payload?(elixir_name) == expected_needs_owned_payload
      assert OpSchema.needs_owned_payload?(elixir_name) == expected_needs_owned_payload

      assert Generated.sync_only?(elixir_name) == expected_sync_only
      assert OpSchema.sync_only?(elixir_name) == expected_sync_only

      assert Generated.batch_boundary_sensitive?(elixir_name) == expected_batch_boundary_sensitive
      assert OpSchema.batch_boundary_sensitive?(elixir_name) == expected_batch_boundary_sensitive

      if native_name == elixir_name do
        assert Generated.canonical_name(native_name) == {:ok, elixir_name}
        assert OpSchema.canonical_name(native_name) == {:ok, elixir_name}
      else
        assert Generated.canonical_name(native_name) == :error
        assert OpSchema.canonical_name(native_name) == :error
      end
    end)
  end

  test "marks only implemented protocol opcodes as binary-batchable" do
    batchable_operations =
      @ops_def_path
      |> File.read!()
      |> parse_operations()
      |> Enum.filter(&(&1.batchable == "1"))
      |> Enum.map(&expected_elixir_name(&1.operation_name))

    assert batchable_operations == [
             :display,
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
             :fill_triangle,
             :set_text_size,
             :set_text_datum,
             :set_text_wrap,
             :set_text_font_preset,
             :set_text_color,
             :set_cursor,
             :draw_string,
             :print,
             :println,
             :draw_jpg,
             :push_image,
             :set_clip_rect,
             :clear_clip_rect,
             :set_palette_color,
             :set_pivot,
             :push_sprite,
             :push_rotate_zoom,
             :push_rotate_zoom_list
           ]
  end

  test "keeps internal batch metadata self-consistent" do
    operations =
      @ops_def_path
      |> File.read!()
      |> parse_operations()

    Enum.each(operations, fn operation ->
      assert operation.batchable in ["0", "1"]
      assert operation.needs_owned_payload in ["0", "1"]
      assert operation.sync_only in ["0", "1"]
      assert operation.batch_boundary_sensitive in ["0", "1"]

      assert operation.batchable == "1" != (operation.sync_only == "1")

      if operation.needs_owned_payload == "1" do
        assert operation.batchable == "1"
      end

      if operation.batch_boundary_sensitive == "1" do
        assert operation.batchable == "1"
      end
    end)
  end

  test "validates wire arity and flags before crossing the port boundary" do
    assert OpSchema.validate_wire_call(:fill_rect, [1, 2, 3, 4, 0xFFFF], 0) == :ok
    assert OpSchema.validate_wire_call(:fill_rect, [1, 2, 3, 4, 0xFFFF], 2) == :ok

    assert OpSchema.validate_wire_call(:fill_rect, [1, 2, 3, 4], 0) ==
             {:error, {:bad_lgfx_arg_count, :fill_rect, 5, 5, 4}}

    assert OpSchema.validate_wire_call(:fill_rect, [1, 2, 3, 4, 0xFFFF], 4) ==
             {:error, {:bad_lgfx_flags, :fill_rect, 2, 4}}
  end

  test "allows only zero flags for submit binary batch" do
    command_binary = <<0xF0, 0, OpSchema.opcode!(:fill_screen), 0::little-16>>
    transparent_index_flag = Protocol.transparent_index_flag()

    assert OpSchema.validate_wire_call(:submit_binary_batch, [command_binary], 0) == :ok

    assert OpSchema.validate_wire_call(
             :submit_binary_batch,
             [command_binary],
             transparent_index_flag
           ) ==
             {:error, {:bad_lgfx_flags, :submit_binary_batch, 0, transparent_index_flag}}
  end

  defp expected_bool("0"), do: false
  defp expected_bool("1"), do: true

  defp expected_elixir_name(operation_name) do
    operation_name
    |> String.replace("VLine", "Vline")
    |> String.replace("HLine", "Hline")
    |> Macro.underscore()
    |> String.to_atom()
  end

  defp expected_arg_range(operation) do
    min_arg_count = String.to_integer(operation.min_arity) - 5
    max_arg_count = String.to_integer(operation.max_arity) - 5

    min_arg_count..max_arg_count
  end

  defp expected_allowed_flags("0"), do: 0

  defp expected_allowed_flags(mask) do
    @flag_values
    |> Enum.reduce(0, fn {flag_name, flag_value}, acc ->
      if String.contains?(mask, flag_name) do
        Bitwise.bor(acc, flag_value)
      else
        acc
      end
    end)
  end

  defp expected_target_policy("LGFX_OP_TARGET_BAD_TARGET"), do: :bad_target
  defp expected_target_policy("LGFX_OP_TARGET_ANY"), do: :any
  defp expected_target_policy("LGFX_OP_TARGET_SPRITE_ONLY"), do: :sprite_only

  defp expected_state_policy("LGFX_OP_STATE_ANY"), do: :any
  defp expected_state_policy("LGFX_OP_STATE_REQUIRES_INIT"), do: :requires_init

  defp expected_capability("0"), do: nil
  defp expected_capability("LGFX_CAP_SPRITE"), do: :sprite
  defp expected_capability("LGFX_CAP_PUSHIMAGE"), do: :pushimage
  defp expected_capability("LGFX_CAP_LAST_ERROR"), do: :last_error
  defp expected_capability("LGFX_CAP_TOUCH"), do: :touch
  defp expected_capability("LGFX_CAP_PALETTE"), do: :palette
  defp expected_capability("LGFX_CAP_BATCH"), do: :batch

  defp parse_operations(ops_def_content) do
    ops_def_content
    |> String.split("\n")
    |> Enum.reduce([], fn line, operations ->
      case parse_x_macro_line(line) do
        {:ok, operation} -> [operation | operations]
        :skip -> operations
      end
    end)
    |> Enum.reverse()
  end

  defp parse_x_macro_line(line) do
    trimmed_line = String.trim(line)

    cond do
      trimmed_line == "" ->
        :skip

      String.starts_with?(trimmed_line, "//") ->
        :skip

      true ->
        case Regex.run(~r/^X\((.*)\)\s*$/, trimmed_line, capture: :all_but_first) do
          [arguments_blob] ->
            arguments = split_top_level_arguments(arguments_blob)

            case arguments do
              [
                operation_name,
                _handler_function_name,
                _atom_str_macro,
                min_arity,
                max_arity,
                allowed_flags_mask,
                target_policy,
                state_policy,
                feature_cap_bit,
                batchable,
                needs_owned_payload,
                sync_only,
                batch_boundary_sensitive
              ] ->
                {:ok,
                 %{
                   operation_name: String.trim(operation_name),
                   min_arity: String.trim(min_arity),
                   max_arity: String.trim(max_arity),
                   allowed_flags_mask: String.trim(allowed_flags_mask),
                   target_policy: String.trim(target_policy),
                   state_policy: String.trim(state_policy),
                   feature_cap_bit: String.trim(feature_cap_bit),
                   batchable: String.trim(batchable),
                   needs_owned_payload: String.trim(needs_owned_payload),
                   sync_only: String.trim(sync_only),
                   batch_boundary_sensitive: String.trim(batch_boundary_sensitive)
                 }}

              _ ->
                raise "Unexpected X(...) field count in line: #{line}"
            end

          nil ->
            :skip
        end
    end
  end

  defp split_top_level_arguments(arguments_blob) do
    arguments_blob
    |> String.to_charlist()
    |> do_split_top_level_arguments([], [], 0, false, false)
    |> Enum.map(&to_string/1)
    |> Enum.map(&String.trim/1)
  end

  defp do_split_top_level_arguments(
         [],
         current_argument,
         parsed_arguments,
         _depth,
         _in_string?,
         _escaping?
       ) do
    Enum.reverse([Enum.reverse(current_argument) | parsed_arguments])
  end

  defp do_split_top_level_arguments(
         [character | rest],
         current_argument,
         parsed_arguments,
         depth,
         in_string?,
         escaping?
       ) do
    cond do
      in_string? and escaping? ->
        do_split_top_level_arguments(
          rest,
          [character | current_argument],
          parsed_arguments,
          depth,
          true,
          false
        )

      in_string? and character == ?\\ ->
        do_split_top_level_arguments(
          rest,
          [character | current_argument],
          parsed_arguments,
          depth,
          true,
          true
        )

      in_string? and character == ?" ->
        do_split_top_level_arguments(
          rest,
          [character | current_argument],
          parsed_arguments,
          depth,
          false,
          false
        )

      in_string? ->
        do_split_top_level_arguments(
          rest,
          [character | current_argument],
          parsed_arguments,
          depth,
          true,
          false
        )

      character == ?" ->
        do_split_top_level_arguments(
          rest,
          [character | current_argument],
          parsed_arguments,
          depth,
          true,
          false
        )

      character == ?( ->
        do_split_top_level_arguments(
          rest,
          [character | current_argument],
          parsed_arguments,
          depth + 1,
          false,
          false
        )

      character == ?) ->
        do_split_top_level_arguments(
          rest,
          [character | current_argument],
          parsed_arguments,
          depth - 1,
          false,
          false
        )

      character == ?, and depth == 0 ->
        do_split_top_level_arguments(
          rest,
          [],
          [Enum.reverse(current_argument) | parsed_arguments],
          depth,
          false,
          false
        )

      true ->
        do_split_top_level_arguments(
          rest,
          [character | current_argument],
          parsed_arguments,
          depth,
          false,
          false
        )
    end
  end
end
