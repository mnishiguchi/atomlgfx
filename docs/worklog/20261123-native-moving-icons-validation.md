# 2026-11-23 ネイティブ MovingIcons の実機検証

## 目的

LovyanGFX の [MovingIcons](https://github.com/lovyan03/LovyanGFX/blob/master/examples/Sprite/MovingIcons/MovingIcons.ino) を、AtomVM 上の Elixir から起動できるようにする。
既存の Elixir 版（6個・5 FPS 上限）は残し、50個のアイコンの移動・回転・拡大縮小を C++ / FreeRTOS で実行する `moving_icons_native` を追加した（Issue #12）。

## 環境

- XIAO ESP32-S3（8 MB flash / 8 MB PSRAM）、ILI9488、回転1で480x320、SPI 60 MHz
- AtomVM `0.7.0-beta.0+git.e2cacc99`、ESP-IDF v5.5、CPU 160 MHz

## 経過と結果

- `mix format` 後の `just check` は **109 tests、0 failures**。
- Elixir アプリのみを先に書き込んだ際は `Return value: error` で終了。カスタム AtomVM ファームウェアも更新すると、NIF 経由で描画を起動できた。初回エラーの原因は未確定。
- 当初の30秒の自動終了を Elixir 側で撤廃し、アプリのみを再書き込みして継続動作を再確認。

```text
selected rotation=1 viewport=480x320
moving_icons_native stats obj_count=50 fps=8 average_fps=8 frames=251 elapsed_ms=30318 last_frame_us=107378
moving_icons_native stats obj_count=50 fps=7 average_fps=8 frames=259 elapsed_ms=31327 last_frame_us=106528
```

31秒を超えて統計が継続し、観測範囲ではクラッシュや再起動はなかった。
この記録はシリアルログに基づくもので、画面の目視、長時間安定性、停止・再初期化は未確認。

## 補足

- 起動ログの `psram_sprites=0` だけでは、PSRAM 全体が無効とは判断できない。
- 8 MB flash を検出したが、ファームウェアイメージのヘッダーは4 MBを指定していた。設定の整合は未確認。
- ネイティブ NIF の変更時は `just build-firmware` と `just flash-firmware` が必要。Elixir の変更のみなら `just flash-app` で反映できる。
