# include(/path/to/Edelweiss/sdk/GenerateEDL.cmake)
# add_edl(my_app main.c) -- use the EXWORD toolchain, no gint link required.
set(_EDL_SDK_DIR "${CMAKE_CURRENT_LIST_DIR}")
set(EDL_ARENA_START 0x8c700000)
set(EDL_ARENA_END 0x8c7f0000)
set(EDL_ARENA_SIZE 0x000f0000)
configure_file("${_EDL_SDK_DIR}/edl.ld.in" "${CMAKE_CURRENT_BINARY_DIR}/edl.ld" @ONLY)
function(add_edl TARGET)
  add_executable(${TARGET} ${ARGN} "${_EDL_SDK_DIR}/start.c" "${_EDL_SDK_DIR}/minic.c")
  set_target_properties(${TARGET} PROPERTIES SUFFIX ".edl"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
  target_include_directories(${TARGET} PRIVATE "${_EDL_SDK_DIR}")
  target_compile_options(${TARGET} PRIVATE -Os -Wall -Wextra -fno-pic -fno-pie
    -fno-stack-protector -ffreestanding -fno-builtin -ffunction-sections -fdata-sections)
  target_link_options(${TARGET} PRIVATE -nostdlib -no-pie
    "-Wl,-T,${CMAKE_CURRENT_BINARY_DIR}/edl.ld" -Wl,--gc-sections
    -Wl,--emit-relocs -Wl,--build-id=none "-Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/${TARGET}.map")
  target_link_libraries(${TARGET} PRIVATE gcc)
  set_property(TARGET ${TARGET} APPEND PROPERTY LINK_DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/edl.ld")
endfunction()

# Standard gint applications: main(), normal gint headers, software library in
# the payload, and hardware services resolved through the host's import API.
function(add_gint_edl TARGET)
  find_package(Gint 2.11 REQUIRED)
  find_library(GINT_EDL_LIBRARY NAMES gint-edl REQUIRED)
  file(STRINGS "${_EDL_SDK_DIR}/gint_imports.def" IMPORT_LINES REGEX "^EDL_GINT_SYMBOL")
  set(IMPORT_SCRIPT "${CMAKE_CURRENT_BINARY_DIR}/edl-imports.ld")
  file(WRITE "${IMPORT_SCRIPT}" "/* Runtime-resolved resident services. */\n")
  foreach(LINE IN LISTS IMPORT_LINES)
    string(REGEX REPLACE "EDL_GINT_SYMBOL\\(([A-Za-z0-9_]+)\\)" "\\1" SYMBOL "${LINE}")
    file(APPEND "${IMPORT_SCRIPT}" "PROVIDE(_${SYMBOL} = 0);\n")
  endforeach()
  add_executable(${TARGET} ${ARGN} "${_EDL_SDK_DIR}/gint_start.c" "${_EDL_SDK_DIR}/gint_fs.c")
  set_target_properties(${TARGET} PROPERTIES SUFFIX ".edl" RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
  target_include_directories(${TARGET} PRIVATE "${_EDL_SDK_DIR}" "${GINT_EXWORD_INCLUDE_DIR}")
  target_compile_definitions(${TARGET} PRIVATE FXEXWORD TARGET_EXWORD GINT_EDL)
  target_compile_options(${TARGET} PRIVATE -Os -Wall -Wextra -fno-pic -fno-pie
    -fno-stack-protector -ffreestanding -fno-builtin -ffunction-sections -fdata-sections)
  target_link_options(${TARGET} PRIVATE -nostdlib -no-pie -Wl,--emit-relocs
    "-Wl,-T,${CMAKE_CURRENT_BINARY_DIR}/edl.ld" "-Wl,-T,${IMPORT_SCRIPT}"
    -Wl,--gc-sections -Wl,--build-id=none "-Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/${TARGET}.map")
  target_link_libraries(${TARGET} PRIVATE -Wl,--start-group "${GINT_EDL_LIBRARY}" c openlibm gcc -Wl,--end-group)
  set_property(TARGET ${TARGET} APPEND PROPERTY LINK_DEPENDS
    "${CMAKE_CURRENT_BINARY_DIR}/edl.ld" "${IMPORT_SCRIPT}")
endfunction()
