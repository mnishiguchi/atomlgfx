# SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
#
# SPDX-License-Identifier: Apache-2.0

defmodule AtomLGFX.Raw do
  @moduledoc """
  Explicit escape hatch for low-level LovyanGFX protocol calls.

  This module bypasses the curated public-operation filter used by
  `AtomLGFX.call/4`. It still requires operation names to be known atoms from
  the generated opcode table.
  """

  alias AtomLGFX.OpSchema
  alias AtomLGFX.Protocol

  @doc """
  Calls a known operation through the raw v3 call protocol.
  """
  def call(port, op_name, args \\ [], opts \\ [])

  def call(port, op_name, args, opts) when is_atom(op_name) and is_list(args) and is_list(opts) do
    with {:ok, canonical_name} <- OpSchema.canonical_name(op_name) do
      target = Keyword.get(opts, :target, 0)
      flags = Keyword.get(opts, :flags, 0)
      timeout = Keyword.get(opts, :timeout, Protocol.long_timeout())

      Protocol.raw_call(port, canonical_name, target, flags, args, timeout)
    else
      :error -> {:error, {:unknown_lgfx_op, op_name}}
    end
  end
end
