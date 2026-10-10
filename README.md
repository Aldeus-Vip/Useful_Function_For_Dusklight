# Useful Functions for Dusklight

[Dusklight](https://github.com/TwilitRealm/dusklight)（トワイライトプリンセス PC版）向けの、便利機能をまとめたネイティブ（C++）Modです。

## 機能一覧

| 機能 | 内容 | 主な設定 |
|---|---|---|
| Magic Armor 重量切替 | ルピーがあっても、マジックアーマーを「重い状態」（アイアンブーツ相当）にON/OFFできる | ON/OFF、切替ボタン、重い間も防御を残すか |
| Spinner 速度倍率 | スピナーの移動速度を倍率で変更する | 有効/無効、倍率（100〜500%）、Rを押している間だけ |
| 泳ぐ速度倍率 | ゾーラの服なしでも泳ぐ速度を上げる | 有効/無効、倍率（100〜400%）、ゾーラの服にも適用、ウルフにも適用 |

設定はゲーム内の **Mods 画面 → Useful Functions** のパネルで変更できます。値は `config.json` に保存されます。

### Magic Armor 重量切替

- 切替ボタン（初期値：**L + R + 十字キー下**）を押すたびに ON/OFF が切り替わり、トースト通知が出ます。
- ボタンは「L + R + 下 / R + 下 / L + 下 / Z + 下 / R + 上 / 無効（パネルのみ）」から選べます。
- 「重い間も防御を残す」
  - ON（初期値）：重い状態でも、ダメージはルピー消費で防ぐ（通常のマジックアーマー動作）。
  - OFF：本来のルール通り、重い間は防御しない。
- マジックアーマーを装備していないときは何も起きません。

### Spinner 速度倍率

- 移動処理（`daSpinner_c::posMove`）の間だけ速度を倍率で上げます。加速・減速・乗っていられる時間は元のままです。
- 本体の「Fast Spinner」チート（R押しで加速、上限60）とも併用できます。
- 倍率を上げすぎると、壁をすり抜けることがあります。

### 泳ぐ速度倍率

- 泳いでいる間だけ、水平方向の速度を倍率で上げます。潜る・浮く速度は変わりません。
- アイアンブーツ／重いマジックアーマーで水底を歩いているときは適用しません。
- ゾーラの服は元から速いので、初期値では適用しません（設定で変更可）。

## ビルド方法

```sh
cmake -B build
cmake --build build
```

- `build/mods/useful_functions.dusk` が生成されます。
- これを mods フォルダにコピーします：
  - Windows: `%APPDATA%\TwilitRealm\Dusklight\mods`
  - Linux: `~/.local/share/TwilitRealm/Dusklight/mods`
  - macOS: `~/Library/Application Support/TwilitRealm/Dusklight/mods`
- ローカルビルドは自分のOS専用です。配布用には GitHub Actions の `mod-combined` 成果物（全プラットフォーム対応）を使ってください。
- 対象の Dusklight バージョンは `CMakeLists.txt` の `DUSKLIGHT_VERSION`（現在 `v2.0.3`）です。

## 構成（機能の追加方法）

```
src/
├── mod.cpp            ライフサイクルとUIパネル。kFeatures に機能を並べる
├── feature.hpp        Feature インターフェース（initialize / update / buildUi）
├── config.*           ConfigService の薄いラッパー（毎フレーム値をキャッシュ）
├── ui_helpers.*       トグル・数値・ドロップダウン・トースト
└── features/
    ├── magic_armor.cpp
    ├── spinner_speed.cpp
    └── swim_speed.cpp
```

新しい機能を追加する手順：

1. `src/features/` に新しいファイルを作り、`Feature` を定義する
2. `feature.hpp` に `extern` 宣言を追加する
3. `mod.cpp` の `kFeatures` に追加する
4. `CMakeLists.txt` の `SOURCES` に追加する
