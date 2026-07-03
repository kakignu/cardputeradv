# ClaudeOS for Cardputer ADV

**ClaudeOS** は、M5Stack **Cardputer ADV**(ESP32-S3)のために Claude がゼロから設計した小さなオペレーティングシステムです。アプリスタック型のカーネル、独自 UI ツールキット、10 個のビルトインアプリを備えています。

```
        \ | /
      -- (*) --      ClaudeOS v0.1.0
        / | \        for M5Stack Cardputer ADV
```

## 機能

| アプリ | 説明 |
|---|---|
| **Files** | microSD / 内蔵 Flash(LittleFS)のファイルブラウザ。閲覧・削除・新規作成 |
| **Edit** | プレーンテキストエディタ(カーソル移動、Ctrl+S 保存、保存確認ダイアログ) |
| **Term** | コマンドシェル(`ls` `cat` `free` `scan` `connect` `open` など 25+ コマンド) |
| **WiFi** | ネットワークスキャン・接続・資格情報の保存(NVS)・自動接続 |
| **Clock** | 大型時計。WiFi 接続時に NTP 自動同期(タイムゾーン設定可) |
| **Level** | BMI270 IMU を使った水準器 + 加速度/ジャイロのライブ表示 |
| **Paint** | 8bit キャンバスのドット絵ツール。BMP 形式で SD に保存可能 |
| **Snake** | ゲーム |
| **Config** | 明るさ・音量・キークリック音・テーマ(dark/light)・TZ などを NVS に永続化 |
| **Info** | チップ・メモリ・ストレージ・ネットワークのシステム情報 |

OS レイヤーの機能:

- **アプリスタック型カーネル** — ランチャーを底に、アプリを push/pop。安全な遅延クローズ処理
- **フルスクリーン オフスクリーンキャンバス**(240x135 RGB565)によるちらつきのない ~30fps 描画
- **キーボードイベントサービス** — TCA8418 のキー状態スナップショットをエッジ検出+オートリピート付きイベントに変換
- **ステータスバー** — アプリ名、時刻(NTP 同期後)、SD / WiFi / バッテリー表示
- **統合 VFS** — `/sd/...`(microSD)と `/flash/...`(LittleFS)を同一 API で扱う
- **サウンド** — 起動ジングル、キークリック、操作音(ES8311 コーデック経由、M5Unified)
- **グローバルホットキー** — `Ctrl+Alt+Fn+Backspace`(=Ctrl+Alt+Del)で再起動

## ビルドと書き込み

[PlatformIO](https://platformio.org/) を使用します。

```bash
pio run                 # ビルド
pio run -t upload       # USB 経由で書き込み
pio device monitor      # シリアルモニタ (115200bps)
```

初回ビルド時に ESP32 ツールチェーンと依存ライブラリ(M5Cardputer / M5Unified / M5GFX)が自動ダウンロードされます。

## キー操作(共通)

Cardputer ADV のキーボードでは矢印キーは **Fn レイヤー** にあります:

- `Fn + ;` = ↑ `Fn + .` = ↓ `Fn + ,` = ← `Fn + /` = →
- `Fn + \`` = Esc(戻る) `Fn + Backspace` = Del
- ランチャー: 矢印で選択、`Enter` で起動、`1`〜`0` でクイック起動
- ほとんどのアプリ: `Esc` で戻る

## アーキテクチャ

```
src/
├── main.cpp              エントリポイント(setup/loop → OS::begin/tick)
├── os/
│   ├── os.h/.cpp         カーネル: アプリスタック、メインループ、ステータスバー、
│   │                     設定(NVS)、サウンド、NTP 同期
│   ├── keys.h/.cpp       キーボードイベントサービス(エッジ検出+リピート)
│   ├── storage.h/.cpp    統合 VFS(/sd + /flash)
│   ├── widgets.h/.cpp    UI ツールキット(ListView / TextPrompt / MsgBox / ヒントバー)
│   ├── theme.h           カラーテーマ(dark / light)
│   └── registry.h/.cpp   アプリレジストリ+アイコン描画
└── apps/                 ビルトインアプリ(それぞれ App 基底クラスを実装)
```

詳細は [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md)、ハードウェアのピンマップは [docs/HARDWARE.md](docs/HARDWARE.md) を参照してください。

## ハードウェア(Cardputer ADV)

- ESP32-S3FN8(StampS3A、8MB Flash、PSRAM なし)
- 1.14" ST7789V2 IPS LCD 240x135(SPI: G33-G38)
- 56 キーキーボード(TCA8418 I2C スキャナ、INT=G11)
- ES8311 オーディオコーデック + 1W スピーカー、MEMS マイク
- BMI270 6軸 IMU、microSD(SPI: G12/G14/G39/G40)、IR 送信(G44)、1750mAh バッテリー

HAL には [M5Cardputer](https://github.com/m5stack/M5Cardputer) / [M5Unified](https://github.com/m5stack/M5Unified) / [M5GFX](https://github.com/m5stack/M5GFX)(いずれも MIT ライセンスの OSS)を使用し、その上の OS レイヤーはすべて独自実装です。

## ライセンス

MIT
