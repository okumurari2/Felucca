# maku：開発メモ（全AIエージェント共通）

## マージ方針

- 変更が完了したら、確認を取らずに自動で main にマージして構わない。

## モデル特性と役割分担 (Claude / Gemini 3.8 Flash)

MAKU (幕間機) プロジェクトでは、各 AI モデルの得意領域に応じてタスクを明確に分担します。

| モデル | 主な担当領域 | 特性・強み |
| :--- | :--- | :--- |
| **Claude / Pro モデル** | 音響DSPアルゴリズムの新規設計、音色チューニング、生成ロジック（変異・嗜好学習アルゴリズム）、低レイヤ（JieLi HAL / USB / フラッシュセクタ管理）のアーキテクチャ設計 | 高い推論力、音楽的直観、低レベルC/C++コア設計 |
| **Gemini 3.8 Flash (High)** | 仕様書とコードの整合性同期、テストコード・アサーションの追従、孤立ファイル・デッドコードのクリーンアップ、Webエミュレータ/UIの定型調整、ツール・ビルド整合性検証、リリースノート・差分ドキュメント作成 | 超高速レスポンス、広大コンテキスト、網羅的差分チェック・テキスト構造化 |

### Gemini 3.8 Flash に振るべき作業の検出基準 (Detection Triggers)
以下のいずれかを検出した場合、ローカルの Antigravity (Gemini 3.8 Flash) に作業を振る（または委譲を提案する）：
1. **仕様書・ドキュメントの同期 (Doc/Spec Sync)**:
   - `firmware/src/` の定数、マクロ範囲、BPM、ボイス数、LED動作などを変更したが、`docs/SPEC.md`（英語正本）、`docs/AMBIENT.md`（日本語仕様）、`README.md` に差分が残っている時。
2. **テストコード・アサーションの追従 (Test Updating)**:
   - 定数やUI動作の変更に伴い、`tests/run_tests.sh`（`maku_test.c`, `regress.c`, `ui_test.c` 等）やコスト検証ファイル（`cpu_baseline.txt`, `target_budget.txt`, `golden.txt`）の更新が必要な時。
3. **孤立リソース・デッドコード・リンク破綻の掃除 (Hygiene & Cleanup)**:
   - 未参照画像アセット、削除されたCソースのツール側解析リスト残り、古いURLや不要設定ファイルの掃除。
4. **Webエミュレータ・静的UIの定型調整 (Web / UI Tweaks)**:
   - `web/emu/` のHTML/CSSレイアウト、ロゴサイズ、favicon、文言の微調整。
5. **ツール・環境整合性の検証 (Tooling & Verification)**:
   - `tools/check_emu.py` によるローカルビルドと公開サイトの整合性検証、コミットID埋め込みの保守。
6. **リリース準備・差分サマリー作成 (Release Documentation)**:
   - `docs/RELEASE_X.Y.md`（日英）、GitHub Release 本文、FM-1 Firmware Hub 提出用ドラフト（`docs/HUB_SUBMISSION.md`）の作成・推敲。

## UI 作法の実装箇所

- ハード前提（電池・小スピーカー）から取り入れた作法は、コードのコメントの目印 `MANNER:<名前>` で探せる：`grep -rn "MANNER:" firmware`。一覧と守ること: `docs/UI_RULES.md` 10 章。動作を変えても目印は消さない。

## 並行開発の設定（`.agents/PROTOCOL.md` が参照する）
**複数のAIが並行で作業している。作業を始める前に必ず `.agents/PROTOCOL.md` の手順に従う。**
- 統合先：変更が完了したら確認なしで `main` にマージしてよい（上の「マージ方針」）
- 確認コマンド：`tests/run_tests.sh`（音が変わる変更は `BUILDING.md` の golden 更新手順に従う）
- 衝突しやすいファイル：`tests/golden.txt`、`tests/cpu_baseline.txt`、`docs/decisions.md`（着手前にタスクの `files:` で確保する）
- 生成物（衝突したら作り直すもの）：`tests/golden.txt`（`GOLDEN_UPDATE=1 sh tests/run_tests.sh`）、`./build.sh` の出力
- **共有脳（`.agents/`）の仕組みそのものを変えたいときは、このリポジトリを直さず上流 https://github.com/okumurari2/agents-kit に push する**（詳細は `.agents/PROTOCOL.md` 末尾）。
