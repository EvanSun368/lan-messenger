# windres resolves ICON paths relative to the working directory, not the .rc
# file. Run it from the directory that contains the resource script.
if (NOT IS_ABSOLUTE "${OBJECT}")
    set(OBJECT "${CMAKE_BINARY_DIR}/${OBJECT}")
endif()
get_filename_component(src_dir "${SOURCE}" DIRECTORY)
get_filename_component(src_name "${SOURCE}" NAME)
get_filename_component(OBJECT "${OBJECT}" ABSOLUTE)

separate_arguments(def_list NATIVE_COMMAND "${DEFINES}")
separate_arguments(inc_list NATIVE_COMMAND "${INCLUDES}")
separate_arguments(flag_list NATIVE_COMMAND "${FLAGS}")

execute_process(
    COMMAND "${CMAKE_RC_COMPILER}" -O coff ${def_list} ${inc_list} ${flag_list}
            "${src_name}" "${OBJECT}"
    WORKING_DIRECTORY "${src_dir}"
    RESULT_VARIABLE _rv
)
if (_rv)
    message(FATAL_ERROR "windres failed with exit code ${_rv}")
endif()
