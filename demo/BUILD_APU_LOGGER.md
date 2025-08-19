# APU Logger ビルド手順

## APUログ機能付きでビルドする方法

### 1. buildディレクトリでCMake設定
```bash
cd /home/kishima/dev/game-music-emu/build
cmake -DGME_APU_LOGGER=ON ..
```

### 2. ビルド実行
```bash
make
```

### 3. APUログデモの実行
```bash
cd demo
./apu_logger_demo test.nsf 0 10
```

## ファイル出力

実行すると以下のファイルが生成されます：
- `out.wav` - 通常の音声ファイル
- `apu_log_track0.bin` - APUレジスタログ（バイナリ）
- `apu_log_track0.txt` - APUレジスタログ（テキスト、デバッグ用）

## 既存機能への影響

- `GME_APU_LOGGER`を指定しない場合、既存機能に一切影響なし
- 通常のdemo、demo_mem、demo_multiは従来通り動作

## トラブルシューティング

### ビルドエラーが出る場合
1. C++コンパイラがC++11以降に対応しているか確認
2. 以下のように再設定してみる：
```bash
rm -rf CMakeCache.txt CMakeFiles/
cmake -DGME_APU_LOGGER=ON ..
make
```

### apu_logger_demoが生成されない場合
- `GME_APU_LOGGER=ON`が設定されているか確認
- CMakeの出力で"APU Logger demo will be built"が表示されるか確認