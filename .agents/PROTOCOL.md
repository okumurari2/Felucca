# 複数AIエージェントの並行開発プロトコル

Claude・Gemini など複数のAIエージェント（と人間）が、同じ GitHub リポジトリで同時に作業するための約束事。
**会話の記憶は共有されない。共有できるのは git に push されたファイルだけ。** だから文脈・作業途中の情報はすべて `.agents/` に書いて push する。

このファイルはプロジェクトに依存しない（他のリポジトリにもそのまま入れる）。プロジェクト固有の設定（統合先ブランチ、確認コマンド、衝突しやすいファイル）は `AGENTS.md` の「並行開発の設定」に書く。

## 置き場所

| パス | 書く人 | 中身 |
|---|---|---|
| `AGENTS.md` | 全員（方針が確定したときだけ） | プロジェクトの正本。方針・制約・並行開発の設定 |
| `.agents/state/<自分>.md` | **自分だけ** | 今なにをしているか、途中の状態、次の一手、気づいた落とし穴（引き継ぎメモ） |
| `.agents/tasks/NNNN-*.md` | 担当者（`owner`） | 1タスク1ファイル。誰が・どのファイルを触るか、完了条件、作業メモ |
| `.agents/decisions.md` | 全員（追記のみ） | 決まったこと・却下したことを1行ずつ。理由つき |
| `.agents/bin/agent` | — | 上の3つを読み書きする小さな道具（Python 3、依存なし） |

自分の名前は `claude` / `gemini` / `human` のように小文字1語。`AGENT_NAME` 環境変数か `--as` で道具に伝える。

**他人の state ファイルは書き換えない。** 1人1ファイルにしているのは、同時に書いても git が衝突しないようにするため。

## 作業の流れ

### 1. 始めるとき（毎回）
```
git pull --rebase
python3 .agents/bin/agent status          # 全員の状態・進行中タスク・確保中ファイル・最近のコミット
```
- 他のエージェントの `state` を読み、いま何が動いているか把握する。
- ユーザーから頼まれた作業が、他人が `doing` で確保しているファイルに触るなら、着手せずユーザーに伝える（または区画を分ける）。

### 2. 着手する（確保 = claim）
```
python3 .agents/bin/agent new "やること" --files app_template.html#fx   # 新しいタスクを作って自分に割り当てる
python3 .agents/bin/agent claim 0003 --files gen_items.py              # 既存の open タスクを取る
git add .agents && git commit -m "[claude] claim 0003" && git push
```
- **確保は push した者勝ち。** push が拒否されたら `git pull --rebase` → もう一度 `agent status`。相手が先に同じファイルを取っていたら譲る。
- 道具は、他人が `doing` で持っているファイルと重なる確保を拒否する。
- `files:` は「これから書き換えるもの」。読むだけのファイルは書かない。大きな1ファイルは `file#区画` で分けてよい。

### 3. 作業中
- コミットは小さく。メッセージの先頭に `[自分の名前]` を付ける（例：`[gemini] 花火の段階4を追加`）。
- 区切りごとに（少なくとも push のたびに）`state/<自分>.md` を更新して一緒に push する。**途中で止まっても、他のエージェントがそのメモだけで続きを引き継げること** が基準。
- 作業メモ（試したこと、ダメだった案、未確認の点）はタスクファイルの「作業メモ」に追記する。
- push 前：`git pull --rebase` → `AGENTS.md` の確認コマンド → push。

### 4. 終える
```
python3 .agents/bin/agent done 0003        # 確保を解いて done にする
```
- `state/<自分>.md` を「待機中」に更新し、引き継ぐことがあれば「次の一手」に残す。
- 方針が決まったなら `decisions.md` に1行追記し、恒久的なルールなら `AGENTS.md` に反映する。

### 5. 止まるとき・判断待ち
- ユーザー判断が要る → `agent block 0003 "理由"`。理由はユーザーがそのまま答えられる形で書く。
- 途中で手を離す → `agent release 0003`（`open` に戻り、他の誰でも取れる）。state に「どこまでやったか」を必ず書く。

## 衝突を避ける原則
1. 同じファイル（区画）を同時に2人が持たない。確保は `agent` で機械的に確認する。
2. 生成物（ビルド結果）は手で直さない。衝突したら作り直す。
3. 他人のタスクを勝手に書き換えない。気づいたことは自分の state か、相手のタスクの「作業メモ」に `[自分の名前]` 付きで1行足す程度にとどめる。
4. 長く `doing` のまま放置しない。24時間以上 state の更新がない確保は `status` で「古い」と表示される。人間の了承があれば取り上げてよい。

## 人間（ユーザー）向け
- 誰が何をしているか：`python3 .agents/bin/agent status`、または GitHub 上で `.agents/state/` を開く。
- 片方に仕事を振る：`agent new "..." --to gemini` で owner を指定したタスクを作って push する（`open` のまま割り当て済みになる）。各エージェントには「`.agents` を見て自分宛てのタスクをやって」と頼めばよい。

## このキット自体を変更するとき（上流への push）
`.agents/PROTOCOL.md`・`.agents/bin/agent`・`.agents/templates/*`・`.agents/install.sh` は **https://github.com/okumurari2/agents-kit が正本**で、導入先リポジトリのコピーは `install.sh` で上書きされる。
- 共有脳の仕組み（プロトコル・道具・ひな形・install.sh・stub の文面）を変えたいときは、**導入先のコピーを直さず agents-kit に push する**。導入先で直しても次の更新で消える。
- 手順：agents-kit を clone（または追加）→ 変更 → commit → push → 導入先で `curl -fsSL https://raw.githubusercontent.com/okumurari2/agents-kit/main/install.sh | sh -s -- .` で取り込む。
- 導入先固有の設定（`AGENTS.md` の「並行開発の設定」、`state/`、`tasks/`、`decisions.md`）は対象外。各リポジトリで直接変更する。
- 緊急で導入先のコピーを先に直した場合も、同じ変更を必ず agents-kit に push する。
