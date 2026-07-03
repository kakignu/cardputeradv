# ClaudeOS アーキテクチャ

## 全体像

```
┌─────────────────────────────────────────────┐
│  Apps: Files Edit Term WiFi Clock Level      │
│        Paint Snake Config Info               │
├─────────────────────────────────────────────┤
│  UI toolkit: ListView TextPrompt MsgBox      │
│              hintBar / Theme                 │
├─────────────────────────────────────────────┤
│  Kernel (OS class):                          │
│   app stack / frame loop / status bar        │
│   KeyService / settings(NVS) / sound / NTP   │
│   vfs (/sd + /flash)                         │
├─────────────────────────────────────────────┤
│  HAL: M5Cardputer + M5Unified + M5GFX        │
│  (display, TCA8418 keyboard, ES8311 audio,   │
│   BMI270 IMU, power)                         │
├─────────────────────────────────────────────┤
│  Arduino-ESP32 / FreeRTOS / ESP-IDF          │
└─────────────────────────────────────────────┘
```

## カーネル(`src/os/os.h` / `os.cpp`)

`OS` はシングルトン。`setup()` で `OS::begin()`、`loop()` で `OS::tick()` が呼ばれる。

### フレームループ(~30fps)

1. `M5Cardputer.update()` — HAL 更新(キーボードスキャン結果の取り込み)
2. `KeyService::poll()` — キー状態のスナップショット差分からイベント生成
3. イベントディスパッチ — スタック最上位アプリの `onKey()`
4. `onTick()` — アプリのロジック
5. NTP 同期ポーリング(WiFi 接続後、自動)
6. 描画 — 全画面スプライト(240x135 RGB565、約 64KB)に `onDraw()` →
   ステータスバー → `pushSprite()` で一括転送(ちらつきなし)
7. 33ms にフレームペーシング

### アプリライフサイクル

```cpp
class App {
  virtual const char* title() const = 0;
  virtual void onStart();            // push 直後
  virtual void onResume();           // 上のアプリが閉じた
  virtual void onStop();             // 破棄直前
  virtual void onKey(const KeyEvent&) = 0;
  virtual void onTick();             // 毎フレーム
  virtual void onDraw(M5Canvas&) = 0;
  virtual bool fullscreen() const;   // ステータスバー非表示
};
```

`OS::launch(app)` / `OS::closeTop()` は **遅延実行**される(イベントハンドラの実行中に
自分自身を破棄しないため)。ランチャー(スタック底)は閉じられない。

## キーボードイベント(`src/os/keys.h` / `keys.cpp`)

Cardputer ADV のキーボードは TCA8418 が自律スキャンし、M5Cardputer ライブラリが
「現在押されているキーの集合」(`KeysState`)を提供する。`KeyService` は毎フレーム:

1. スナップショットを取得(特殊キーのフラグ + 押下中の印字文字ベクタ)
2. 前フレームとの差分で **エッジ(押した瞬間)** を検出しイベント化
3. 最後のキーが押しっぱなしなら **オートリピート**(450ms 後から 60ms 間隔)
4. イベント発火時にキークリック音のコールバック

矢印/Esc/Del は Fn レイヤー(`Fn+;` = ↑ など)。`KeyEvent` に修飾キー
(ctrl/shift/alt/opt/fn)が付くので、アプリ側は `e.ctrl && e.ch=='s'` のように判定する。

## VFS(`src/os/storage.h` / `storage.cpp`)

`/sd/...`(microSD、専用 SPI)と `/flash/...`(LittleFS)を一つのパス空間に統合。
ルート `/` は仮想ディレクトリで、マウント済みボリュームを列挙する。
Files / Edit / Term / Paint はすべてこの API 経由でストレージを扱う。

## 設定の永続化

`Preferences`(NVS)を使用。

- ネームスペース `claudeos`: 明るさ・音量・キークリック・起動音・テーマ・TZ・WiFi 自動接続・FPS 表示
- ネームスペース `wifi`: 最後に接続に成功した SSID / パスワード(WiFi アプリが保存、
  `wifiAuto` 有効時に起動時自動接続)

## メモリ方針

ESP32-S3FN8 は PSRAM なし(内蔵 SRAM 512KB)のため:

- 全画面キャンバスは 1 枚だけ(64.8KB)
- Paint は使用中のみ 8bit スプライト(~23KB)を確保し、終了時に解放
- エディタはファイルサイズ 30KB / 500 行に制限
- ターミナルのスクロールバックは 250 行に制限
