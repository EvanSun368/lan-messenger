# This file is part of Telegram Desktop,
# the official desktop application for the Telegram messaging service.
#
# For license and copyright information please follow this link:
# https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL

# On official MSVC builds this calls Microsoft midl. MinGW / Linux
# cross-builds have no midl; they try Wine widl, then fall back to the
# committed headers under SourceFiles/platform/win/midl/.

function(generate_midl target_name src_loc)
    set(gen_dst ${CMAKE_CURRENT_BINARY_DIR}/gen)
    file(MAKE_DIRECTORY ${gen_dst})

    if (build_winarm)
        set(env arm64)
        set(widl_machine -m64)
    elseif (build_win64)
        set(env x64)
        set(widl_machine -m64)
    else()
        set(env win32)
        set(widl_machine -m32)
    endif()

    find_program(MIDL_EXECUTABLE midl)
    find_program(WIDL_EXECUTABLE widl)

    set(gen_timestamp ${gen_dst}/${target_name}_midl.timestamp)
    set(gen_files "")
    set(full_generation_sources "")
    set(midl_fallback_dir ${src_loc}/platform/win/midl)
    foreach (file ${ARGN})
        list(APPEND full_generation_sources ${src_loc}/${file})
        get_filename_component(file_name ${file} NAME_WLE)
        list(APPEND gen_files
            ${gen_dst}/${file_name}_i.c
            ${gen_dst}/${file_name}_h.h
        )
        if (MIDL_EXECUTABLE)
            list(APPEND gen_commands
            COMMAND
                ${MIDL_EXECUTABLE}
                /out ${gen_dst}
                /h ${file_name}_h.h
                /env ${env}
                /notlb
                ${src_loc}/${file}
            )
        elseif (WIDL_EXECUTABLE)
            list(APPEND gen_commands
            COMMAND
                ${WIDL_EXECUTABLE}
                ${widl_machine}
                -h ${gen_dst}/${file_name}_h.h
                --iid=${gen_dst}/${file_name}_i.c
                ${src_loc}/${file}
            )
        else()
            list(APPEND gen_commands
            COMMAND
                ${CMAKE_COMMAND} -E copy_if_different
                    ${midl_fallback_dir}/${file_name}_h.h
                    ${gen_dst}/${file_name}_h.h
            COMMAND
                ${CMAKE_COMMAND} -E copy_if_different
                    ${midl_fallback_dir}/${file_name}_i.c
                    ${gen_dst}/${file_name}_i.c
            )
            list(APPEND full_generation_sources
                ${midl_fallback_dir}/${file_name}_h.h
                ${midl_fallback_dir}/${file_name}_i.c
            )
        endif()
    endforeach()

    add_custom_command(
    OUTPUT
        ${gen_timestamp}
    BYPRODUCTS
        ${gen_files}
    ${gen_commands}
    COMMAND
        ${CMAKE_COMMAND} -E touch ${gen_timestamp}
    COMMENT "Generating headers from IDLs (${target_name})"
    DEPENDS
        ${full_generation_sources}
    )
    generate_target(${target_name} midl ${gen_timestamp} "${gen_files}" ${gen_dst})
endfunction()
