# 軽量化の見積もり（2026-10-13、実測ではなく推定）

> **更新（同日）**: PHASE / LOFI / VOICE / TRIO / WHEEL / NOISE は、音色カタログの行として MAKU が使うことにした（docs/VOICES_RATING.md 9）。下の 1. の表のうち、まだ使っていないのは **FM6 と SAMPLE（エンジンのコード）だけ**。削れるのは約 15 KB（フラッシュ）/ 約 12 KB（RAM）に減る。2. の候補（`sl_buf` など）は変わらない。

問い: Felucca 由来のコードを削って軽くする価値があるか。結論は「今は不要。RAM/プールが詰まったら、下の表の上から削る」。

## 現状（docs/decisions.md の実測、2026-10-10 時点）

| 項目 | 使用 / 上限 | 余裕 |
|---|---|---|
| フラッシュイメージ | 411,512 / 573,372 B（※） | 約 29% |
| RAM | 80,980 / 98,304 B | 約 18% |
| プール | 304,820 / 344,064 B | 約 11%（一番詰まっている） |
| CPU | 実機の約 30% | 約 70% |

※ 数字は 2026-10-10 の INSERT 追加時の値。その後のいいね、BREATH、DRIFT 等は含まない。

## 見積もりの方法と限界

- ターゲット用ツールチェーン（JieLi clang）がないので、ホストの clang（x86-64、`-Os`）で `felucca.c` を一括ビルドし、`nm -S` のシンボルサイズを足した。asm はビルド用コピー上で除去。
- 各エンジンのシンボル（`*_render`、`*_note_on`、エンジン表、パッチ表など）を集計した。他のコードにインライン化された分は含まない（過小評価）。
- ホストとターゲットでは命令サイズが違う。**桁と順位の目安であって、バイト数は当てにしない。** 削る前にターゲットビルドで測り直すこと。
- コードを削るだけでビルドが通ったのは PHASE / LOFI / VOICE のみ。ほかは `icons.c`、`ui_graph.c`、`fm4_convert.c`、GRAIN（サンプル型）が参照していて、スタブが要る。`ui_test.c` もエンジンに依存している（SPEC.md の Open と同じ）。

## 1. MAKU が使わないエンジン

MAKU が使うのは PHYS と GRAIN（とキックの DRUM。ANALOG は候補）。

| 対象 | フラッシュ | RAM/プール | 削りやすさ |
|---|---|---|---|
| FM6（`eng_fm6` + `fm6_core` + `fm4_convert` + `up_fm6`） | 約 13 KB | 約 10 KB | 難: `fm4_convert` と `ui_graph` が参照 |
| TRIO | 約 4.4 KB | 約 0.7 KB | 中: `icons.c` が参照 |
| VOICE（formant） | 約 3.1 KB | 約 0.5 KB | 易 |
| WHEEL | 約 2.9 KB | 約 4 KB | 中: `ui_graph.c` が参照 |
| NOISE | 約 2.6 KB | 約 0.8 KB | 中: `icons.c` が参照 |
| SAMPLE（エンジンのコードだけ） | 約 2.3 KB | 約 1.8 KB | 難: GRAIN が型を共有 |
| LOFI | 約 2.1 KB | 約 1.1 KB | 易 |
| PHASE | 約 1.6 KB | 約 0.7 KB | 易 |
| **合計** | **約 32 KB** | **約 20 KB** | |

SPEC.md の「約 28 KB のフラッシュ」とほぼ一致する。**効くのは FM6 一つで、残りは足しても約 19 KB。**
すでに OFF のもの: SLICE、DIGITAL（`FELUCCA_FM4=0`）。

## 2. エンジン以外の候補（未検証、プールに効きそうなもの）

ホストビルドで目立った大きなシンボル。MAKU で本当に不要か、他と領域を共有していないかは未確認。

| シンボル | サイズ | メモ |
|---|---|---|
| `sl_buf`（`slicer.c`） | 32 KB | トラックごとの SLICER インサート用。MAKU では SLICER を捨てている。最大の候補 |
| `proj_slot` / `proj_scratch` / `proj_wire_u`（`project.c`） | 約 23 KB | プロジェクト保存。MAKU は保存しない。ただし OTA/設定の経路と絡む |
| `up_bank`（`upreset.c`） | 約 6 KB | ユーザープリセット。同上 |
| `SMP_DATA` | 104 KB（フラッシュ） | GRAIN の素材を兼ねる。GRAIN が使わないセットだけ削れるかは未調査 |

## 3. 判断

- プールが 11% しか空いていないので、RAM 側の候補（`sl_buf`、`project.c`、FM6）が一番効く。フラッシュの余裕は 29% あり急がない。
- 削る順: ① `sl_buf` / 保存系の実際の確保元を確認 → ② FM6 → ③ 易しい三つ（PHASE / LOFI / VOICE）→ ④ ソースの削除。
- 戻す手段: `git revert`（INSERT と同じ運用）。
- Felucca 本家の修正を取り込むなら、構造を残すほうが差分を当てやすい。
- 削ったら `tests/target_budget.txt`、`tests/cpu_baseline.txt`、`tests/golden.txt` を更新する。
