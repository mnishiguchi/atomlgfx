# SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
#
# SPDX-License-Identifier: Apache-2.0

defmodule AtomLGFX.InstanceBuffer do
  @moduledoc """
  Retained native instance-buffer helpers.

  Instance buffers hold compact per-object records used by retained render
  scenes. The first supported layout is `:sprite_transform_2d`.
  """

  alias AtomLGFX.ObjectBuffers

  def create(port, opts) when is_list(opts), do: ObjectBuffers.create_object_buffer(port, opts)

  def write(port, handle, instances),
    do: ObjectBuffers.write_object_buffer(port, handle, instances)

  def delete(port, handle), do: ObjectBuffers.delete_object_buffer(port, handle)
end
