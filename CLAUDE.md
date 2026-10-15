@AGENTS.md

# Claude 用の補足
- 方針はすべて AGENTS.md に書く。ここには Claude 固有のことだけ。
- 並行開発：`.agents/PROTOCOL.md` に従う。名前は `claude`（`export AGENT_NAME=claude`）。最初に `git pull --rebase && python3 .agents/bin/agent status`、区切りごとに `.agents/state/claude.md` を更新して push。
