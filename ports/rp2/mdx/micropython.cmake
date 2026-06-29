# ports/rp2/mdx/micropython.cmake

# モジュール（usermod_mdx）の定義
add_library(usermod_mdx INTERFACE)

# ⭕ 元のCMakeLists.txtに書かれていた「本物のファイル一覧」に修正！
target_sources(usermod_mdx INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/modmdx.c
    ${CMAKE_CURRENT_LIST_DIR}/gamdx/downsample/downsample.cpp
    ${CMAKE_CURRENT_LIST_DIR}/gamdx/fmgen/fmgen.cpp
    ${CMAKE_CURRENT_LIST_DIR}/gamdx/fmgen/fmtimer.cpp
    ${CMAKE_CURRENT_LIST_DIR}/gamdx/fmgen/opm.cpp
    ${CMAKE_CURRENT_LIST_DIR}/gamdx/mxdrvg/so.cpp
    ${CMAKE_CURRENT_LIST_DIR}/gamdx/mxdrvg/opm_delegate.cpp
    ${CMAKE_CURRENT_LIST_DIR}/gamdx/pcm8/pcm8.cpp
    ${CMAKE_CURRENT_LIST_DIR}/gamdx/pcm8/x68pcm8.cpp
    ${CMAKE_CURRENT_LIST_DIR}/gamdx/mame/ym2151.c
)

# インクルードパス（ヘッダーファイルを探す場所）にこのフォルダを追加
target_include_directories(usermod_mdx INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}
)

# Pico SDK の I2S オーディオライブラリをリンクする
target_link_libraries(usermod_mdx INTERFACE
    pico_audio_i2s
)

# このモジュールをMicroPython本体（usermod）へと結合する
target_link_libraries(usermod INTERFACE usermod_mdx)
