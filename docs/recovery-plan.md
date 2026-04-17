# 書き込みトラブル - 現状とリカバリ計画

## 現時点の状況（2026-04-17時点）

### 左側 BMP（ペリフェラル）
- **状態**: 動作中
- **書き込まれているファーム**: `ble_micro_pro_vial_1_4_0`（標準vialファーム）
- **bootloader**: `ble_micro_pro_bootloader_1_4_0`（更新済み）
- **mymap_1_0_0 は未書き込み**

### 右側 BMP（セントラル・トラックボール側）
- **状態**: bootloaderは生きている（USB認識されbootloaderポート `cu.usbmodem0000000000011` として現れる）
- **書き込まれているファーム**: なし（途中で失敗、アプリは未完成）
- **bootloader**: `ble_micro_pro_bootloader_1_4_0`（更新済み）
- **BOOTピン→GNDショートで強制bootloader起動は可能**（これは確認済み）

## やりたかったこと（本来のシンプルな目標）

commit `fa66e6b` の時点ではスクロール以外は動作していた。
その状態に戻して、追加でスクロールだけ有効化したい。

### commit履歴
- `fa66e6b` "Fix mouse button indices, disable automouse-layer, lower CPI" - **両側動作していた安定点**
- `d29932c` "feat: enable scroll-layers on layers 2, 3, 5" - スクロール追加
- `8e6bc29` "tune: flatten accel curve, lower CPI further" - トラックボール感度調整

## 発生した問題

### 問題1: mymap firmware の書き込みで CRC エラーが出る
- Web Configurator の DFU プロトコル経由で `sekigon_torabo_tsuki_mymap_right_1_0_0`（約257KB）を書き込むとCRC Error発生
- 具体的なCRC値: `Expect: 0x-2762320a, Received: 0x-782b235c` （決定論的に同じ値）
- 一方 `ble_micro_pro_vial_1_4_0`（約70KB）や `sekigon_torabo_tsuki_vial_1_4_0`（約80KB）は書き込み成功

### 問題2: bootloaderの部分状態保持
- DFU書き込みが途中失敗すると、bootloaderが部分的な書き込み状態を保持
- 次の書き込み時、`tryToResume` ロジックがこの状態を使おうとするがCRC不一致で失敗
- 部分状態をクリアする方法が見つからなかった

### 問題3: 書き込み試行を繰り返すうちに状態悪化
- CRC Error → 再試行を何度もするうちに右BMPが一時USB認識しなくなる
- BOOTピン→GNDショートで物理的に強制bootloader起動することで復旧可能

## 環境・ファイル

### リポジトリ
- **ZMK firmware**: `/Users/ren/workspace/private/zmk-keyboard-torabo-tsuki`
  - ブランチ: `main`
  - 最新コミット: `8e6bc29`
  - リモート: `github.com-private:ren-mntn/zmk-keyboard-torabo-tsuki.git`

### ローカルで動いているWeb Configurator
- **パス**: `/tmp/bmp-webconfig`
- **起動**: Vite dev server on `http://localhost:5173/BLE-Micro-Pro-WebConfigurator/`
- **プロセス**: バックグラウンドで vite 走ってる（`ps aux | grep vite` で確認）
- **重要**: 初期状態に `git checkout` で戻し済み（src/dfu.ts, index.jsの改造は破棄）
- **ただし `index.js` の applications list には mymap エントリを再追加済み**

### 配置済みファーム
`/tmp/bmp-webconfig/public/application/`:
- `sekigon_torabo_tsuki_mymap_1_0_0.bin/.dat` - 左ペリフェラル用（build 24561611179の成果物）
- `sekigon_torabo_tsuki_mymap_right_1_0_0.bin/.dat` - 右セントラル用（build 24561611179の成果物）

### CI build artifacts
- **最新ビルド ID**: 24561611179（commit `8e6bc29`）
- **以前の安定ビルド ID**: 24557996015（commit `fa66e6b`）
- **ダウンロード済み**:
  - `/tmp/zmk-artifacts/firmware/*.uf2` - 最新（24561611179）
  - `/tmp/zmk-old/firmware/*.uf2` - 以前の安定版（24557996015）

### 変換ツール
- UF2→BIN変換: `python3 /Users/ren/workspace/private/torabo-tsuki/vial-qmk/util/uf2conv.py <input.uf2> -o <output.bin> --convert`
- DAT生成: `adafruit-nrfutil pkg generate --hw-version 52 --sd-req 0x00 --application-version 1 --application <bin> <out.zip>`
  - 注: Python 3 互換パッチを当てている（`/Users/ren/.local/lib/python3.9/site-packages/nordicsemi/dfu/`）

## BMPの物理操作

