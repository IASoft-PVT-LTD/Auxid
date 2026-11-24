set(SRC_FILES
    "cpp/main.cpp"

    "cpp/memory/arena.cpp"
    "cpp/memory/arc.cpp"
    "cpp/memory/heap.cpp"
    "cpp/memory/foreign_thread.cpp"
    "cpp/os/time.cpp"
    "cpp/os/env.cpp"
    "cpp/os/ipc.cpp"
    "cpp/os/proc.cpp"
    "cpp/os/dl.cpp"
    "cpp/core/result.cpp"
    "cpp/thread/thread.cpp"

    "cpp/containers/vec.cpp"
    "cpp/containers/string.cpp"
    "cpp/containers/format.cpp"
    "cpp/containers/option.cpp"
    "cpp/containers/hash_base.cpp"
    "cpp/containers/hash_map.cpp"
    "cpp/containers/hash_set.cpp"
    "cpp/containers/pair.cpp"
    "cpp/containers/iterator_concepts.cpp"
    "cpp/containers/slot_map.cpp"
)

add_executable(TestSuite ${SRC_FILES})
target_precompile_headers(TestSuite PRIVATE
    <new>
    <vector>
)
target_link_libraries(TestSuite PRIVATE libauxid)
if(Auxid_BUILD_SHARED AND WIN32)
    add_custom_command(TARGET TestSuite POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_RUNTIME_DLLS:TestSuite>
            $<TARGET_FILE_DIR:TestSuite>
        COMMAND_EXPAND_LISTS
    )
endif()
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND WIN32)
    # Runtime DLLs must stay dynamic (see the mingw toolchain file for the
    # rpmalloc/emutls recursion that static linking causes). Pin the
    # toolchain's exact DLLs beside the exe so they never resolve from PATH.
    get_filename_component(_auxid_mingw_bin "${CMAKE_CXX_COMPILER}" DIRECTORY)
    foreach(_auxid_runtime_dll libstdc++-6.dll libgcc_s_seh-1.dll libwinpthread-1.dll)
        if(EXISTS "${_auxid_mingw_bin}/${_auxid_runtime_dll}")
            add_custom_command(TARGET TestSuite POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${_auxid_mingw_bin}/${_auxid_runtime_dll}"
                    $<TARGET_FILE_DIR:TestSuite>
            )
        endif()
    endforeach()
endif()
set_target_properties(TestSuite PROPERTIES CXX_SCAN_FOR_MODULES ON)
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_options(TestSuite PRIVATE "LINKER:--allow-multiple-definition")
endif()
add_test(NAME TestSuite COMMAND TestSuite)
