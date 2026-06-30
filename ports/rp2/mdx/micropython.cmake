# ports/rp2/mdx/micropython.cmake

add_library(usermod_mdx INTERFACE)

set(MDX_SOURCES
    ${CMAKE_CURRENT_LIST_DIR}/modmdx.c
    #${CMAKE_CURRENT_LIST_DIR}/gamdx/test_empty.cpp  # これだけにする
    #${CMAKE_CURRENT_LIST_DIR}/gamdx/downsample/downsample.cpp
    ${CMAKE_CURRENT_LIST_DIR}/gamdx/fmgen/fmgen.cpp
    #${CMAKE_CURRENT_LIST_DIR}/gamdx/fmgen/fmtimer.cpp
    #${CMAKE_CURRENT_LIST_DIR}/gamdx/fmgen/opm.cpp
    #${CMAKE_CURRENT_LIST_DIR}/gamdx/mxdrvg/so.cpp
    #${CMAKE_CURRENT_LIST_DIR}/gamdx/mxdrvg/opm_delegate.cpp
    #${CMAKE_CURRENT_LIST_DIR}/gamdx/pcm8/pcm8.cpp
    #${CMAKE_CURRENT_LIST_DIR}/gamdx/pcm8/x68pcm8.cpp
    #${CMAKE_CURRENT_LIST_DIR}/gamdx/mame/ym2151.c
)

target_sources(usermod_mdx INTERFACE ${MDX_SOURCES})

# 必要なマクロ定義のみ継承
target_compile_definitions(usermod_mdx INTERFACE
    MDX_VOLUME=40
)

# 🌟 音源ファイル「だけ」に安全にオプションを適用（本体には絶対に伝播させない）
foreach(src ${MDX_SOURCES})
    if(${src} MATCHES "\\.cpp$")
        set_property(SOURCE ${src} APPEND PROPERTY COMPILE_OPTIONS
            "-Wno-unused-variable"
            "-Wno-unused-function"
            "-Wno-unused-but-set-variable"
            "-Wno-conversion-null"
            "-Wno-sign-compare"
            "-fno-exceptions"
            "-fno-rtti"
            "-fno-use-cxa-atexit"
            "-fno-threadsafe-statics"
        )
    elseif(${src} MATCHES "\\.c$")
        set_property(SOURCE ${src} APPEND PROPERTY COMPILE_OPTIONS
            "-Wno-unused-variable"
            "-Wno-unused-function"
            "-Wno-unused-but-set-variable"
            "-Wno-implicit-function-declaration"
            "-Wno-builtin-declaration-mismatch"
            "-Wno-implicit-int"
            "-include" "math.h"
            "-include" "string.h"
        )
    endif()
endforeach()

target_include_directories(usermod_mdx INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}
)

# ここで本体と繋ぐ（オプションの汚染は起きない）
target_link_libraries(usermod INTERFACE usermod_mdx)
