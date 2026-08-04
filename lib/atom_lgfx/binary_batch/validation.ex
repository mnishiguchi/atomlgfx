# SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
#
# SPDX-License-Identifier: Apache-2.0

defmodule AtomLGFX.BinaryBatch.Validation do
  @moduledoc false

  alias AtomLGFX.BinaryBatch.Codec
  alias AtomLGFX.Errors

  @doc """
  Validates a binary-batch command stream without submitting it.

  This performs the same Elixir-side structural decode used by `decode/1`, but
  returns only `:ok` or `{:error, reason}`. Use this for tests, generated-frame
  guardrails, and optional preflight checks around risky frame construction.
  """
  @spec validate(iodata()) :: :ok | {:error, term()}
  def validate(commands) do
    case Codec.decode(commands) do
      {:ok, _decoded_commands} -> :ok
      {:error, reason} -> {:error, reason}
    end
  end

  @doc """
  Validates a binary-batch command stream or raises `ArgumentError`.
  """
  @spec validate!(iodata()) :: :ok
  def validate!(commands) do
    case validate(commands) do
      :ok -> :ok
      {:error, reason} -> raise ArgumentError, Errors.format_error(reason)
    end
  end
end
