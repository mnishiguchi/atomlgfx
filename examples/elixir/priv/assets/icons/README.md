<!--
SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi

SPDX-License-Identifier: Apache-2.0
-->

# Icon assets

This directory contains raw RGB565 icon assets used by the Elixir examples.

## Files

- `info.rgb565`
- `alert.rgb565`
- `close.rgb565`
- `piyopiyo.rgb565`

## Format

Each file is:

- raw RGB565 pixel data
- 32 x 32 pixels
- 2 bytes per pixel
- 2048 bytes total
- stored as ordinary little-endian 16-bit RGB565 words

These files are intended to be passed through as-is from Elixir.

Do not byte-swap them in `SampleApp.Assets`.

## Runtime contract

When loading these files with `AtomLGFX.push_image_rgb565/8`:

- treat the asset bytes as ordinary RGB565 payloads
- enable `set_swap_bytes(port, true, target)` on the destination sprite before upload
- restore swap mode afterward if needed by later operations

This matches the finalized `pushImage` contract used by the examples.

## MovingIcons note

`SampleApp.MovingIcons` loads these files into icon sprites and uses RGB565 `0x0000` as the transparent key during sprite rendering.

That means the icon artwork should keep `0x0000` reserved for transparent background pixels.

## Regenerating assets

If these assets are regenerated, keep the same contract:

- 32 x 32
- raw RGB565
- little-endian word order
- background color `0x0000` when transparency is needed

If the byte order changes, `MovingIcons` colors will look wrong.
