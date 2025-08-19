# ESP32移植時の注意点

## ビルド設定

### 必要なコンポーネント
```cmake
# CMakeLists.txt
set(COMPONENT_SRCS 
    "gme/gme.cpp"
    "gme/Music_Emu.cpp"
    "gme/Classic_Emu.cpp"
    "gme/Nsf_Emu.cpp"
    "gme/Nsfe_Emu.cpp"
    "gme/Nes_Apu.cpp"
    "gme/Nes_Cpu.cpp"
    "gme/Nes_Oscs.cpp"
    "gme/Nes_Fme7_Apu.cpp"
    "gme/Nes_Namco_Apu.cpp"
    "gme/Nes_Vrc6_Apu.cpp"
    "gme/Nes_Fds_Apu.cpp"
    "gme/Nes_Vrc7_Apu.cpp"
    "gme/Blip_Buffer.cpp"
    "gme/Multi_Buffer.cpp"
    "gme/Gme_File.cpp"
    "gme/Data_Reader.cpp"
    "gme/Dual_Resampler.cpp"
    "gme/Effects_Buffer.cpp"
    "gme/Fir_Resampler.cpp"
    "gme/M3u_Playlist.cpp"
    "gme/ext/emu2413.c"
    "gme/ext/panning.c"
    "esp32_nsf_player.c"
)

set(COMPONENT_ADD_INCLUDEDIRS "gme" ".")
```

### コンパイラフラグ
```cmake
# 最適化設定
target_compile_options(${COMPONENT_LIB} PRIVATE 
    -O2                    # 最適化レベル
    -ffast-math           # 高速数学演算
    -fno-exceptions       # 例外無効化
    -fno-rtti            # RTTI無効化
)

# NSF専用定義
target_compile_definitions(${COMPONENT_LIB} PRIVATE
    USE_GME_NSF=1
    USE_GME_NSFE=1
    BLARGG_LITTLE_ENDIAN=1
    GME_DISABLE_STEREO_DEPTH=1  # ステレオエフェクト無効
)
```

## プラットフォーム固有の修正

### 1. エンディアン対応
```cpp
// gme/blargg_endian.h で確認済み
// ESP32は little endian なので問題なし
#define BLARGG_LITTLE_ENDIAN 1
```

### 2. メモリ管理
```cpp
// malloc/free の代わりに ESP32 heap 関数使用可能
#include "esp_heap_caps.h"

// 大きなバッファは外部RAM使用
void* buffer = heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
```

### 3. ファイルI/O修正
```cpp
// SPIFFS/FatFS 対応
#include "esp_spiffs.h"

// ファイルパス修正
const char* nsf_path = "/spiffs/music.nsf";
```

### 4. 浮動小数点演算
```cpp
// ESP32 FPU使用設定
// menuconfig で "Floating point unit (FPU)" を "Hardware" に設定
```

## メモリ配置最適化

### 1. IRAM配置
```cpp
// 高頻度呼び出し関数をIRAMに配置
IRAM_ATTR void nsf_player_generate_frame(void);
IRAM_ATTR void audio_frame_callback(TimerHandle_t timer);
```

### 2. DRAM/PSRAM使用
```cpp
// 大きなバッファは外部RAM
EXT_RAM_ATTR static char nsf_file_buffer[1024*1024];

// 頻繁アクセスは内部RAM
DRAM_ATTR static short audio_buffer[512];
```

### 3. ROMデータ配置
```cpp
// 定数データをFlashに配置
PROGMEM const char default_nsf[] = { /* NSF data */ };
```

## リアルタイム制約対応

### 1. タスク設定
```cpp
// 高優先度タスク作成
xTaskCreatePinnedToCore(
    audio_task,           // タスク関数
    "AudioTask",          // タスク名
    4096,                 // スタックサイズ
    NULL,                 // パラメータ
    configMAX_PRIORITIES-1, // 最高優先度
    NULL,                 // タスクハンドル
    1                     // CPU1に固定（CPU0はWiFi等で使用）
);
```

