# ports/rp2/mdx/micropython.cmake

# モジュール（mdx）の定義
mp_compile_user_cmodule(mdx)

# コンパイル対象のCファイルを指定
target_sources(usermod_mdx INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/modmdx.c
    
    # 🌟 gamdx内のコンパイルに必要な3つのファイルを指定
    ${CMAKE_CURRENT_LIST_DIR}/gamdx/mxdrvg/mxdrvg.c
    ${CMAKE_CURRENT_LIST_DIR}/gamdx/mxdrvg/mxdrv_v.c
    ${CMAKE_CURRENT_LIST_DIR}/gamdx/mxdrvg/mxdrv_f.c
)

# インクルードパス（ヘッダーファイルを探す場所）にこのフォルダを追加
target_include_directories(usermod_mdx INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}
)

# Pico SDK の I2S オーディオライブラリをリンクする
target_link_libraries(usermod_mdx INTERFACE
    pico_audio_i2s
)
