@AGENTS.md

# Claude 用の補足
- 方針はすべて AGENTS.md に書く。ここには Claude 固有のことだけ。
- 並行開発：`.agents/PROTOCOL.md` に従う。名前は `claude`（`export AGENT_NAME=claude`）。最初に `git pull --rebase && python3 .agents/bin/agent status`、区切りごとに `.agents/state/claude.md` を更新して push。

## Gemini 3.8 Flash への委譲ハンドオフ
Claude 側で音響DSP設計やコアアーキテクチャの変更を完了した後、あるいは以下のタスクが検出された場合は、`AGENTS.md` の「Gemini 3.8 Flash に振るべき作業の検出基準」に基づき、ローカルの Antigravity (Gemini 3.8 Flash) に委譲することを推奨します：
- 仕様書同期 (`docs/SPEC.md`, `docs/AMBIENT.md`, `README.md`)
- テストコード・アサーション追従 (`tests/`, `target_budget.txt`, `golden.txt`)
- 孤立リソース・デッドコード・リンク掃除
- Webエミュレータ (`web/emu/`) の定型UI調整
- `tools/check_emu.py` などの検証スクリプト実行、リリースノート作成

タスクを切り出して委譲する場合：
```bash
python3 .agents/bin/agent new "タスク名" --to gemini --files <対象ファイル>
```
