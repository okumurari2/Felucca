# maku：開発メモ（全AIエージェント共通）

## マージ方針

- 変更が完了したら、確認を取らずに自動で main にマージして構わない。

## UI 作法の実装箇所

- ハード前提（電池・小スピーカー）から取り入れた作法は、コードのコメントの目印 `MANNER:<名前>` で探せる：`grep -rn "MANNER:" firmware`。一覧と守ること: `docs/UI_RULES.md` 10 章。動作を変えても目印は消さない。


## 並行開発の設定（`.agents/PROTOCOL.md` が参照する）
**複数のAIが並行で作業している。作業を始める前に必ず `.agents/PROTOCOL.md` の手順に従う。**
- 統合先：変更が完了したら確認なしで `main` にマージしてよい（上の「マージ方針」）
- 確認コマンド：`tests/run_tests.sh`（音が変わる変更は `BUILDING.md` の golden 更新手順に従う）
- 衝突しやすいファイル：`tests/golden.txt`、`tests/cpu_baseline.txt`、`docs/decisions.md`（着手前にタスクの `files:` で確保する）
- 生成物（衝突したら作り直すもの）：`tests/golden.txt`（`GOLDEN_UPDATE=1 sh tests/run_tests.sh`）、`./build.sh` の出力
- **共有脳（`.agents/`）の仕組みそのものを変えたいときは、このリポジトリを直さず上流 https://github.com/okumurari2/agents-kit に push する**（詳細は `.agents/PROTOCOL.md` 末尾）。
