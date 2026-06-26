# ====================================================================
# MicroPython ユーザーCモジュール 登録スクリプト
# ====================================================================

# CモジュールのソースコードとヘッダーをMicroPythonのビルドシステムに登録します
interface_userspace_cmodules_register_headers_and_sources(
    ${CMAKE_CURRENT_LIST_DIR}
    MODULE_NAME mymodule
    SOURCES mymodule.c
)
