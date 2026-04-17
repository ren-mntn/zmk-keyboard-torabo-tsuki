# 書き込み手順

## 前提: bootloader バージョン

**必ず `ble_micro_pro_bootloader_1_4_0` (MSC 有効版)** を使う。

| bootloader | ZMK 起動 | Mac BLE pair | 備考 |
|---|---|---|---|
| `1_4_0` (MSC) | ✅ | ✅ | **推奨** |
| `1_4_0_no_msc` | ❌ | - | app crash で起動しない |
| `1_3_2` (MSC) | ✅ | ❌ | app は起動するが BLE が Mac に即切断される |
| `1_3_2_no_msc` | 未検証 | - | |

MSC 版なので `BLEMICROPRO` ドライブとして認識され、**UF2 ドラッグ&ドロップで書き込める**のが最大の利点。

## 標準書き込み手順 (UF2 ドラッグ&ドロップ)

### 1. bootloader に入る (BOOT→GND ショート)

1. USB ケーブルを抜く
2. BMP 基板を USB コネクタ上向きに持ち、**右側のピン列**を確認:
   ```
   [1] BAT
   [2] +5V
   [3] GND   ← ピンセット片側
   [4] BOOT  ← ピンセット片側
   [5] +4.3V
   ```
3. ピンセットで **3番 (GND) と 4番 (BOOT) をショート**
4. **ショートしたまま**で USB ケーブルを挿す
5. `/dev/cu.usbmodem0000000000011` が出現したらピンセット離して OK

参考: https://github.com/sekigon-gonnoc/BLE-Micro-Pro/blob/master/pin%20assign.jpg

### 2. BLEMICROPRO をマウント

自動マウントされないことがあるので手動で:

```bash
# 確認
ls /Volumes/

# 出てなければ disk 番号確認してマウント
diskutil list external
diskutil mount /dev/disk4  # 番号は環境による
```

### 3. settings_reset で bond クリア (bond 不一致時のみ)

左右の BLE pair 情報や Mac との bond を完全リセットしたい場合のみ実施:

```bash
cp /Users/ren/workspace/private/zmk-keyboard-torabo-tsuki/build-artifacts/settings_reset-ble_micro_pro-zmk.uf2 /Volumes/BLEMICROPRO/
```

settings_reset ファームは自動で走り、bond クリア後に bootloader に戻る or 再 BOOT→GND 要。完了後 BLEMICROPRO 再マウント。

### 4. 本番 UF2 を投下

```bash
# 左 (peripheral)
cp build-artifacts/torabo_tsuki_left_peripheral.uf2 /Volumes/BLEMICROPRO/

# 右 (central)
cp build-artifacts/torabo_tsuki_right_central.uf2 /Volumes/BLEMICROPRO/
```

**書き込み完了後、peripheral は起動に 10〜15 秒かかる**。焦らず待つ。

```bash
# 起動確認 (app port の出現)
sleep 15
ls /dev/cu.*
# cu.usbmodem2101 (1.4.0 bootloader) or cu.usbmodem101 (1.3.2) が出れば起動成功
```

## ファーム差し替えの命名規則

build.yaml の artifact-name より:

| ファイル | 役割 | col-offset |
|---|---|---|
| `torabo_tsuki_right_central.uf2` | 右=トラックボール側=central | 7 |
| `torabo_tsuki_left_peripheral.uf2` | 左=peripheral | 0 |
| `torabo_tsuki_left_central.uf2` | (逆配置用) 左を central にしたい時 | 0 |
| `torabo_tsuki_right_peripheral.uf2` | (逆配置用) 右を peripheral にしたい時 | 7 |

通常は `right_central` + `left_peripheral` の組み合わせ (トラックボールが右の場合)。

## bootloader 書き換え (通常不要)

現在の bootloader を `1_4_0` MSC 以外から変更したい場合:

1. BOOT→GND で bootloader 起動
2. Web Configurator → http://localhost:5173/BLE-Micro-Pro-WebConfigurator/#/update/bootloader
3. `ble_micro_pro_bootloader_1_4_0` (MSC 版) 選択 → Update
4. port `cu.usbmodem0000000000011` 選択
5. **書き込み中 USB 絶対抜かない** (中断 = brick、SWD 復旧必要)

## トラブルシュート

### app が起動しない (port が出ない)

- **待つ時間不足**: peripheral は 10〜15 秒かかる
- **BOOT→GND が効いていない**: ピンセット接触確認、`0000000000011` port が出るまでショートキープ
- **USB ケーブル不良**: 充電専用ケーブルだとデータ通信不可

### BLE pair できたがキー入力来ない

- **左右の bond 不一致** → 両側で settings_reset 実行 → 再 pair
- **Mac 側の古い bond 残存**:
  ```bash
  sudo rm /Library/Preferences/com.apple.Bluetooth.plist
  sudo rm -f /Library/Preferences/ByHost/com.apple.Bluetooth.*.plist
  sudo pkill bluetoothd
  ```
  (副作用: Mac の全 Bluetooth デバイス bond が消える)
- **ZMK peripheral 起動不完全**: 給電後 LED 点滅パターンで確認

### CRC Error (DFU 書き込み時)

Web Configurator の DFU 経由で書く場合のみ発生。MSC drag-drop なら無関係。

bootloader `1_4_0_no_msc` 時の nRF Secure DFU は最終ページが非 4KB 境界だと CRC 失敗する:
- `.bin` を 4KB 境界にパディング (0xFF 追加) してから `adafruit-nrfutil pkg generate` する必要あり
- ただし 1.4.0 MSC に上げれば UF2 drag-drop で全回避できる
