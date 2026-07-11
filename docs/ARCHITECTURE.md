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

## マルチタスク(`src/os/proc.h` / `proc.cpp`)

v0.2.0 で「案C: ハイブリッド構成」のマルチタスクを実装した。

```
UIスレッド (loopTask, core 1)          バックグラウンドジョブ (FreeRTOSタスク)
┌──────────────────────┐             ┌──────────────┐ ┌──────────────┐
│ アプリスタック / 描画   │             │ copy (core 0) │ │ music (core 0)│
│ 画面は自分だけが触る    │             │ 進捗を報告     │ │ 音を鳴らす     │
└──────┬───────────────┘             └──────┬───────┘ └──────────────┘
       │  proc::list() ←── ジョブテーブル(mutex保護) ──┘
       │  トースト表示  ←── 通知キュー(FreeRTOS queue) ← ctx.post()
       └── vfs::* ──→ FSミューテックス ←── vfs::* (ジョブ側)
```

設計判断:

1. **UIシングルスレッド原則** — 画面(M5Canvas)はUIスレッドだけが触る。
   ジョブは `ctx.progress` と `ctx.post()`(通知キュー)で結果を返す。
   AndroidのメインスレッドやJSイベントループと同じ考え方。
2. **プロセステーブル** — 固定8スロット。pid・名前・進捗・スタック残量
   (`uxTaskGetStackHighWaterMark`)・経過時間を mutex 保護で公開。
3. **協調キャンセル vs 強制kill** — `kill` は `ctx.cancel` を立てるだけで、
   ジョブが自分で後片付けして終わる。`kill -9`(`vTaskDelete`)は即死だが、
   ロックを握ったまま死ぬと後続がデッドロックする — 実OSと同じトレードオフ。
4. **共有資源の直列化** — SD/LittleFS は再入可能ミューテックスで保護。
   `vfs::copyFile` は4KBチャンクごとにロックを取り直すので、コピー中も
   UIスレッドのファイル操作が割り込める(飢餓しない)。
5. **スピーカーのチャネル分離** — UI効果音は ch0、音楽ジョブは ch1 を使い、
   タスク間の競合面を減らしている。

学習ポイント: Tasksアプリの「Jobs」ビューでスタック残量が減っていく様子や、
`worker 30 &` を複数起動して `ps` で観察、`kill -9` 後の挙動などが実験できる。

## メモリ方針

ESP32-S3FN8 は PSRAM なし(内蔵 SRAM 512KB)のため:

- 全画面キャンバスは 1 枚だけ(64.8KB)
- Paint は使用中のみ 8bit スプライト(~23KB)を確保し、終了時に解放
- エディタはファイルサイズ 30KB / 500 行に制限
- ターミナルのスクロールバックは 250 行に制限
