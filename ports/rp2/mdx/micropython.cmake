# ports/rp2/mdx/micropython.cmake

add_library(usermod_mdx INTERFACE)

set(MDX_SOURCES
    ${CMAKE_CURRENT_LIST_DIR}/modmdx.c
    #${CMAKE_CURRENT_LIST_DIR}/gamdx/downsample/downsample.cpp
    #${CMAKE_CURRENT_LIST_DIR}/gamdx/fmgen/fmgen.cpp
    #${CMAKE_CURRENT_LIST_DIR}/gamdx/fmgen/fmtimer.cpp
    #${CMAKE_CURRENT_LIST_DIR}/gamdx/fmgen/opm.cpp
    #${CMAKE_CURRENT_LIST_DIR}/gamdx/mxdrvg/so.cpp
    #${CMAKE_CURRENT_LIST_DIR}/gamdx/mxdrvg/opm_delegate.cpp
    #${CMAKE_CURRENT_LIST_DIR}/gamdx/pcm8/pcm8.cpp
    #${CMAKE_CURRENT_LIST_DIR}/gamdx/pcm8/x68pcm8.cpp
    ${CMAKE_CURRENT_LIST_DIR}/gamdx/mame/ym2151.c
)

target_sources(usermod_mdx INTERFACE ${MDX_SOURCES})

# 必要なマクロ定義のみ継承
target_compile_definitions(usermod_mdx INTERFACE
    MDX_VOLUME=40
)

foreach(src ${MDX_SOURCES})
    if(${src} MATCHES "\\.cpp$")
        set_source_files_properties(${src} PROPERTIES COMPILE_FLAGS "-Wno-unused-variable -Wno-unused-function -Wno-unused-but-set-variable -Wno-conversion-null -Wno-sign-compare -fno-exceptions -fno-rtti")
    elseif(${src} MATCHES "\\.c$")
        set_source_files_properties(${src} PROPERTIES COMPILE_FLAGS "-Wno-unused-variable -Wno-unused-function -Wno-unused-but-set-variable -Wno-implicit-function-declaration -Wno-builtin-declaration-mismatch -Wno-implicit-int -include math.h -include string.h")
    endif()
endforeach()

target_include_directories(usermod_mdx INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}
)

# ⭕ pico_audio_i2s のリンクを削除（MicroPython本体と喧嘩させないため）
target_link_libraries(usermod INTERFACE usermod_mdx)
