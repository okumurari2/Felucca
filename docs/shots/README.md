# 画面デザイン案のスクショ（プレビュー）

**このディレクトリの絵は常にこの1か所・同じファイル名で更新する**（案ごと・ブランチごとに増やさない）。見るのは `mk_styles.png` 1枚でよい。

![4案の比較](mk_styles.png)

列は左から 現状 / 1 FUWA / 2 MARU / 3 RPG / 4 CAVE。行は上から 世界A（seed 7）/ 世界B（seed 99）/ SINK を押している間。

| 案 | 狙い | 書体 |
| --- | --- | --- |
| 0 NOW | 現状（出荷中の画面） | Inter Tight |
| 1 FUWA | 藍の夜にパステル。丸いカード、丸薬のバー、またたく星 | Inter Tight |
| 2 MARU | 桃色の紙に、キャンディ色。丸いカード、紙吹雪 | Fredoka |
| 3 RPG | 深い青の窓に白い枠。区切りゲージ、▶ のカーソル | DotGothic16 |
| 4 CAVE | 松明の色の窓。**地面が波形**で、その上に焚き火。4つのパートは小さな生き物 | DotGothic16 |

個別の絵（seed 7）: [現状](style0_now.png) · [1](style1.png) · [2](style2.png) · [3](style3.png) · [4](style4.png)

## 触って見る

エミュレーター（`web/emu/build.sh` で作る）の URL に `#style=N`（0〜4）を付ける。`#seed=7&style=4` のように、世界も指定できる。

## 撮り直す

```
python3 -c "import sys; sys.path.insert(0, 'tools'); import build; build.generate()"   # 初回だけ
sh web/emu/build.sh                  # emcc が要る
node tools/mk_shots.mjs              # docs/shots/mk_styles.png を作り直す（Playwright と Chromium が要る）
```

## 実装

- `firmware/src/ui_maku_style.c`: 4案の描画。**エミュレーターだけ**（`-DMK_STYLES`）。出荷ファームには入らない。
- 色・書体・配置は案ごとの表（`MKS`）にあり、同じ描画関数が使う。データとレイアウトの決まりは出荷中の画面と同じ（つまみ1つに1区画）。
- 追加の書体は `tools/gen_mk_fonts.py`（Fredoka と DotGothic16、どちらも SIL OFL 1.1）。出荷ファームの書体は変えていない。
