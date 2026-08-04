# SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
#
# SPDX-License-Identifier: Apache-2.0

defmodule AtomLGFX.BinaryBatch.Validation do
  @moduledoc false

  alias AtomLGFX.BinaryBatch.Codec
  alias AtomLGFX.Errors

  @doc """
  バイナリーバッチ命令列を送信せずに検証します。
  """
  @spec validate(iodata()) :: :ok | {:error, term()}
  def validate(commands) do
    case Codec.decode(commands) do
      {:ok, _decoded_commands} -> :ok
      {:error, reason} -> {:error, reason}
    end
  end

  @doc """
  バイナリーバッチ命令列を検証し、不正な場合は `ArgumentError` を送出します。
  """
  @spec validate!(iodata()) :: :ok
  def validate!(commands) do
    case validate(commands) do
      :ok -> :ok
      {:error, reason} -> raise ArgumentError, Errors.format_error(reason)
    end
  end
end
