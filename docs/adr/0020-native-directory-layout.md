# ADR 0020: ネイティブ実装のディレクトリー構成を整理する

## Status

Accepted

## Context

AtomLGFX は旧 AtomVM Port 実装を廃止し、ネイティブ実行経路を NIF に統一した。

一方、現在の構成には過去の実装を反映した名称が残っていた。

    lgfx_port/
    lgfx_device/

`lgfx_port/` は AtomVM Port を含まず、NIF、実行時設定、バイナリーバッチを保持しているため、名称と実態が一致していなかった。

また、C/C++ の実装が複数のトップレベルディレクトリーに分かれていた。

## Decision

ネイティブ実装を `native/` 配下へ集約する。

    native/
      nif.c
      open_config.c
      render_batch.cpp

      cmake/
        lgfx_port_config.h.in

      include/
        atom_lgfx/
          constants.h
          nif.inc
          nif_dispatch.inc
          open_config.h
          ops.def
          ops.h
          render_batch.h

      device/
        device.h
        device_internal.hpp
        state_runtime.hpp
        state.cpp
        state_runtime.cpp
        control.cpp
        primitives.cpp
        text.cpp
        images.cpp
        clip.cpp
        sprites.cpp
        fonts/
          generated/

内部の include namespace も `lgfx_port/...` から、ライブラリー名を表す `atom_lgfx/...` へ変更する。

例:

    #include "atom_lgfx/ops.h"
    #include "atom_lgfx/constants.h"

LovyanGFX 装置層の内部ヘッダーは `native/device/` に置く。

## Scope

この変更では主にファイル配置と名称を整理する。

以下の構築設定名は変更しない。

- `LGFX_PORT_*`
- `lgfx_port_*` CMake 関数
- `lgfx_port_config.h`

これらの名称変更は別の変更として扱う。

これにより、ディレクトリー移動と構築設定名の変更を分離する。

## Consequences

### Positive

- `native/` が C/C++ 実装の明確な境界になる
- 廃止済みの AtomVM Port を連想するディレクトリー名を除去できる
- NIF と LovyanGFX 装置層の関係が理解しやすくなる
- トップレベルのディレクトリー構成を単純化できる

### Negative

- CMake と include path の変更が必要になる
- 下流で従来のソースパスを直接参照している場合は更新が必要になる
- `LGFX_PORT_*` など一部の歴史的な名称は引き続き残る

## Result

リポジトリーの大きな境界は次のようになる。

    lib/       Elixir
    native/    C / C++ / AtomVM NIF / LovyanGFX integration
    test/
    docs/
    examples/
    scripts/

ネイティブ実装の配置は、過去の transport ではなく現在の責務を表す名称とする。