### BOOTピン→GNDショートでbootloader起動
BMP基板をUSBコネクタを上にして見たとき、**右側のピン列**:
1. BAT
2. +5V
3. **GND** ← ここと
4. **BOOT (P0.7)** ← ここを金属でショート
5. +4.3V
6. PIN20以下

**手順**:
1. USB ケーブルを抜く
2. 金属ピンセット（またはジャンパワイヤ）で GND と BOOT をショート
3. ショートしたまま USB 接続
4. `ls /dev/cu.*` で `cu.usbmodem0000000000011` が現れればOK
5. ピンセット外してよい

**参考画像**: https://github.com/sekigon-gonnoc/BLE-Micro-Pro/blob/master/pin%20assign.jpg

### 1200bps touch でもbootloader起動可能（ZMKアプリ動作時のみ）
```bash
python3 -c "import serial; import time; s = serial.Serial('/dev/cu.usbmodem101', 1200); time.sleep(0.1); s.close()"
```
（ポート名は実際のものに合わせる）

## 推奨リカバリ計画

### 案A: UF2ドラッグ&ドロップ方式を試す（最も確実そう）
1. BOOTピン→GNDショートでbootloader起動
2. macOSが `BLEMICROPRO` というUSBストレージとして認識するか確認（→ 今日は確認できなかった、bootloader 1.4.0 は MSC無効版の可能性あり）
3. 認識したらuf2をドラッグ&ドロップ
4. 認識しない場合、`_no_msc`じゃない版のbootloaderに更新する必要あり

### 案B: 別環境で書き込み
- 別PC（Windowsでも可）のChromeでWeb Configurator試す
- macOSのUSB CDCドライバ特有の問題の可能性切り分け

### 案C: ビルドを作り直してから試す
- 一度何か追加コミットして CI で新ビルド
- バイト配置が微妙に変わるので CRC エラーの再現性が消える可能性
- `git commit --allow-empty -m "empty commit to retry build"` でも可

### 案D: SWDデバッガ（J-Link等）で直接書き込み
- 最確実だが機材必要
- BMP裏面にSWD用パッドあり

### 案E: 左側 mymap_1_0_0 だけ書き込んで右はvialで妥協
- 右は常時vialで運用、左だけカスタムキーマップ
- 現実的には意味が薄い（機能が中途半端になる）

## CRC Error 再現手順（デバッグ時参照用）

1. Web Configurator (`http://localhost:5173/BLE-Micro-Pro-WebConfigurator/#/update/application`) を開く
2. `sekigon_torabo_tsuki_mymap_right_1_0_0` 選択
3. Update押下
4. ポート `cu.usbmodem0000000000011` 選択
5. 進捗バーがちょっと進んで停止
6. ブラウザconsole(F12)で以下エラー:
   ```
   CRC Error: Expect: 0x-2762320a, Received: 0x-782b235c
   at DfuBootloader.streamData
   at async tryToResume / sendFirmware
   ```

## 本日試して**失敗**した方法

- Web Configurator の dfu.ts を修正して sleep 追加 → 改善なし
- tryToResume 無効化して強制フレッシュ書き込み → DFU timeout
- adafruit-nrfutil の dat 生成で sd-req 値変更（0x00, 0xA9, 0xFFFE, 複数指定） → 改善なし
- `--debug-mode` フラグ → 改善なし
- 古いビルド（24557996015）の bin に差し替え → 改善なし
- ブラウザハードリロード → 改善なし
- USB抜き差し・電源再投入 → 一時的にUSB認識失われるケースもあった
- CLI `adafruit-nrfutil dfu usb-serial` 直接 → Python3互換問題で深い修正必要、CRC失敗は解消せず
- 両side の bootloader を 1.4.0 に更新 → 書き込み成功フローは作れたが mymap_right の CRC Error は解消せず
- 小さいファーム（vial）で上書きして状態クリア → vialは書けるが次にmymap書こうとすると再びCRC Error

## 今日のチェック済みファクト

- **vial_1_4_0系は書き込める**（左では `ble_micro_pro_vial_1_4_0` 成功、右では `sekigon_torabo_tsuki_vial_1_4_0` 成功）
- **mymap系（185KB超）は書き込めない**（サイズまたはバイトパターン起因の疑い）
- **左右両方でbootloader 1.4.0 に更新成功**

## 参考リンク

- [BLE Micro Pro GitHub](https://github.com/sekigon-gonnoc/BLE-Micro-Pro)
- [ピン配置画像](https://github.com/sekigon-gonnoc/BLE-Micro-Pro/blob/master/pin%20assign.jpg)
- [BLE Micro Pro公式ドキュメント](https://sekigon-gonnoc.github.io/BLE-Micro-Pro/)
- [torabo-tsuki ZMK repo (sekigon)](https://github.com/sekigon-gonnoc/zmk-keyboard-torabo-tsuki)
- [このリポジトリ (fork)](https://github.com/ren-mntn/zmk-keyboard-torabo-tsuki)
