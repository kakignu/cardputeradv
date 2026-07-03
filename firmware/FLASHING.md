# ClaudeOS 書き込み手順

`claudeos-v0.1.0-merged.bin` は bootloader + パーティションテーブル + アプリを
1 つにまとめた**オフセット 0x0 に書くだけ**のイメージです。PlatformIO のインストールは不要です。

## 方法 1: ブラウザから書き込む(いちばん簡単)

Chrome / Edge(WebSerial 対応ブラウザ)で以下を開きます:

**https://espressif.github.io/esptool-js/**

1. Cardputer ADV を USB-C ケーブルで PC に接続し、電源スイッチを ON にする
2. ページの **Program** セクションで Baudrate `921600` のまま **Connect** を押す
3. ポート選択ダイアログで `USB JTAG/serial debug unit`(または COMx / cu.usbmodem...)を選ぶ
4. **Flash Address** を `0x0` にして、**Choose File** で `claudeos-v0.1.0-merged.bin` を選択
5. **Program** を押して完了まで待つ(1〜2 分)
6. 終わったら本体を再起動(電源を入れ直す)→ ClaudeOS が起動します

※ 接続できない場合: `G0` キー(本体側面の BOOT ボタン)を押しながら USB を挿すと
ダウンロードモードに入ります。

## 方法 2: esptool.py(コマンドライン)

```bash
pip install esptool
esptool.py --chip esp32s3 --baud 921600 write_flash 0x0 claudeos-v0.1.0-merged.bin
```

## 方法 3: ソースからビルドして書き込む

```bash
git clone -b claude/cardputer-os-xjqol1 https://github.com/kakignu/cardputeradv.git
cd cardputeradv
pio run -t upload
```

## 書き込み後にシリアルログを見たい場合

```bash
pio device monitor        # または任意のシリアルモニタで 115200bps
```
