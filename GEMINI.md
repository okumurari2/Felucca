@./AGENTS.md

# Gemini 用の補足
- 方針はすべて AGENTS.md に書く。ここには Gemini 固有のことだけ。
- 並行開発：`.agents/PROTOCOL.md` に従う。名前は `gemini`（`export AGENT_NAME=gemini`）。最初に `git pull --rebase && python3 .agents/bin/agent status`、区切りごとに `.agents/state/gemini.md` を更新して push。

## Gemini 3.8 Flash のタスク検出用チェックコマンド
作業の委譲や着手が必要か判断するための簡易コマンド集：

```bash
# 1. 直近の変更でドキュメントやテストの追従漏れがないか確認
git diff --stat HEAD~3 firmware/ docs/ tests/

# 2. ローカルビルドと公開エミュレータサイトの整合性チェック
python3 tools/check_emu.py

# 3. 参照切れファイルや孤立アセットの確認（例: assets 内の画像参照チェック）
for img in assets/*; do git grep -q "$(basename "$img")" || echo "Unreferenced: $img"; done

# 4. 未コミット変更の確認
git status -s
```

## 過去の類似コミット（参考パターン）
- **Doc/Spec Sync**: `9230c32`, `a55cb9c`, `bb7e922`, `54199e0`
- **Test Updating**: `794c9ad`, `91d0d71`, `0f4dd96`
- **Hygiene**: `b8895f8`, `e3b27cc`, `c93e571`, `aede3b7`, `6ae5fe9`
- **Web/UI Tweaks**: `bc89e85`, `80cf732`, `7d0d2fd`
- **Tooling**: `607d341`, `66a4879`
- **Release Docs**: `903363d`, `44562cd`, `17c81f4`
