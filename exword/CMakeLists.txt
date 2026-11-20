include(GenerateD01)
include(GenerateEXWORDPackage)
find_package(Gint 2.11 REQUIRED)
find_package(Python3 REQUIRED COMPONENTS Interpreter)

set(EDL_APPID "EDELW" CACHE STRING "Five-character installation ID for the embeddable D01")
if(NOT EDL_APPID MATCHES "^[A-Z0-9][A-Z0-9][A-Z0-9][A-Z0-9][A-Z0-9]$")
  message(FATAL_ERROR "EDL_APPID must contain five uppercase ASCII letters/digits")
endif()
# ROM32A profile, shared by stage 0 and the payload linker. P1 bypasses the TLB.
include(../sdk/GenerateEDL.cmake)

add_executable(edelweiss main.c fs.c paths.c profile.c native.S elf.c)
target_compile_definitions(edelweiss PRIVATE
  EDL_APPID="${EDL_APPID}" EDL_ARENA_START=${EDL_ARENA_START} EDL_ARENA_END=${EDL_ARENA_END})
target_compile_options(edelweiss PRIVATE -Wall -Wextra -Os -flto=auto
  -fno-pic -fno-pie -fno-stack-protector)
target_link_options(edelweiss PRIVATE -flto=auto -nostdlib -no-pie
  -Wl,--build-id=none -Wl,--no-warn-rwx-segments
  "-Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/edelweiss.map")
target_link_libraries(edelweiss PRIVATE Gint::Gint)
set_target_properties(edelweiss PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
generate_d01(TARGET edelweiss OUTPUT "${CMAKE_BINARY_DIR}/edelweiss.d01" MODULE edelw)

add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/assets/ebrew_assets.h"
  "${CMAKE_CURRENT_BINARY_DIR}/assets/wallpaper.c"
  "${CMAKE_CURRENT_BINARY_DIR}/assets/splash.c"
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/../tools/generate_assets.py"
    "${CMAKE_CURRENT_BINARY_DIR}/assets"
  DEPENDS ../tools/generate_assets.py ../assets/CASIO_EX.bmp VERBATIM)
add_custom_target(edl_assets DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/assets/ebrew_assets.h"
  "${CMAKE_CURRENT_BINARY_DIR}/assets/wallpaper.c" "${CMAKE_CURRENT_BINARY_DIR}/assets/splash.c")
add_edl(EBrew ../payloads/ebrew.c ../payloads/calendar.c ../payloads/splash.c
  "${CMAKE_CURRENT_BINARY_DIR}/assets/wallpaper.c" "${CMAKE_CURRENT_BINARY_DIR}/assets/splash.c")
add_edl(large-demo ../payloads/demo.c "${CMAKE_CURRENT_BINARY_DIR}/assets/wallpaper.c")
foreach(PAYLOAD IN ITEMS EBrew large-demo)
  target_include_directories(${PAYLOAD} PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/assets")
  add_dependencies(${PAYLOAD} edl_assets)
endforeach()

if(NOT DEFINED EXWORD_METADATA_SOURCE)
  message(FATAL_ERROR "Set EXWORD_METADATA_SOURCE to the EX-word HTML metadata repository")
endif()
generate_exword_package(TARGET edelweiss D01 "${CMAKE_BINARY_DIR}/edelweiss.d01"
  APPID "${EDL_APPID}" TITLE "Edelweiss"
  HTML_DIR "${EXWORD_METADATA_SOURCE}/html" OUTPUT_DIR "${CMAKE_BINARY_DIR}/packages"
  LOCALES ja cn)
foreach(LOCALE IN ITEMS ja cn)
  add_custom_target(edl_package_${LOCALE} ALL
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_FILE:EBrew>"
      "${CMAKE_BINARY_DIR}/packages/${LOCALE}/${EDL_APPID}/EBrew.edl"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_FILE:large-demo>"
      "${CMAKE_BINARY_DIR}/packages/${LOCALE}/${EDL_APPID}/large-demo.edl"
    DEPENDS edelweiss_package_${LOCALE} EBrew large-demo VERBATIM)
endforeach()

# The exact same bootstrap can be hidden inside another add-in. Its active
# content ID selects EXMPL rather than the EDELW fallback compiled above.
add_custom_target(example_bootstrap ALL
  COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${CMAKE_BINARY_DIR}/edelweiss.d01"
    "${CMAKE_BINARY_DIR}/example.d01" DEPENDS edelweiss VERBATIM)
generate_exword_package(TARGET example_bootstrap D01 "${CMAKE_BINARY_DIR}/example.d01"
  APPID EXMPL TITLE "Edelweiss ELF example"
  HTML_DIR "${EXWORD_METADATA_SOURCE}/html" OUTPUT_DIR "${CMAKE_BINARY_DIR}/packages"
  LOCALES ja cn)
foreach(LOCALE IN ITEMS ja cn)
  add_custom_target(example_payload_${LOCALE} ALL
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_FILE:large-demo>"
      "${CMAKE_BINARY_DIR}/packages/${LOCALE}/EXMPL/main.edl"
    DEPENDS example_bootstrap_package_${LOCALE} large-demo VERBATIM)
endforeach()
