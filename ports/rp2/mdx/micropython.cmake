# ports/rp2/mdx/micropython.cmake

add_library(usermod_mdx INTERFACE)

# ソースファイル一覧を変数にまとめる
set(MDX_SOURCES
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

target_sources(usermod_mdx INTERFACE ${MDX_SOURCES})

# ⭕【ここを修正】C言語とC++で、渡すフラグをきっちり分ける処理にしました
foreach(src ${MDX_SOURCES})
    if(${src} MATCHES "\\.cpp$")
        # C++ファイルには、NULL変換警告を消すフラグも含めて全部盛る
        set_source_files_properties(${src} PROPERTIES COMPILE_FLAGS "-Wno-unused-variable -Wno-unused-function -Wno-unused-but-set-variable -Wno-conversion-null")
    elseif(${src} MATCHES "\\.c$")
        # C言語ファイルには、C++専用フラグを除外して安全に盛る
        set_source_files_properties(${src} PROPERTIES COMPILE_FLAGS "-Wno-unused-variable -Wno-unused-function -Wno-unused-but-set-variable")
    endif()
endforeach()

# インクルードパスの設定
target_include_directories(usermod_mdx INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}
)

# MicroPython本体へ結合
target_link_libraries(usermod INTERFACE usermod_mdx)
