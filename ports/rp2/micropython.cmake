usermod_extension_bind_factories(mdx)

target_sources(usermod INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/modmdx.c
)
