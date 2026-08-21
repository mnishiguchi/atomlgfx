#!/usr/bin/env elixir

# SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
#
# SPDX-License-Identifier: Apache-2.0

defmodule Main do
  @moduledoc """
  C の情報ファイルから LGFX プロトコル参照表を生成し、文書へ反映します。
  """

  @ops_def_path "lgfx_port/include_internal/lgfx_port/ops.def"

  # 機能ビットとエラー語彙は、このヘッダーで定義する。
  @native_contract_h_path "lgfx_port/include_internal/lgfx_port/protocol.h"

  @protocol_reference_doc_path "docs/protocol-reference.md"
  @generated_ex_path "lib/atom_lgfx/generated.ex"

  @raw_operation_names [
    "startWrite",
    "endWrite"
  ]

  @flag_values %{
    "LGFX_F_TEXT_HAS_BG" => 1,
    "LGFX_F_COLOR_INDEX" => 2,
    "LGFX_F_TEXT_FG_INDEX" => 4,
    "LGFX_F_TEXT_BG_INDEX" => 8,
    "LGFX_F_TRANSPARENT_INDEX" => 16
  }

  @hidden_operation_names [
    "startWrite",
    "endWrite"
  ]

  @script_path "scripts/#{Path.basename(__ENV__.file)}"
  @generated_by_comment "<!-- #{@script_path} により生成 -->"

  @ops_begin_marker "<!-- BEGIN:generated_ops_matrix -->"
  @ops_end_marker "<!-- END:generated_ops_matrix -->"

  @caps_begin_marker "<!-- BEGIN:generated_caps_table -->"
  @caps_end_marker "<!-- END:generated_caps_table -->"

  @errors_begin_marker "<!-- BEGIN:generated_error_reasons_table -->"
  @errors_end_marker "<!-- END:generated_error_reasons_table -->"

  def main(argv) do
    {options, _args, invalid} =
      OptionParser.parse(argv,
        strict: [
          check: :boolean,
          help: :boolean,
          self_test: :boolean
        ]
      )

    cond do
      invalid != [] ->
        {flag, _value} = List.first(invalid)
        IO.puts(:stderr, "不明な選択肢です: #{flag}")
        print_usage()
        System.halt(2)

      Keyword.get(options, :help, false) ->
        print_usage()
        System.halt(0)

      Keyword.get(options, :self_test, false) ->
        self_test!()
        IO.puts("自己検証に成功しました。")

      true ->
        run(check_mode?: Keyword.get(options, :check, false))
    end
  end

  defp print_usage do
    IO.puts("""
    使用方法:
      elixir #{@script_path}
      elixir #{@script_path} --check
      elixir #{@script_path} --self-test
      elixir #{@script_path} --help

    生成・反映する内容:
      - 操作一覧（#{@ops_def_path} から生成）
      - 機能一覧（#{@native_contract_h_path} から生成）
      - エラー理由一覧（#{@native_contract_h_path} から生成）
      - Elixir の生成済み操作定義（#{@ops_def_path} から生成）

    反映先:
      - #{@protocol_reference_doc_path}
      - #{@generated_ex_path}
    """)
  end

  defp run(check_mode?: check_mode?) do
    self_test!()

    ops_def_content = File.read!(@ops_def_path)
    native_contract_h_content = File.read!(@native_contract_h_path)
    protocol_reference_doc_content = File.read!(@protocol_reference_doc_path)
    generated_ex_content = File.read!(@generated_ex_path)

    operations = parse_operations(ops_def_content)
    capabilities = parse_capabilities(native_contract_h_content)
    error_reasons = parse_error_reasons(native_contract_h_content)

    protocol_feature_cap_bits =
      operations
      |> Enum.map(& &1.feature_cap_bit)
      |> Enum.reject(&(&1 == "0"))
      |> MapSet.new()

    ops_matrix_markdown = render_operations_matrix(operations)

    caps_table_markdown =
      render_capabilities_table(capabilities, protocol_feature_cap_bits)

    error_reasons_table_markdown =
      render_error_reasons_table(error_reasons)

    updated_protocol_reference_doc_content =
      protocol_reference_doc_content
      |> inject_generated_block(@ops_begin_marker, @ops_end_marker, ops_matrix_markdown)
      |> inject_generated_block(@caps_begin_marker, @caps_end_marker, caps_table_markdown)
      |> inject_generated_block(
        @errors_begin_marker,
        @errors_end_marker,
        error_reasons_table_markdown
      )

    updated_generated_ex_content = render_generated_ex(operations)

    protocol_reference_doc_stale? =
      updated_protocol_reference_doc_content != protocol_reference_doc_content

    generated_ex_stale? =
      updated_generated_ex_content != generated_ex_content

    if check_mode? do
      stale_messages =
        [
          if(protocol_reference_doc_stale?,
            do:
              "プロトコル参照文書の生成部分が古くなっています。実行してください: elixir #{@script_path}"
          ),
          if(generated_ex_stale?,
            do: "#{@generated_ex_path} が古くなっています。実行してください: elixir #{@script_path}"
          )
        ]
        |> Enum.reject(&is_nil/1)

      case stale_messages do
        [] ->
          IO.puts("生成対象のプロトコルファイルは最新です。")

        messages ->
          Enum.each(messages, &IO.puts(:stderr, &1))
          System.halt(1)
      end
    else
      updated_paths =
        []
        |> maybe_write_file(
          @protocol_reference_doc_path,
          protocol_reference_doc_content,
          updated_protocol_reference_doc_content
        )
        |> maybe_write_file(
          @generated_ex_path,
          generated_ex_content,
          updated_generated_ex_content
        )

      case Enum.reverse(updated_paths) do
        [] ->
          IO.puts("変更はありません。")

        paths ->
          Enum.each(paths, &IO.puts("更新しました: #{&1}"))
      end
    end
  end

  defp self_test! do
    ping_args_blob =
      "ping, 0, 0, 0, LGFX_OP_TARGET_BAD_TARGET, LGFX_OP_STATE_ANY, 0, 0, 0, 1, 0"

    ping_args = split_top_level_arguments(ping_args_blob)

    assert!(length(ping_args) == 11, "split_top_level_arguments/1 は11項目を返す必要があります")
    assert!(Enum.at(ping_args, 0) == "ping", "第1項目（操作）の解析に失敗しました")

    set_text_color_line =
      "X(setTextColor, 1, 2, LGFX_F_TEXT_HAS_BG, LGFX_OP_TARGET_ANY, LGFX_OP_STATE_REQUIRES_INIT, 0, 1, 0, 0, 0)"

    case parse_x_macro_line(set_text_color_line) do
      {:ok, op} ->
        assert!(op.operation_name == "setTextColor", "operation_name の解析に失敗しました")
        assert!(op.min_args == "1", "min_args の解析に失敗しました")
        assert!(op.max_args == "2", "max_args の解析に失敗しました")
        assert!(op.allowed_flags_mask == "LGFX_F_TEXT_HAS_BG", "flags の解析に失敗しました")
        assert!(op.batchable == "1", "batchable の解析に失敗しました")
        assert!(op.needs_owned_payload == "0", "needs_owned_payload の解析に失敗しました")
        assert!(op.sync_only == "0", "sync_only の解析に失敗しました")
        assert!(op.batch_boundary_sensitive == "0", "batch_boundary_sensitive の解析に失敗しました")

      :skip ->
        raise "自己検証失敗: parse_x_macro_line/1 が setTextColor を予期せず読み飛ばしました"
    end

    assert!(
      elixir_operation_name("drawFastVLine") == :draw_fast_vline,
      "drawFastVLine の Elixir 名正規化に失敗しました"
    )

    assert!(
      elixir_operation_name("drawFastHLine") == :draw_fast_hline,
      "drawFastHLine の Elixir 名正規化に失敗しました"
    )

    caps_sample = """
    #define LGFX_CAP_PUSHIMAGE (1u << 1)
    #define LGFX_CAP_SAFE_YIELD_STRICT (1u << 9)
    #define LGFX_CAP_KNOWN_MASK (LGFX_CAP_PUSHIMAGE | LGFX_CAP_SAFE_YIELD_STRICT)
    """

    parsed_caps = parse_capabilities(caps_sample)

    assert!(
      length(parsed_caps) == 2,
      "parse_capabilities/1 は2個の機能ビット定義を解析する必要があります"
    )

    assert!(Enum.at(parsed_caps, 0).c_macro == "LGFX_CAP_PUSHIMAGE", "機能マクロの解析に失敗しました")
    assert!(Enum.at(parsed_caps, 1).shift == 9, "機能ビット位置の解析に失敗しました")

    errors_sample = """
    // Native error atom strings used for documentation/debugging
    #define LGFX_ERR_BAD_ARGS "bad_args"
    #define LGFX_ERR_UNSUPPORTED "unsupported"

    // Optional detail tuple reason tags
    #define LGFX_ERR_BATCH_FAILED "batch_failed"
    """

    parsed_errors = parse_error_reasons(errors_sample)

    assert!(length(parsed_errors) == 3, "parse_error_reasons/1 は3個のエラー定義を解析する必要があります")
    assert!(Enum.at(parsed_errors, 0).kind == :canonical, "正規エラー種別の解析に失敗しました")
    assert!(Enum.at(parsed_errors, 2).kind == :detail_tag, "詳細タグ種別の解析に失敗しました")
  end

  defp assert!(true, _message), do: :ok
  defp assert!(false, message), do: raise("自己検証に失敗しました: #{message}")

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
                min_args,
                max_args,
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
                   min_args: String.trim(min_args),
                   max_args: String.trim(max_args),
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
                raise "X(...) 行の項目数が不正です: #{line}"
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

  defp render_operations_matrix(operations) do
    header_lines = [
      "| 操作 | 対象規則 | フラグ規則 | 引数 | 状態規則 | 機能ビット | バッチ経路 |",
      "| --- | --- | --- | --- | --- | --- | --- |"
    ]

    row_lines =
      Enum.map(operations, fn operation ->
        operation_name = "`#{elixir_operation_name(operation.operation_name)}`"
        target_rule = "`#{target_rule_label(operation.target_policy)}`"
        flags_rule = "`#{flags_rule_label(operation.allowed_flags_mask)}`"
        arity = "`#{args_count_label(operation.min_args, operation.max_args)}`"
        state_rule = "`#{state_rule_label(operation.state_policy)}`"
        feature_cap_bit = capability_bit_label(operation.feature_cap_bit)
        batch_path = batch_path_label(operation)

        "| #{operation_name} | #{target_rule} | #{flags_rule} | #{arity} | #{state_rule} | #{feature_cap_bit} | #{batch_path} |"
      end)

    Enum.join(header_lines ++ row_lines, "\n")
  end

  defp target_rule_label("LGFX_OP_TARGET_BAD_TARGET"), do: "T0/bad_target"
  defp target_rule_label("LGFX_OP_TARGET_UNSUPPORTED"), do: "T0/unsupported"
  defp target_rule_label("LGFX_OP_TARGET_ANY"), do: "Tany"
  defp target_rule_label("LGFX_OP_TARGET_SPRITE_ONLY"), do: "Tsprite"
  defp target_rule_label(other), do: other

  defp flags_rule_label("0"), do: "F0"
  defp flags_rule_label(other), do: "Fmask(#{other})"

  defp args_count_label(min_args, max_args) do
    arity_label(min_args, max_args)
  end

  defp arity_label(min_arity, max_arity) when min_arity == max_arity, do: min_arity
  defp arity_label(min_arity, max_arity), do: "#{min_arity}/#{max_arity}"

  defp state_rule_label("LGFX_OP_STATE_ANY"), do: "any"
  defp state_rule_label("LGFX_OP_STATE_REQUIRES_INIT"), do: "requires_init"
  defp state_rule_label(other), do: other

  defp capability_bit_label("0"), do: "-"
  defp capability_bit_label(other), do: "`#{other}`"

  defp batch_path_label(operation) do
    flags =
      []
      |> maybe_add_batch_path_flag(operation.batchable == "1", "batch")
      |> maybe_add_batch_path_flag(operation.needs_owned_payload == "1", "payload")
      |> maybe_add_batch_path_flag(operation.sync_only == "1", "sync")
      |> maybe_add_batch_path_flag(operation.batch_boundary_sensitive == "1", "boundary")

    case Enum.reverse(flags) do
      [] -> "-"
      labels -> Enum.map_join(labels, "<br>", &"`#{&1}`")
    end
  end

  defp maybe_add_batch_path_flag(flags, true, label), do: [label | flags]
  defp maybe_add_batch_path_flag(flags, false, _label), do: flags

  defp parse_capabilities(caps_h_content) do
    caps_h_content
    |> String.split("\n")
    |> Enum.reduce([], fn line, acc ->
      trimmed_line = String.trim(line)

      case Regex.run(
             ~r/^#define\s+(LGFX_CAP_[A-Z0-9_]+)\s+\(\s*1u\s*<<\s*(\d+)\s*\)\s*$/,
             trimmed_line,
             capture: :all_but_first
           ) do
        [c_macro, shift_str] ->
          [
            %{
              c_macro: c_macro,
              shift: String.to_integer(shift_str)
            }
            | acc
          ]

        nil ->
          acc
      end
    end)
    |> Enum.reverse()
  end

  defp render_capabilities_table(capabilities, protocol_feature_cap_bits) do
    header_lines = [
      "| C マクロ | プロトコルビット | 桁位置 | 値 | 定義元 |",
      "| --- | --- | --- | --- | --- |"
    ]

    row_lines =
      Enum.map(capabilities, fn capability ->
        c_macro = capability.c_macro
        shift = capability.shift
        protocol_bit = c_macro |> String.replace_prefix("LGFX_", "")
        value_hex = capability_value_hex(shift)
        source = capability_source_label(c_macro, protocol_feature_cap_bits)

        "| `#{c_macro}` | `#{protocol_bit}` | `#{shift}` | `#{value_hex}` | #{source} |"
      end)

    Enum.join(header_lines ++ row_lines, "\n")
  end

  defp capability_value_hex(shift) do
    value = :erlang.bsl(1, shift)

    "0x" <>
      (value
       |> Integer.to_string(16)
       |> String.upcase()
       |> String.pad_leading(4, "0"))
  end

  defp capability_source_label(c_macro, protocol_feature_cap_bits) do
    cond do
      MapSet.member?(protocol_feature_cap_bits, c_macro) ->
        "`ops.def` の `feature_cap_bit`"

      c_macro in ["LGFX_CAP_SAFE_YIELD_FORGIVING", "LGFX_CAP_SAFE_YIELD_STRICT"] ->
        "構築設定 (`LGFX_PORT_SAFE_YIELD_CAP`)"

      true ->
        "予約済み / 現在は操作と未関連"
    end
  end

  defp parse_error_reasons(errors_h_content) do
    errors_h_content
    |> String.split("\n")
    |> Enum.reduce({[], :canonical}, fn line, {acc, current_kind} ->
      trimmed_line = String.trim(line)

      cond do
        String.contains?(trimmed_line, "protocol-level error atom") ->
          {acc, :canonical}

        String.contains?(trimmed_line, "Optional detail tuple reason tags") ->
          {acc, :detail_tag}

        true ->
          case Regex.run(
                 ~r/^#define\s+(LGFX_ERR_[A-Z0-9_]+)\s+"([^"]+)"\s*$/,
                 trimmed_line,
                 capture: :all_but_first
               ) do
            [c_macro, atom_name] ->
              {
                [
                  %{
                    c_macro: c_macro,
                    atom: atom_name,
                    kind: current_kind
                  }
                  | acc
                ],
                current_kind
              }

            nil ->
              {acc, current_kind}
          end
      end
    end)
    |> elem(0)
    |> Enum.reverse()
  end

  defp render_error_reasons_table(error_reasons) do
    header_lines = [
      "| C マクロ | アトム | 種別 |",
      "| --- | --- | --- |"
    ]

    row_lines =
      Enum.map(error_reasons, fn error_reason ->
        c_macro = error_reason.c_macro
        atom_name = error_reason.atom
        kind = error_reason_kind_label(error_reason.kind)

        "| `#{c_macro}` | `#{atom_name}` | `#{kind}` |"
      end)

    Enum.join(header_lines ++ row_lines, "\n")
  end

  defp error_reason_kind_label(:canonical), do: "正規"
  defp error_reason_kind_label(:detail_tag), do: "詳細タグ"
  defp error_reason_kind_label(other), do: to_string(other)

  defp maybe_write_file(updated_paths, path, old_content, new_content) do
    if old_content == new_content do
      updated_paths
    else
      File.write!(path, new_content)
      [path | updated_paths]
    end
  end

  defp render_generated_ex(operations) do
    operation_rows =
      operations
      |> Enum.with_index()
      |> Enum.map(fn {operation, opcode} ->
        render_generated_ex_operation(operation, opcode)
      end)
      |> Enum.join("\n")

    source = """
    # SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
    #
    # SPDX-License-Identifier: Apache-2.0

    # このファイルは #{@script_path} により生成される。
    # 変更する場合は #{@ops_def_path} を編集する。

    defmodule AtomLGFX.Generated do
      @moduledoc false

      @ops [
    #{operation_rows}
      ]

      @ops_by_name Map.new(@ops)
      @names_by_opcode Map.new(@ops, fn {name, meta} -> {Keyword.fetch!(meta, :opcode), name} end)
      def ops, do: @ops

      def meta(name) when is_atom(name) do
        with {:ok, canonical_name} <- canonical_name(name),
             {:ok, meta} <- Map.fetch(@ops_by_name, canonical_name) do
          {:ok, meta}
        else
          :error -> {:error, {:unknown_lgfx_op, name}}
        end
      end

      def opcode(name) when is_atom(name) do
        fetch_meta_value(name, :opcode)
      end

      def opcode!(name) do
        case opcode(name) do
          {:ok, opcode} -> opcode
          {:error, reason} -> raise ArgumentError, "unknown lgfx op: \#{inspect(reason)}"
        end
      end

      def name(opcode) when is_integer(opcode) do
        Map.fetch(@names_by_opcode, opcode)
      end

      def canonical_name(name) when is_atom(name) do
        if Map.has_key?(@ops_by_name, name), do: {:ok, name}, else: :error
      end

      def elixir_name(name) when is_atom(name) do
        case canonical_name(name) do
          {:ok, canonical_name} -> {:ok, canonical_name}
          :error -> {:error, {:unknown_lgfx_op, name}}
        end
      end

      def public?(name), do: flag?(name, :public)

      def raw?(name), do: flag?(name, :raw)

      def batchable?(name), do: flag?(name, :batchable)

      def needs_owned_payload?(name), do: flag?(name, :needs_owned_payload)

      def sync_only?(name), do: flag?(name, :sync_only)

      def batch_boundary_sensitive?(name), do: flag?(name, :batch_boundary_sensitive)

      def arg_range(name), do: fetch_meta_value(name, :arg_range)

      def allowed_flags(name), do: fetch_meta_value(name, :allowed_flags)

      def target_policy(name), do: fetch_meta_value(name, :target_policy)

      def state_policy(name), do: fetch_meta_value(name, :state_policy)

      def capability(name), do: fetch_meta_value(name, :capability)

      defp fetch_meta_value(name, key) when is_atom(name) and is_atom(key) do
        with {:ok, meta} <- meta(name) do
          {:ok, Keyword.fetch!(meta, key)}
        end
      end

      defp flag?(name, flag) when is_atom(name) do
        with {:ok, meta} <- meta(name) do
          Keyword.fetch!(meta, flag)
        else
          {:error, _reason} -> false
        end
      end
    end
    """

    source
    |> Code.format_string!()
    |> IO.iodata_to_binary()
    |> ensure_trailing_newline()
  end

  defp ensure_trailing_newline(content) do
    if String.ends_with?(content, "\n") do
      content
    else
      content <> "\n"
    end
  end

  defp render_generated_ex_operation(operation, opcode) do
    elixir_name = operation.operation_name |> elixir_operation_name() |> Atom.to_string()
    public? = generated_public?(operation)
    raw? = generated_raw?(operation)

    """
        #{elixir_name}: [
          opcode: #{opcode},
          public: #{public?},
          raw: #{raw?},
          arg_range: #{generated_arg_range(operation)},
          allowed_flags: #{generated_allowed_flags(operation.allowed_flags_mask)},
          target_policy: #{inspect(generated_target_policy(operation.target_policy))},
          state_policy: #{inspect(generated_state_policy(operation.state_policy))},
          capability: #{inspect(generated_capability(operation.feature_cap_bit))},
          batchable: #{generated_bool(operation.batchable)},
          needs_owned_payload: #{generated_bool(operation.needs_owned_payload)},
          sync_only: #{generated_bool(operation.sync_only)},
          batch_boundary_sensitive: #{generated_bool(operation.batch_boundary_sensitive)}
        ],
    """
  end

  defp generated_bool("0"), do: false
  defp generated_bool("1"), do: true

  defp generated_bool(value) do
    raise "ops.def の真偽メタデータには 0 または 1 が必要です: #{value}"
  end

  defp generated_arg_range(operation) do
    "#{operation.min_args}..#{operation.max_args}"
  end

  defp generated_allowed_flags("0"), do: 0

  defp generated_allowed_flags(mask) do
    @flag_values
    |> Enum.reduce(0, fn {flag_name, flag_value}, acc ->
      if String.contains?(mask, flag_name) do
        Bitwise.bor(acc, flag_value)
      else
        acc
      end
    end)
  end

  defp generated_target_policy("LGFX_OP_TARGET_BAD_TARGET"), do: :bad_target
  defp generated_target_policy("LGFX_OP_TARGET_ANY"), do: :any
  defp generated_target_policy("LGFX_OP_TARGET_SPRITE_ONLY"), do: :sprite_only

  defp generated_target_policy(target_policy) do
    raise "ops.def に未知の対象規則があります: #{target_policy}"
  end

  defp generated_state_policy("LGFX_OP_STATE_ANY"), do: :any
  defp generated_state_policy("LGFX_OP_STATE_REQUIRES_INIT"), do: :requires_init

  defp generated_state_policy(state_policy) do
    raise "ops.def に未知の状態規則があります: #{state_policy}"
  end

  defp generated_capability("0"), do: nil
  defp generated_capability("LGFX_CAP_SPRITE"), do: :sprite
  defp generated_capability("LGFX_CAP_PUSHIMAGE"), do: :pushimage
  defp generated_capability("LGFX_CAP_LAST_ERROR"), do: :last_error
  defp generated_capability("LGFX_CAP_TOUCH"), do: :touch
  defp generated_capability("LGFX_CAP_PALETTE"), do: :palette
  defp generated_capability("LGFX_CAP_BATCH"), do: :batch

  defp generated_capability(feature_cap_bit) do
    raise "ops.def に未知の機能があります: #{feature_cap_bit}"
  end

  defp elixir_operation_name(operation_name) do
    operation_name
    |> String.replace("VLine", "Vline")
    |> String.replace("HLine", "Hline")
    |> Macro.underscore()
    |> String.to_atom()
  end

  defp generated_public?(operation) do
    operation.operation_name not in @hidden_operation_names
  end

  defp generated_raw?(operation) do
    operation.operation_name in @raw_operation_names
  end

  defp inject_generated_block(document_content, begin_marker, end_marker, generated_content) do
    unless String.contains?(document_content, begin_marker) and
             String.contains?(document_content, end_marker) do
      raise "生成部分の目印が見つかりません: #{begin_marker} ... #{end_marker}"
    end

    [before_begin_marker, begin_and_after] =
      String.split(document_content, begin_marker, parts: 2)

    [_old_generated_block, after_end_marker] =
      String.split(begin_and_after, end_marker, parts: 2)

    generated_block_content =
      [
        begin_marker,
        @generated_by_comment,
        "",
        generated_content,
        end_marker
      ]
      |> Enum.join("\n")

    before_begin_marker <> generated_block_content <> after_end_marker
  end
end

Main.main(System.argv())
