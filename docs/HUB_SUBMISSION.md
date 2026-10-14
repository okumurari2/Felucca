# FM-1 Firmware Hub: submission draft

MAKU is listed on <https://fm1.designburgapps.com> (added 12 Oct 2026, v0.1). The hub looks at each project's repository and
newest release every night, so a new release is picked up without a new submission; the listing's text and its BETA mark
(shown for a 0.x version, as far as the other listings show) are the hub's. This page is the text MAKU would give it. Not part of the firmware.

## Fields

| Field | Text |
| --- | --- |
| Name | MAKU (幕間機 / maku-ma-ki) |
| Author | okumurari ([@okumurari2](https://github.com/okumurari2)); based on Felucca by Leo Kuroshita (Hügelton Instruments) |
| Repository | <https://github.com/okumurari2/maku> |
| Release | the newest one: <https://github.com/okumurari2/maku/releases/latest> (v0.1: `maku-0.1.fwsc`) |
| Licence | GPL-3.0-only |
| Picture | `docs/screenshot-pulse.png` (the FM-1's screen); logo: `assets/logo.jpg` |
| Try without hardware | <https://okumurari2.github.io/maku/> |

## One line

Ambience for DJ-event interludes that learns what you like: power it on and a new, random, in-key world is already playing; keep the ones you love and the next worlds grow from them.

## Description (EN)

MAKU (幕間機, "interlude machine") turns the FM-1 into an ambience machine for the gaps between DJ sets. There is no setup
step: from power-on, four parts (a pulse, a floor, a haze and a voice) play a random world in a random scale, and notes never
leave the scale. Four knobs set the volumes, three macros per part make it livelier as you turn right, and the buttons are
verbs: sink, cascade, swell, wobble, lift, scramble, drift. Nothing is saved except your likes: HOME held makes a new world,
REC keeps one you like, and the more likes you have, the more new worlds come from them (a little changed), so it grows to
your taste. USB / TRS MIDI in with clock follow. A browser emulator runs the same code, so you can hear it before flashing.

## Description (JA)

MAKU（幕間機）は、DJ イベントの転換や幕間のための FM-1 用ファームウェアです。電源を入れた瞬間から、ランダムなスケールの
ランダムな世界（脈・床・霞・声の4つの役）が鳴っていて、音はつねにスケールの中に収まります。つまみは4つの音量と、各役の
3つのマクロ（右に回すほど賑やか）。ボタンは「潜る・降り注ぐ・膨らむ・揺れる・持ち上げる・崩す・漂う」という動詞です。
保存されるのは「いいね」だけ：HOME 長押しで新しい世界、REC で気に入った世界を記録、いいねが増えるほど新しい世界はいいねから
（少し変えて）選ばれ、使う人の好みに育ちます。USB / TRS MIDI 入力とクロック追従に対応。ブラウザのエミュレータで、書き込む前に音を聴けます。

## Notes for the hub

- The package is a normal FM-1 `.fwsc` with the update loader (identity `FM-1_901`); returning to the official V15 works as usual,
  but it erases the likes.
- Verified on hardware by the author. Not yet measured on the device: CPU load, the ~40 ms gap of a REC write, the likes
  surviving an update (listed in the release notes).
- The logo is AI-generated (stated in the README). MAKU is not affiliated with M-VAVE.
- Please link the hub's install flow to this release rather than to Felucca's own installer; the packages are separate.
