add_library(usermod_mdx INTERFACE)

target_sources(usermod_mdx INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/modmdx.c
)

target_include_directories(usermod_mdx INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}
)

target_link_libraries(usermod INTERFACE usermod_mdx)
