# SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
#
# SPDX-License-Identifier: Apache-2.0

defmodule AtomLGFX.Batch do
  @moduledoc false

  alias AtomLGFX.Batch.Command
  alias AtomLGFX.Protocol

  defstruct commands: []

  @type t :: %__MODULE__{commands: [Command.t()]}

  @spec new() :: t
  def new, do: %__MODULE__{}

  @spec add(t, Command.t() | {:ok, Command.t()} | {:error, term}) :: t | {:error, term}
  def add(%__MODULE__{} = batch, {:ok, command}) do
    add(batch, command)
  end

  def add(%__MODULE__{commands: commands} = batch, {:cmd, _, _, _, _} = command) do
    %{batch | commands: [command | commands]}
  end

  def add(%__MODULE__{}, {:error, _reason} = error), do: error

  def add(%__MODULE__{}, other) do
    {:error, {:bad_batch_command, other}}
  end

  @spec to_commands(t) :: [Command.t()]
  def to_commands(%__MODULE__{commands: commands}), do: :lists.reverse(commands)

  @spec submit(port(), t | [Command.t()]) :: {:ok, term} | {:error, term}
  def submit(port, %__MODULE__{} = batch) do
    submit(port, to_commands(batch))
  end

  def submit(_port, []), do: {:error, :empty_batch}

  def submit(port, commands) when is_list(commands) do
    with {:ok, wire_commands} <- commands_to_wire(commands) do
      Protocol.submit_batch(port, wire_commands, Protocol.long_timeout())
    end
  end

  defp commands_to_wire(commands) when is_list(commands) do
    commands_to_wire(commands, [])
  end

  defp commands_to_wire([], acc) do
    {:ok, :lists.reverse(acc)}
  end

  defp commands_to_wire([command | rest], acc) do
    case Command.to_wire(command) do
      {:ok, wire_command} ->
        commands_to_wire(rest, [wire_command | acc])

      {:error, _reason} = error ->
        error
    end
  end
end
