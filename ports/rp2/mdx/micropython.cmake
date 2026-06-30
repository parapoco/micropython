# ports/rp2/mdx/micropython.cmake

add_library(usermod_mdx INTERFACE)

# 1. gamdx フォルダ以下の .cpp と .c をすべて再帰的に自動収集
file(GLOB_RECURSE GAMDX_SOURCES "${CMAKE_CURRENT_LIST_DIR}/gamdx/*.cpp" "${CMAKE_CURRENT_LIST_DIR}/gamdx/*.c")

# 2. modmdx.c と 合体
set(MDX_SOURCES ${CMAKE_CURRENT_LIST_DIR}/modmdx.c ${GAMDX_SOURCES})

target_sources(usermod_mdx INTERFACE ${MDX_SOURCES})

# 3. 必要なマクロ定義
target_compile_definitions(usermod_mdx INTERFACE
    MDX_VOLUME=40
)

# 4. コンパイルオプション（音源ファイル群に適用）
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
        set_property(SOURCE ${src} APPEND PROPERTY COMPLY_OPTIONS
            "-Wno-unused-variable"
            "-Wno-unused-function"
            "-Wno-unused-but-set-variable"
            "-Wno-implicit-function-declaration"
            "-Wno-builtin-declaration-mismatch"
            "-Wno-implicit-int"
        )
    endif()
endforeach()

target_include_directories(usermod_mdx INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/gamdx
)

target_link_libraries(usermod INTERFACE usermod_mdx)
