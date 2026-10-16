# gemini の状態
更新: 2026-10-16 21:44
ブランチ: main

## 今やっていること
Webエミュレータ上のバージョン記載修正（v0.1.0 -> v1.0、プレビュー表記の解除）。

## 途中の状態
- `firmware/src/felucca.c` の `FELUCCA_VERSION` を `"v0.1.0"` から `"v1.0"` に更新。
- `web/emu/build.sh` の `EMU_PREVIEW` デフォルトを 0（正式版表示）に変更。
- `.github/ISSUE_TEMPLATE/bug_report.yml` のバージョン placeholder を `v1.0` に更新。
- WebAssembly エミュレータのローカルビルドとテスト（`node web/emu/emu_test.mjs`）で bit for bit 一致を確認。

## 次の一手
コミット・pushして GitHub Actions（`.github/workflows/emulator.yml`）による Pages への自動デプロイを走らせる。

## 気づいた落とし穴・他のエージェントへの連絡
- `tests/perform_test.c` の REPEAT 1/16 の BPM アサーション（41 BPM）は、呼吸テンポ導入（`e8589db`）に伴い実効テンポが曲がった影響で 45 BPM からしか収まらなくなっているためアサーション調整が必要（今回は未変更）。
