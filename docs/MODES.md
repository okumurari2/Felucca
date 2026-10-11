# 複数モードの同居（MAKU + t8lab + 今後の派生）— 作業メモ

自分用の「後で思い出す」メモ。決めたことと、まだやっていないことを残す。（2026-10-11 時点。更新したら日付を足す）

## 何をしたいか

FM-1 の 1 つのファームに、複数のアプリ（モード）を入れて、起動時に選ぶ。MAKU と t8lab が最初の 2 つで、メモリの許す限り派生を足していく。
**公開は各リポジトリで別々**（maku / t8lab）。同居は必須ではなく、各リポジトリは今までどおり単体の `.fwsc` を出す。

## 決めたこと

- 方式：**1 つのバイナリに複数モードを静的リンク**（フラッシュを 2 スロットに分ける案は、容量・ローダ・`.fwsc` 形式の作り直しが重く、採らない）。
- モード切替は**再起動のみ**（ライブ切替なし）。保存したモード ID か、電源投入時のボタンで選ぶ。
  **OCT− + OCT+ は使えない**（パネル設定と、5 秒長押しの UBOOT に使用済み。`firmware/src/main.c`）。
- 共有するのは `firmware/mode/mode.h`（インタフェースだけ。Felucca 由来のコードを混ぜない）。HAL / UI / エンジンは当面コピーのまま、Felucca とこのリポジトリを基準に同期。
- `felucca-bsp`（hugelton）は今後の主な維持対象になる予定だが、**単体ビルド不可・名前も変わる段階なので見送り**。ビルドできるようになったら、まず `chip/` と `loader/` の差し替えを試す。ライセンスは暫定（MPL-2.0 予定、`usb.c` は GPL-3.0-only）。

## mode.h（draft 0）の状態

- 中身：`fm1_mode_t`（`enter / leave / control / render / frame`、`pool_bytes / ram_bytes`）、`fm1_store_t`（4 KiB 単位の保存セクタ宣言）、モード ID（MAKU=1, T8=2, 0xF0–0xFF は実験用）、`FM1_MODE_REQUIRE_API(v)`。
- 正本：**maku リポジトリのブランチ `mode-h-draft`**。t8lab の `firmware/mode/mode.h` はそのコピー。**形を変えるときは maku 側を先に直して、t8lab へ写す**。
- ライセンス表記は `MPL-2.0`（暫定）のまま。main に入れる前に確定させる（確定するまでは GPL-3.0-only に揃える手もある）。

## 残っている作業（順番どおり）

1. **`control` をブロックサイズ引数付きにするか決める**（`control(uint32_t n)`）。t8lab の `t8_mode.c` は今 128 を直書き（`HALF_FRAMES`）。直したら t8lab のコピーにも反映。
2. **dsp へのイベント受け渡しをヘッダに入れるか決める**。今は `t8_mode.c` の配列 `t8_evq`。3 つ目のモードが出てきた時点で見直す。
3. **MAKU を `mode_maku` で包む**：`main.c` の `maku_setup()` + `maku_world()` + `transport_req = 1`、`seq.c` の `maku_block()`、UI の `ui_input / ui_leds / ui_draw` を `fm1_mode_t` に載せ替える。MAKU の既存テスト（`tests/run_tests.sh`）が通ることを確認。**これが済むまで mode.h を main に入れない**（呼び出し元のない死んだヘッダになるため）。
4. **t8lab のコア以降を C に移す**：dsp（キック共振器 `dsp/kick.js`、バス、voices）→ UI（`view()`、画面）。キット補間（`taste.js` の `blend` / `kitAt`）は dsp と同時。キック共振器は MAKU の `phys_dsp.c` の `ResonatorSvf` と共有できるか検討（PLAN M6）。
5. **サイズを実測する**：ツールチェーン（`AC79_SDK`、pi32v2 の clang）のある環境で、両方の単体ビルドの `.map` を取る。枠は `APP_SLOT = 0x8DFBC`（約 573 KB）。MAKU に残る未使用エンジン約 28 KB（`docs/SPEC.md`）は、同居前に削る候補。
6. **同居ビルドの置き場を決める**：新しい `fm1-multi`（submodule で maku と t8lab を取り込む）か、当面 t8lab の `multi/`。持つのは、リンカスクリプト、モード選択、`.fwsc` 化、フラッシュ予算表（モードごとの上限。超えたらビルド失敗）、保存セクタの重複検査。
7. **POOL / RAM の重ね方**：`POOL`（0x54000）は全モードで使い回す。各モードが `pool_bytes` を宣言し、リンカスクリプトの値と照合する。
8. **保存セクタの割り当て**：MAKU は `0xE5000 / 0xE6000`（REC のいいね）。t8lab のいいね（16 件、現状は localStorage）の置き場を決める。`[0x4000, 0x97000)`（アプリ）、`[0xE0000, 0xE5000)`（OTA ステージング）、`0xFC000` 以降は使わない。公式への復帰（Return to official V15）で消える領域かも記録する。
9. **共有の仕組み**：形が安定したら `fm1-mode`（ヘッダだけのリポジトリ）を作り、各リポジトリへ `git subtree` で取り込む。`MODE_API_VERSION` で版のずれをビルド時に検出。CI で取り込んだヘッダと正本の一致を確認。それまではコピー運用。
10. **ライセンスの最終確認**：MAKU / t8lab は Felucca 由来の GPL-3.0-only。同居ビルドで混ぜても矛盾しないか、`mode.h` を緩いライセンスにしてよいか（Felucca の作者への確認を含む）。

## すぐ使えるもの

- t8lab：`sh tests/t8_parity.sh` が JS コアと C コアの出力を突き合わせる（ブランチ `t8-core-c`）。`tests/t8_mode_test.c` が `mode.h` 経由のクロックを確認。
- 設計の背景：このメモの元になった相談の要約は、各リポジトリの `docs/decisions.md` の 2026-10-11 の行を参照。

## このリポジトリ（maku）でやること

- ブランチ `mode-h-draft` に `firmware/mode/mode.h` がある（**mode.h の正本**）。まだ main に入れていない（上の手順 3 のあと）。
- 上の 1・2 を決めたら、ここで mode.h を直し、t8lab（ブランチ `t8-core-c`、のちに main）へ写す。
- 手順 3（`mode_maku`）はこのリポジトリの仕事。`firmware/src/main.c` のブート順（`maku_setup` → `maku_world` → スプラッシュ → メインループ）を壊さないこと。
