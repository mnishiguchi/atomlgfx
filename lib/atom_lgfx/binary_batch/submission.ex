# SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
#
# SPDX-License-Identifier: Apache-2.0

defmodule AtomLGFX.BinaryBatch.Submission do
  @moduledoc false

  alias AtomLGFX.BinaryBatch.Codec
  alias AtomLGFX.BinaryBatch.Validation

  @doc """
  バイナリーバッチ命令列を送信します。
  """
  @spec render(port(), iodata()) :: :ok | {:error, term()}
  def render(port, commands) do
    command_binary = Codec.batch(commands)
    submit_binary(port, command_binary)
  end

  @doc """
  バイナリーバッチ命令列を検証してから送信します。
  """
  @spec render_checked(port(), iodata()) :: :ok | {:error, term()}
  def render_checked(port, commands) do
    command_binary = Codec.batch(commands)

    with :ok <- Validation.validate(command_binary) do
      submit_binary(port, command_binary)
    end
  end

  defp submit_binary(port, command_binary) do
    case AtomLGFX.submit_binary_batch(port, command_binary) do
      {:ok, :ok} -> :ok
      {:ok, other} -> {:error, {:unexpected_reply, other}}
      {:error, reason} -> {:error, reason}
    end
  end
end
