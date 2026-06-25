usermod_extension_bind_factories(mdx)

target_sources(usermod INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/modmdx.c
)
# 自作CモジュールをMicroPythonに登録
micropython_add_user_cmodule(
    MODULE mdx
    SRC ${CMAKE_CURRENT_LIST_DIR}/modmdx.c
)
