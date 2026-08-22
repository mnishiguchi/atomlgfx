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
  @spec render(reference(), iodata()) :: :ok | {:error, term()}
  def render(handle, commands) do
    command_binary = Codec.batch(commands)
    submit_binary(handle, command_binary)
  end

  @doc """
  バイナリーバッチ命令列を検証してから送信します。
  """
  @spec render_checked(reference(), iodata()) :: :ok | {:error, term()}
  def render_checked(handle, commands) do
    command_binary = Codec.batch(commands)

    with :ok <- Validation.validate(command_binary) do
      submit_binary(handle, command_binary)
    end
  end

  defp submit_binary(handle, command_binary) do
    case AtomLGFX.submit_binary_batch(handle, command_binary) do
      :ok -> :ok
      {:error, reason} -> {:error, reason}
      other -> {:error, {:unexpected_reply, other}}
    end
  end
end
