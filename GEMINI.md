@./AGENTS.md

# Gemini 用の補足
- 方針はすべて AGENTS.md に書く。ここには Gemini 固有のことだけ。
- 並行開発：`.agents/PROTOCOL.md` に従う。名前は `gemini`（`export AGENT_NAME=gemini`）。最初に `git pull --rebase && python3 .agents/bin/agent status`、区切りごとに `.agents/state/gemini.md` を更新して push。
