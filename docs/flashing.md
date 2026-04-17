# 書き込み手順メモ

## DFU bootloader起動方法

ZMKファーム動作中の本機では、Web Configuratorの `dfu` コマンド（「Bootloader is activated」の表示）は**効かない**（QMK時代の名残）。物理リセットボタンもBMP基板上に見当たらないため、以下の方法で起動する。

### 1200bps touch（推奨）

`CONFIG_ZMK_CDC_ACM_BOOTLOADER_TRIGGER=y` により、シリアルポートを1200bpsで開いてから閉じるとbootloaderに入る。

```bash
# 現在のポートを確認
ls /dev/cu.*

# 1200bpsで一瞬開いて閉じる
python3 -c "import serial; import time; s = serial.Serial('/dev/cu.usbmodem101', 1200); time.sleep(0.1); s.close()"

# 5秒後、bootloaderポートが出現しているか確認
sleep 5
ls /dev/cu.*
```

成功すると `cu.usbmodem0000000000011` のようなbootloaderポートが現れる。

### キーマップ経由

`&bootloader` バインドを任意のキーに割り当てておけば、そのキーを押すだけで起動できる。（現状は未設定）

## Web Configuratorでの書き込み

ローカル起動: `/tmp/bmp-webconfig` （カスタムファームを登録済み）

1. 上記方法でbootloader起動（`cu.usbmodem0000000000011` が見える状態にする）
2. Web ConfiguratorでUpdate Applicationを開く
3. ファーム選択:
   - 左（ペリフェラル）: `sekigon_torabo_tsuki_mymap_1_0_0`
   - 右（セントラル・トラックボール側）: `sekigon_torabo_tsuki_mymap_right_1_0_0`
4. **Update** → ポートピッカーで `cu.usbmodem0000000000011` を選択
5. 自動で書き込み開始

左右両方とも同じ手順で実施。

## トラブルシュート

- **ポートが `Resource busy`**: Web Configuratorのタブを閉じる（またはリロード）するとポートが解放される
- **1200bps後にポートが消えたまま戻らない**: キーボード電源OFF → ON で復帰。USBケーブルがデータ通信対応か確認
- **書き込み中に進捗が出ない**: 1回目のUpdateは「Bootloader is activated」の偽通知。実際は遷移していないので1200bps方式で確実にbootloaderに入れる
