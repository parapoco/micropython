usermod_extension_bind_factories(mdx)

target_sources(usermod INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/modmdx.c
)
# 自作Cモジュール（usermod）として正式登録
micropython_add_user_cmodule(
    MODULE mdx
    SRC ${CMAKE_CURRENT_LIST_DIR}/modmdx.c
)

# モジュールを常に有効化
target_compile_definitions(usermod INTERFACE
    MODULE_MDX_ENABLED=1
)
