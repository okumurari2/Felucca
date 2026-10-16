<!-- Paste into the GitHub release body (v1.0). Generated from RELEASE_1.0.md: relative links made absolute. -->
幕間機。M-VAVE FM-1 のためのファームウェア。 / An interlude machine: firmware for the M-VAVE FM-1.

![MAKU on the FM-1: the pulse part has focus](https://raw.githubusercontent.com/okumurari2/maku/main/docs/screenshot-pulse.png)

## 日本語

**毎回ちがう世界が鳴り、好きを押すと、次の世界があなたに寄っていく。** MAKU は、電源を入れた瞬間から鳴っているアンビエンスの機械です。

### MAKU のここが違う

FM-1 にはアンビエント系のファームがほかにもあります（たとえば [AMB-1](https://github.com/charlesvestal/fm1-amb)：曲を選び、手入れして育てる）。MAKU は設計の軸が違います。

- **起動のたびに新しい世界。** 曲を選ぶのではなく、音色・根音・スケール・テンポ・つまみの初期値がその場でランダムに決まります。音はつねにスケールの中。保存される状態はありません。
- **好みで育つ。** REC で気に入った世界を「いいね」に記録（最大16件）。増えるほど、新しい世界は、いいねから（少し変えて）選ばれやすくなります（最大90%）。保存されるのはいいねだけ。
- **ボタンは動詞。** 潜る・降り注ぐ・膨らむ・揺れる・持ち上げる・崩す。押している間だけ効き、離すとゆっくり戻ります。音を「設定する」のではなく、場に「手を入れる」。
- **幕間のための道具。** DJ イベントの転換でつないで、すぐ鳴らす。電池駆動と小さなスピーカーを前提に、起動は無音からふくらむ・高域のピークを抑える・REC で音を止めない・電池表示は必要なときだけ、LED は電池なら暗め、という作法（`docs/UI_RULES.md` 10 章）で作っています。MIDI クロックに追従します。

### v1.0 の変更点

v0.1 以降の変更をまとめて 1.0 とします。

- **呼吸**：テンポを 70 BPM より下げると、遅くなるほど 1 小節が 1 回の呼吸（最長約 18 秒）になり、キックは小節頭のやわらかい合図に、DENSITY は効かなくなります。外部クロック追従中は働きません。
- **DRIFT が音色も漂わせる**：DRIFT がオンの間、40〜60 秒ごとに床・霞・アルペジオのどれかが、休符のところで同じ種類の別の音色に替わります。
- **新しい声はベールの下から**：新しい世界・いいね・DRIFT の音色替えのあと、音の明るさが低いところから約 6 秒で自然に開きます。
- **高域のコンプレッサー**：マスターにかかり、明るい音のピークを抑えます（小さなスピーカーで耳に刺さらないように）。
- **背景層**：4つの役とは別に、世界ごとに背景の質感を1つ（または、なし）敷きます。潮騒・雨・風・たきび・レコードのノイズ・人のざわめき。録音は使わず、すべて計算で作ります。音量は控えめで、DRIFT がレベルを揺らし、SINK で後ろへ下がり、SWELL で大きくなります（潮騒は大波）。新しい世界のとき、音色名のあとに「~ SURF」の形で名前が出ます。既存のいいねはそのまま使えます（背景層なしとして読みます）。
- **音色が増えました**：床 +7、霞 +6、声 +7（計 +20 行）。既存のいいねはそのまま使えます。
- **ROOM が広いホールに**：残響のコムとオールパスを長くし、プリディレイを 34 ms に。
- **キックの初期音量**：104 → 90。
- **音色名の表示**：フォーカスした役の音色名が PRESETS の下に出て、新しい世界は 3 つの音色名を順に画面左上に出します。
- **ビルドの修正**：呼吸の位相計算が 64 ビット除算でリンクできなかったのを直しました。

### インストール

> ファームウェアの書き換えは**自己責任**です。戻し方を先に確認してください。

1. 下の `maku-1.0.fwsc` をダウンロードします。
2. FM-1 を USB でつなぎ、コンピューターから書き込みます：
   `python3 tools/fm1_install.py maku-1.0.fwsc`（`pip3 install mido python-rtmidi` が必要。詳しくは [BUILDING.md](https://github.com/okumurari2/maku/blob/main/BUILDING.md)）。
   FM-1 Firmware Hub などのブラウザ版インストーラーからも書き込めます（Chrome / Edge）。
3. **戻すには**：インストーラーの「Return to official V15」、または M-VAVE 公式のアップデーター。どちらもいいねは消えます。

途中で止まって画面が真っ黒のままのときは、[README](https://github.com/okumurari2/maku/blob/main/README.md) の "If the FM-1 does not start" を見てください。

### ブラウザで試せます

[MAKU emulator](https://okumurari2.github.io/maku/)（実機と同じコードを WebAssembly で動かしています）。

### 既知の制限・実機で未確認のこと

実機での動作は作者が確認済みですが、次はコードやホストでの測定から読んだ値で、実機では確かめていません。

- **REC の書き込み**：フラッシュ書き込みで音が約 40 ms 途切れます。
- **いいねの保存**：MAKU / Felucca 間の更新では残るはずですが（コードからの判断）、実機では未検証です。
- **処理負荷**：v0.1 での見積もりは最悪の世界で約 51%（ホスト測定からの換算）。v1.0 の追加分（高域コンプ・ホールリバーブ・背景層など）は実機で測っていません（背景層は、ホストの見積もりで最悪の世界に +4〜7 ポイント、合計約 58%）。
- v0.1 以降に加えた音色・リバーブ・呼吸・背景層は（背景層は各種類の大きさと音質が、小さなスピーカーでどう聞こえるか未確認）、、ホストでの試聴・測定が中心です。気になる音があれば [Issues](https://github.com/okumurari2/maku/issues) へ。

### ライセンス

GPL-3.0-only。[Felucca](https://github.com/hugelton/Felucca)（Leo Kuroshita / Hügelton Instruments）から、シーケンサー編集・プリセットと保存・音作りページ・Web エディタを捨てて作った派生版です。パッケージには JieLi AC79 SDK（Apache-2.0）のファイルが3つ入っているため、ライセンス文は [`LICENSES/`](https://github.com/okumurari2/maku/blob/main/LICENSES) にあります（パッケージの添付物にも `LICENSE` と `LICENSING.md` を付けています）。ロゴは AI 生成です。M-VAVE・FM-1 は各権利者の商標で、MAKU は関係も承認もありません。

## English

**A different world every time, and the more you like, the more the next ones come from your taste.** MAKU is an ambience machine that is already playing when you switch it on.

### What sets MAKU apart

There are other ambient firmwares for the FM-1 (for example [AMB-1](https://github.com/charlesvestal/fm1-amb): you pick a piece and tend it). MAKU is built on a different axis.

- **A new world on every boot.** You do not choose a piece: the sounds, root, scale, tempo and every macro are drawn at random, on the spot. Notes always stay in the scale. No state is saved.
- **It grows to your taste.** REC keeps the world playing as a like (up to 16). The more likes there are, the more often a new world comes from them, a little changed (up to 90 %). The likes are all it stores.
- **Buttons are verbs.** Sink, cascade, swell, wobble, lift, scramble. Each works while held and fades back slowly: you lay a hand on the room rather than set a sound.
- **Made for the gaps between DJ sets.** Plug it in and get sound, fast. Built for a small speaker and a battery (`docs/UI_RULES.md`, section 10): power-on swells up from silence, high-band peaks are tamed, REC does not stop the sound, the battery shows only when it matters, LEDs are dim on battery. It follows MIDI clock.

### What changed in v1.0

Everything since v0.1 is collected into 1.0.

- **Breath**: below 70 BPM, the slower it gets the more a bar becomes one breath (up to about 18 s); the kick becomes a soft cue at the head of the bar and DENSITY fades as it loses its effect. Not under external clock.
- **DRIFT drifts the voices too**: while it is on, every 40-60 s one of the floor / haze / arp takes another sound of its kind, at its rest.
- **A new voice comes in under a veil**: after a new world, a like or DRIFT's change of voice, the brightness starts low and opens by itself in about 6 s.
- **A high-band compressor** on the master takes the peaks off bright sounds, so a small speaker does not shout.
- **A background bed**: besides the four parts, each world lays one texture under them (or none): surf, shower, breeze, campfire, vinyl or crowd. Nothing is recorded: all of it is computed. It is quiet; DRIFT moves its level, SINK pushes it back, SWELL raises it (a surf sends a big wave). A new world names it after the sounds, as "~ SURF". Existing likes keep working (they read as no bed).
- **More sounds**: floor +7, haze +6, voice +7 (20 rows). Existing likes keep working.
- **ROOM is a wider hall**: longer combs and allpasses, 34 ms pre-delay.
- **The kick's initial level**: 104 to 90.
- **The kit is shown**: the focused part's sound name under PRESETS, and a new world names its three sounds in the top left.
- **Build fix**: the breath's phase used a 64-bit divide the target cannot link; fixed.

### Install

> Installing replaces the FM-1's firmware: **at your own risk**. Check the way back first.

1. Download `maku-1.0.fwsc` below.
2. Connect the FM-1 over USB and write the package from a computer:
   `python3 tools/fm1_install.py maku-1.0.fwsc` (needs `pip3 install mido python-rtmidi`; see [BUILDING.md](https://github.com/okumurari2/maku/blob/main/BUILDING.md)).
   A browser installer such as the FM-1 Firmware Hub (Chrome / Edge) can write it too.
3. **To go back**: the installer's "Return to official V15", or M-VAVE's own updater. Both erase your likes.

If an update is interrupted and the screen stays black, see "If the FM-1 does not start" in the [README](https://github.com/okumurari2/maku/blob/main/README.md).

### Try it in the browser

The [MAKU emulator](https://okumurari2.github.io/maku/) runs the same code as the instrument, compiled to WebAssembly.

### Known limits / not yet verified on hardware

Verified on hardware by the author, except for the following, which are read from the code or measured on the host:

- **REC write**: the flash write leaves a gap of about 40 ms in the sound.
- **Likes surviving an update**: they should survive MAKU / Felucca updates (read from the code), not yet tested on hardware.
- **CPU load**: about 51 % of the device in the worst world in v0.1 (converted from host measurements). What v1.0 adds (the high-band compressor, the hall reverb, the background bed and so on) is not measured on the device (the bed adds 4-7 points in the worst world by a host estimate, about 58 % in all).
- The sounds, reverb, breath and background bed (its levels and tone on a small speaker above all) added since v0.1 were mostly judged on the host. If a sound bothers you, please open an [Issue](https://github.com/okumurari2/maku/issues).

### Licence

GPL-3.0-only. A derivative of [Felucca](https://github.com/hugelton/Felucca) (Leo Kuroshita / Hügelton Instruments) with the
sequencer editing, presets and saving, sound-design pages and web editor removed. Every package contains three Apache-2.0
files of the JieLi AC79 SDK, so the licence texts are kept in the repository ([`LICENSES/`](https://github.com/okumurari2/maku/blob/main/LICENSES)). The logo is AI-generated. M-VAVE and FM-1 are
trademarks of their respective owners; MAKU is not affiliated with or endorsed by them.

### Files

| File | What |
| --- | --- |
| `maku-1.0.fwsc` | the installable package (identity `FM-1_910`) |
| `maku-1.0-app.bin` | the app alone |
| `SHA256SUMS` | checksums |
| `ATTRIBUTION.txt`, `LICENSE`, `LICENSING.md` | sample attribution and licence texts (the other licence texts: [`LICENSES/`](https://github.com/okumurari2/maku/blob/main/LICENSES) in the repository) |
