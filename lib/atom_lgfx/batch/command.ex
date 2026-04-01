# SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
#
# SPDX-License-Identifier: Apache-2.0

defmodule AtomLGFX.Batch.Command do
  @moduledoc """
  Explicit batch command descriptor.

  This first Elixir batching slice keeps the command envelope generic.
  Operation-specific builders can be added separately without changing the
  batch container or protocol submit path.
  """

  import AtomLGFX.Guards

  @type t :: {:cmd, atom, 0..254, non_neg_integer, [term]}

  @spec new(atom, 0..254, non_neg_integer, [term]) :: {:ok, t} | {:error, term}
  def new(op, target, flags, args)
      when is_atom(op) and target_any(target) and is_integer(flags) and flags >= 0 and
             is_list(args) do
    {:ok, {:cmd, op, target, flags, args}}
  end

  def new(op, target, flags, args) do
    {:error, {:bad_batch_command, {op, target, flags, args}}}
  end

  @spec to_wire(t) :: {:ok, tuple} | {:error, term}
  def to_wire({:cmd, op, target, flags, args})
      when is_atom(op) and target_any(target) and is_integer(flags) and flags >= 0 and
             is_list(args) do
    {:ok, List.to_tuple([op, target, flags | args])}
  end

  def to_wire(other) do
    {:error, {:bad_batch_command, other}}
  end
end
