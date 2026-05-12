# SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
#
# SPDX-License-Identifier: Apache-2.0

defmodule AtomLGFX.OpSchema do
  @moduledoc false

  import Bitwise

  alias AtomLGFX.Generated

  @type op_name :: atom()
  @type op_meta ::
          [
            opcode: non_neg_integer(),
            public: boolean(),
            raw: boolean(),
            arg_range: Range.t(),
            allowed_flags: non_neg_integer(),
            target_policy: :bad_target | :any | :sprite_only,
            state_policy: :any | :requires_init,
            capability:
              nil
              | :sprite
              | :pushimage
              | :last_error
              | :touch
              | :palette
              | :batch,
            batchable: boolean(),
            needs_owned_payload: boolean(),
            sync_only: boolean(),
            batch_boundary_sensitive: boolean()
          ]

  @ops Generated.ops()

  @ops_by_name Map.new(@ops)
  @names_by_opcode Map.new(@ops, fn {name, meta} -> {Keyword.fetch!(meta, :opcode), name} end)

  @spec ops() :: [{op_name(), op_meta()}]
  def ops, do: @ops

  @spec meta(atom()) :: {:ok, op_meta()} | {:error, {:unknown_lgfx_op, atom()}}
  def meta(name) when is_atom(name) do
    with {:ok, canonical_name} <- canonical_name(name),
         {:ok, meta} <- Map.fetch(@ops_by_name, canonical_name) do
      {:ok, meta}
    else
      :error -> {:error, {:unknown_lgfx_op, name}}
    end
  end

  @spec opcode(atom()) :: {:ok, non_neg_integer()} | {:error, {:unknown_lgfx_op, atom()}}
  def opcode(name) when is_atom(name), do: fetch_meta_value(name, :opcode)

  @spec opcode!(atom()) :: non_neg_integer()
  def opcode!(name) do
    case opcode(name) do
      {:ok, opcode} -> opcode
      {:error, reason} -> raise ArgumentError, AtomLGFX.Errors.format_error(reason)
    end
  end

  @spec name(integer()) :: {:ok, op_name()} | :error
  def name(opcode) when is_integer(opcode) do
    Map.fetch(@names_by_opcode, opcode)
  end

  @spec canonical_name(atom()) :: {:ok, op_name()} | :error
  def canonical_name(name) when is_atom(name) do
    if Map.has_key?(@ops_by_name, name), do: {:ok, name}, else: :error
  end

  @spec elixir_name(atom()) :: {:ok, op_name()} | {:error, term()}
  def elixir_name(name) when is_atom(name) do
    case canonical_name(name) do
      {:ok, canonical_name} -> {:ok, canonical_name}
      :error -> {:error, {:unknown_lgfx_op, name}}
    end
  end

  @spec public?(atom()) :: boolean()
  def public?(name), do: flag?(name, :public)

  @spec raw?(atom()) :: boolean()
  def raw?(name), do: flag?(name, :raw)

  @spec batchable?(atom()) :: boolean()
  def batchable?(name), do: flag?(name, :batchable)

  @spec needs_owned_payload?(atom()) :: boolean()
  def needs_owned_payload?(name), do: flag?(name, :needs_owned_payload)

  @spec sync_only?(atom()) :: boolean()
  def sync_only?(name), do: flag?(name, :sync_only)

  @spec batch_boundary_sensitive?(atom()) :: boolean()
  def batch_boundary_sensitive?(name), do: flag?(name, :batch_boundary_sensitive)

  @spec arg_range(atom()) :: {:ok, Range.t()} | {:error, {:unknown_lgfx_op, atom()}}
  def arg_range(name) when is_atom(name), do: fetch_meta_value(name, :arg_range)

  @spec allowed_flags(atom()) :: {:ok, non_neg_integer()} | {:error, {:unknown_lgfx_op, atom()}}
  def allowed_flags(name) when is_atom(name), do: fetch_meta_value(name, :allowed_flags)

  @spec validate_wire_call(atom(), [term()], non_neg_integer()) :: :ok | {:error, term()}
  def validate_wire_call(name, args, flags)
      when is_atom(name) and is_list(args) and is_integer(flags) and flags >= 0 do
    with {:ok, canonical_name} <- canonical_name(name),
         :ok <- validate_arg_count(canonical_name, length(args)),
         :ok <- validate_flags(canonical_name, flags) do
      :ok
    else
      :error -> {:error, {:unknown_lgfx_op, name}}
      {:error, _reason} = error -> error
    end
  end

  def validate_wire_call(name, args, flags) do
    {:error, {:bad_lgfx_call, name, args, flags}}
  end

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

  defp validate_arg_count(name, count) do
    range = Keyword.fetch!(Map.fetch!(@ops_by_name, name), :arg_range)

    if count in range do
      :ok
    else
      {:error, {:bad_lgfx_arg_count, name, range.first, range.last, count}}
    end
  end

  defp validate_flags(name, flags) do
    allowed = Keyword.fetch!(Map.fetch!(@ops_by_name, name), :allowed_flags)

    if (flags &&& bnot(allowed)) == 0 do
      :ok
    else
      {:error, {:bad_lgfx_flags, name, allowed, flags}}
    end
  end
end
