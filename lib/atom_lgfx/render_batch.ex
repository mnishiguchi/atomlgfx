# SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
#
# SPDX-License-Identifier: Apache-2.0

defmodule AtomLGFX.RenderBatch do
  @moduledoc false

  alias AtomLGFX.BinaryBatch
  alias AtomLGFX.Command

  @type command :: Command.command()
  @type normalized_command :: Command.normalized_command()

  @doc """
  描画命令を正規化し、1つのバイナリーバッチ命令列へ符号化します。
  """
  @spec encode([command()], keyword()) :: {:ok, binary()} | {:error, term()}
  def encode(commands, opts \\ [])

  def encode(commands, opts) when is_list(commands) and is_list(opts) do
    with {:ok, normalized_commands} <- Command.normalize(commands, opts) do
      encode_normalized(normalized_commands)
    end
  end

  def encode(commands, opts) when is_list(commands), do: {:error, {:bad_render_options, opts}}
  def encode(commands, _opts), do: {:error, {:bad_render_commands, commands}}

  @doc """
  正規化済み命令を1つのバイナリーバッチ命令列へ符号化します。
  """
  @spec encode_normalized([normalized_command()]) :: {:ok, binary()} | {:error, term()}
  defdelegate encode_normalized(commands), to: AtomLGFX.RenderBatch.Encoder

  @doc """
  既存のバイナリーバッチ用ポート経路を通じて描画命令を送信します。
  """
  @spec render(port(), [command()], keyword()) :: :ok | {:error, term()}
  def render(port, commands, opts \\ [])

  def render(port, commands, opts) when is_list(commands) and is_list(opts) do
    with {:ok, command_binary} <- encode(commands, opts) do
      submit(port, command_binary, opts)
    end
  end

  def render(_port, commands, opts) when is_list(commands),
    do: {:error, {:bad_render_options, opts}}

  def render(_port, commands, _opts), do: {:error, {:bad_render_commands, commands}}

  @doc """
  構築済みのバイナリーバッチ命令列を送信します。
  """
  @spec submit(port(), iodata(), keyword()) :: :ok | {:error, term()}
  def submit(port, commands, opts \\ [])

  def submit(port, commands, opts) when is_list(opts) do
    case validate_option(opts) do
      {:ok, true} -> BinaryBatch.render_checked(port, commands)
      {:ok, false} -> BinaryBatch.render(port, commands)
      {:error, reason} -> {:error, reason}
    end
  end

  def submit(_port, _commands, opts), do: {:error, {:bad_render_options, opts}}

  defp validate_option(opts) do
    case Keyword.get(opts, :validate, false) do
      validate? when is_boolean(validate?) -> {:ok, validate?}
      validate? -> {:error, {:bad_render_validate_option, validate?}}
    end
  end
end
