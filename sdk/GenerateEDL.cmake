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
