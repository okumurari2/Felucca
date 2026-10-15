#!/bin/sh
# 並行開発キット（.agents/）を別のリポジトリに入れる。何度実行してもよい（既存の state・タスク・方針は上書きしない）。
#   sh install.sh /path/to/other-repo
# リポジトリを手元に置かず GitHub から直接入れる場合（対象リポジトリの直下で）:
#   curl -fsSL https://raw.githubusercontent.com/okumurari2/agents-kit/main/install.sh | sh -s -- .
set -eu
DST=${1:?使い方: sh install.sh 対象リポジトリのパス}
SRC=$(cd "$(dirname "$0")" 2>/dev/null && pwd || true)
RAW=${AGENTS_KIT_URL:-https://raw.githubusercontent.com/okumurari2/agents-kit/main}
cd "$DST"
mkdir -p .agents/bin .agents/templates .agents/state .agents/tasks

fetch() {  # 共通部分（PROTOCOL・道具・ひな形）は常に最新で上書きする
  if [ -n "$SRC" ] && [ -f "$SRC/$1" ] && [ "$SRC" != "$(pwd)/.agents" ]; then cp "$SRC/$1" ".agents/$1"
  elif [ -n "$SRC" ] && [ "$SRC" = "$(pwd)/.agents" ]; then :
  else curl -fsSL "$RAW/$1" -o ".agents/$1"; fi
}
for f in PROTOCOL.md bin/agent templates/task.md templates/state.md install.sh; do fetch "$f"; done
chmod +x .agents/bin/agent
touch .agents/tasks/.gitkeep

now=$(date '+%Y-%m-%d %H:%M')
for n in claude gemini; do
  [ -f ".agents/state/$n.md" ] || sed -e "s/NAME/$n/" -e "s/YYYY-MM-DD HH:MM/$now/" .agents/templates/state.md > ".agents/state/$n.md"
done
[ -f .agents/decisions.md ] || printf '# 決定ログ（追記のみ）\n\n形式：`- YYYY-MM-DD [誰] 決めたこと — 理由`\n' > .agents/decisions.md

# 方針の正本を AGENTS.md に。既存の CLAUDE.md があれば中身をそこへ移す。
if [ ! -f AGENTS.md ]; then
  if [ -f CLAUDE.md ] && ! grep -q '^@AGENTS.md' CLAUDE.md; then git mv CLAUDE.md AGENTS.md 2>/dev/null || mv CLAUDE.md AGENTS.md
  else printf '# %s：開発メモ（全AIエージェント共通）\n\n' "$(basename "$(pwd)")" > AGENTS.md; fi
fi
grep -q '.agents/PROTOCOL.md' AGENTS.md || cat >> AGENTS.md <<'MD'

## 並行開発の設定（`.agents/PROTOCOL.md` が参照する）
**複数のAIが並行で作業している。作業を始める前に必ず `.agents/PROTOCOL.md` の手順に従う。**
- 統合先：`main` に直接 push ／ または各自 `<名前>/*` ブランチから PR（どちらかに決めて書く）
- 確認コマンド：（例 `npm test`、`python3 -m pytest`）
- 衝突しやすいファイル：（着手前にタスクの `files:` で確保するもの）
- 生成物（衝突したら作り直すもの）：
- 共有脳（`.agents/`）の仕組み自体の変更は、このリポジトリでなく上流 https://github.com/okumurari2/agents-kit に push する（`.agents/PROTOCOL.md` 末尾）。
MD

stub() {  # $1=ファイル $2=import行 $3=名前
  [ -f "$1" ] && grep -q "AGENTS.md" "$1" && return 0
  { echo "$2"; echo; echo "# $3 用の補足"
    echo "- 方針はすべて AGENTS.md に書く。ここには $3 固有のことだけ。"
    echo "- 並行開発：詳細は \`.agents/PROTOCOL.md\`（軽量モードの節）。名前は \`$4\`（\`export AGENT_NAME=$4\`）。"
    echo "  1. 始めに \`git pull --rebase && python3 .agents/bin/agent status\`"
    echo "  2. 終わりに \`.agents/state/$4.md\` を更新して push（push しない変更は他人に見えない）"
    echo "  3. 自分宛てのタスク（owner が自分）の \`files:\` 以外は触らない。claim/done は任意。"
    if [ -f "$1" ]; then echo; cat "$1"; fi; } > "$1.tmp"
  mv "$1.tmp" "$1"
}
stub CLAUDE.md '@AGENTS.md' Claude claude
stub GEMINI.md '@./AGENTS.md' Gemini gemini

echo "導入しました。AGENTS.md の「並行開発の設定」を埋めてから commit / push してください。"
