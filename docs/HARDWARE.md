# Cardputer ADV ハードウェアリファレンス

M5Stack 公式ドキュメント(https://docs.m5stack.com/en/core/Cardputer-Adv)およびOSSとして公開されている回路情報に基づく、ClaudeOS が対象とするハードウェアのまとめ。

## コア

| 項目 | 値 |
|---|---|
| SoC | ESP32-S3FN8(Xtensa LX7 デュアルコア、240MHz) |
| モジュール | M5Stamp-S3A |
| Flash | 8MB(PSRAM なし) |
| バッテリー | 1750mAh(電圧測定: G10 ADC) |

## ディスプレイ — ST7789V2(SPI)

| 信号 | GPIO |
|---|---|
| SCK | G36 |
| MOSI | G35 |
| DC (RS) | G34 |
| CS | G37 |
| RST | G33 |
| バックライト | G38 |

1.14 インチ IPS、240x135。ClaudeOS では M5GFX がボード自動検出でドライブする(回転=1 の横向きで使用)。

## キーボード — TCA8418RTWR(I2C)

| 信号 | GPIO |
|---|---|
| SDA | G8(共有 I2C バス) |
| SCL | G9(共有 I2C バス) |
| INT | G11(立ち下がりエッジ) |

- I2C アドレス: **0x34**
- 56 キー(物理 4x14)を 7 行 x 8 列の電気マトリクスでスキャン
- キーイベントは FIFO(最大 10 イベント)経由。M5Cardputer ライブラリ 1.1.1 の
  `TCA8418KeyboardReader` がドレインし、ClaudeOS の `KeyService` が
  エッジ検出+オートリピート付きの `KeyEvent` に変換する

旧 Cardputer でキーマトリクスに使われていた G3-G7 / G13 / G15 は ADV では EXT ヘッダに開放されている。

## オーディオ — ES8311 + NS4150B

| 信号 | GPIO |
|---|---|
| I2S SCLK | G41 |
| I2S LRCK | G43 |
| I2S DOUT(スピーカーへ) | G42 (DSDIN) |
| I2S DIN(マイクから) | G46 (ASDOUT) |
| コーデック制御 | I2C G8/G9(アドレス 0x18) |

1W スピーカー + MEMS マイク(SNR 65dB)+ 3.5mm 出力。M5Unified の Speaker/Mic クラスが扱う。

## microSD(専用 SPI バス)

| 信号 | GPIO |
|---|---|
| SCK | G40 |
| MISO | G39 |
| MOSI | G14 |
| CS | G12 |

ClaudeOS は `SPIClass(HSPI)` を G40/G39/G14/G12 で初期化し 20MHz でマウントする(`src/os/os.cpp`)。LCD とは独立したバスなので排他制御は不要。

## その他

| デバイス | 接続 |
|---|---|
| BMI270 6軸 IMU | I2C G8/G9(アドレス 0x68) |
| IR 送信 | G44 |
| Grove (HY2.0-4P) | G1 / G2 / 5V / GND |
| EXT 2.54-14P | G3 G4 G5 G6 G8 G9 G13 G14 G15 G39 G40 + 5VIN/5VOUT/GND |
| バッテリー ADC | G10 |

## I2C バス(G8/G9)上のデバイス一覧

| デバイス | 7bit アドレス |
|---|---|
| TCA8418(キーボード) | 0x34 |
| ES8311(オーディオ) | 0x18 |
| BMI270(IMU) | 0x68 |
