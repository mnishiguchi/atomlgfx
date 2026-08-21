# SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
#
# SPDX-License-Identifier: Apache-2.0

defmodule AtomLGFX.Raw do
  @moduledoc """
  低水準の LovyanGFX ネイティブ操作を明示的に行うための窓口です。

  このモジュールは `AtomLGFX.call/4` が使用する公開操作の選別を迂回します。ただし、操作名には生成済み操作コード表に存在する既知のアトムだけを指定できます。
  """

  alias AtomLGFX.OpSchema
  alias AtomLGFX.Protocol

  @doc """
  共通 NIF 振り分けを通じて、既知の操作を呼び出します。
  """
  def call(handle, op_name, args \\ [], opts \\ [])

  def call(handle, op_name, args, opts)
      when is_atom(op_name) and is_list(args) and is_list(opts) do
    with {:ok, canonical_name} <- OpSchema.canonical_name(op_name) do
      target = Keyword.get(opts, :target, 0)
      flags = Keyword.get(opts, :flags, 0)
      Protocol.raw_call(handle, canonical_name, target, flags, args)
    else
      :error -> {:error, {:unknown_lgfx_op, op_name}}
    end
  end
end