### 2. 割り込み処理
```cpp
// タイマー割り込みでフレーム生成
esp_timer_handle_t frame_timer;
esp_timer_create_args_t timer_args = {
    .callback = &audio_frame_callback,
    .name = "audio_frame"
};
esp_timer_create(&timer_args, &frame_timer);
esp_timer_start_periodic(frame_timer, 16667); // 60Hz
```

### 3. DMA使用
```cpp
// I2S DMAでPWM出力の代替
#include "driver/i2s.h"

i2s_config_t i2s_config = {
    .mode = I2S_MODE_MASTER | I2S_MODE_TX,
    .sample_rate = 15000,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_I2S,
    .dma_buf_count = 4,
    .dma_buf_len = 256,
    .use_apll = false
};
```

## デバッグとプロファイリング

### 1. メモリ使用量監視
```cpp
#include "esp_system.h"

void print_memory_info() {
    printf("Free heap: %d bytes\n", esp_get_free_heap_size());
    printf("Min free heap: %d bytes\n", esp_get_minimum_free_heap_size());
    
    #if CONFIG_SPIRAM_SUPPORT
    printf("Free PSRAM: %d bytes\n", heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    #endif
}
```

### 2. パフォーマンス測定
```cpp
#include "esp_timer.h"

void profile_audio_generation() {
    int64_t start = esp_timer_get_time();
    nsf_player_generate_frame();
    int64_t end = esp_timer_get_time();
    
    printf("Frame generation time: %lld us\n", end - start);
}
```

### 3. スタックサイズ監視
```cpp
void check_stack_usage() {
    UBaseType_t stack_remaining = uxTaskGetStackHighWaterMark(NULL);
    printf("Stack remaining: %d bytes\n", stack_remaining * 4);
}
```

## 電力管理

### 1. 動的周波数制御
```cpp
#include "esp_pm.h"

// 電力管理設定
esp_pm_config_esp32_t pm_config = {
    .max_freq_mhz = 240,      // 最大周波数
    .min_freq_mhz = 80,       // 最小周波数
    .light_sleep_enable = false // オーディオ処理中はスリープ無効
};
esp_pm_configure(&pm_config);
```

### 2. ペリフェラル管理
```cpp
// 不要なペリフェラルを無効化
esp_wifi_stop();          // WiFi停止
esp_bt_controller_disable(); // Bluetooth停止
```

## トラブルシューティング

### よくある問題

1. **音飛び・ノイズ**
   - タスク優先度不足
   - メモリ不足
   - 割り込み処理時間超過

2. **メモリ不足**
   - NSFファイルサイズ過大
   - バッファサイズ過大
   - メモリリーク

3. **ビルドエラー**
   - C++/C言語混在問題
   - ヘッダーファイルパス不正
   - リンカー設定不備

### 解決方法

1. **音質問題**
```cpp
// バッファサイズ調整
#define SAMPLES_PER_FRAME 128  // 小さくして遅延減少

// タスク優先度上げる
xTaskCreate(..., configMAX_PRIORITIES-1, ...);
```

2. **メモリ問題**
```cpp
// ファイルストリーミング読み込み
// NSFファイル全体をRAMに展開しない方式
```

3. **ビルド問題**
```cmake
# extern "C" 追加
set_source_files_properties(*.c PROPERTIES LANGUAGE C)
set_source_files_properties(*.cpp PROPERTIES LANGUAGE CXX)
```

## 推奨開発フロー

1. **段階的実装**
   - まず基本的なNSF読み込み
   - 次にオーディオ出力
   - 最後にリアルタイム制約対応

2. **テスト方法**
   - 小さなNSFファイルから開始
   - オシロスコープで波形確認
   - 長時間動作テスト

3. **最適化順序**
   - 正確性確保
   - メモリ使用量削減
   - パフォーマンス向上
   - 電力消費削減