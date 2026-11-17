# windows.cmake -- Windows-ARM64.apk's Windows ARM64 host targets, included by
# runtime/CMakeLists.txt in place of the macOS build when WIN32 (AGENTS.md: platform-conditional,
# never breaking the macOS build). Built by scripts/windows/build.ps1 -Component Host.
#
# Inputs: JAVA_HOME (a JDK 21 for ARM64: jni.h only, jvm.dll is loaded at run time) and
# WINAPK_BIN_DIR (where winapk.exe goes). Vulkan headers, zlib, SQLite and Capstone come from vcpkg (vcpkg.json here).

if(NOT CMAKE_SYSTEM_PROCESSOR MATCHES "ARM64|arm64|aarch64")
  message(FATAL_ERROR "Windows-ARM64.apk's host builds for ARM64 only (got ${CMAKE_SYSTEM_PROCESSOR}); no ARM64EC or x64")
endif()
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT JAVA_HOME OR NOT EXISTS "${JAVA_HOME}/include/jni.h")
  message(FATAL_ERROR "jni.h not found under JAVA_HOME=${JAVA_HOME}")
endif()
find_package(VulkanHeaders CONFIG REQUIRED)
find_package(ZLIB REQUIRED)
find_package(unofficial-sqlite3 CONFIG REQUIRED)
find_package(capstone CONFIG REQUIRED)

# One version for the front end and the runtime (AGENTS.md, "Release, version").
file(STRINGS "${CMAKE_CURRENT_LIST_DIR}/../../../../internal/version/version.go" _ver REGEX "^const Version = \"")
string(REGEX REPLACE "^const Version = \"([^\"]+)\".*" "\\1" WINAPK_VERSION "${_ver}")
if(NOT WINAPK_VERSION)
  message(FATAL_ERROR "no Version in internal/version/version.go")
endif()

set(_win "${CMAKE_CURRENT_LIST_DIR}")
add_executable(winapk
  ${_win}/src/winapk_main.cpp
  ${_win}/src/apk.cpp
  ${_win}/src/run.cpp
  ${_win}/src/host_probe.cpp
  ${_win}/src/jvm_probe.cpp
  ${_win}/src/vulkan_probe.cpp
  # the UTF-8 active code page, for Unicode paths in JVM options (embedded by the linker)
  ${_win}/winapk.manifest)
target_include_directories(winapk PRIVATE "${JAVA_HOME}/include" "${JAVA_HOME}/include/win32")
target_compile_definitions(winapk PRIVATE WINAPK_VERSION="${WINAPK_VERSION}" UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
# Win32 and Vulkan structs are zero-filled from their first field ({sizeof v}, {sType}).
target_compile_options(winapk PRIVATE /W4 /EHsc -Wno-missing-field-initializers)
# vulkan-1.dll and jvm.dll load at run time: no import library for either.
target_link_libraries(winapk PRIVATE Vulkan::Headers ZLIB::ZLIB dxgi advapi32 bcrypt shell32 ole32 shcore)

# The Windows host natives (ADR-0001): loaded by winapk.exe and by a test java.exe alike.
add_library(winapk-bridge SHARED
  ${_win}/src/bridge/bridge.cpp
  ${_win}/src/bridge/log.cpp
  ${_win}/src/bridge/window.cpp
  ${_win}/src/bridge/graphics.cpp
  ${_win}/src/bridge/guest_graphics.cpp
  ${_win}/src/bridge/guest_vulkan.cpp
  ${_win}/src/bridge/surface.cpp
  ${_win}/src/bridge/guest.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../../guest/jni/bridge.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../../guest/jni/entries.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../../guest/bionic/libraries.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../../guest/bionic/threads.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../../guest/loader/faults.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../../guest/loader/elf_loader.cpp
  # upstream's NativeSqlite, unchanged but for its Windows Unicode helpers (ADR-0002)
  ${CMAKE_CURRENT_LIST_DIR}/../../src/native_sqlite.cpp)
target_include_directories(winapk-bridge PRIVATE "${JAVA_HOME}/include" "${JAVA_HOME}/include/win32")
target_include_directories(winapk-bridge PRIVATE "${CMAKE_CURRENT_LIST_DIR}/../../translator/include")
target_compile_definitions(winapk-bridge PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX _CRT_SECURE_NO_WARNINGS)
target_compile_options(winapk-bridge PRIVATE /W4 /EHsc -Wno-missing-field-initializers)
target_link_libraries(winapk-bridge PRIVATE Vulkan::Headers unofficial::sqlite3::sqlite3 user32 gdi32 shcore capstone::capstone mincore)
set(_angle_headers "${CMAKE_CURRENT_LIST_DIR}/../../../../build/windows-arm64/deps/angle/include")
if(EXISTS "${_angle_headers}/EGL/eglext_angle.h")
  target_include_directories(winapk-bridge PRIVATE "${_angle_headers}")
  target_compile_definitions(winapk-bridge PRIVATE WINAPK_HAS_ANGLE_HEADERS)
endif()

# The standalone guest ELF experiment (PLAN.md §9.1): the N0 loader and its ABI suite, without
# the JVM or a window. Same compiler flags and mitigations as winapk.exe, the process guest code
# will run in. The ELF record layouts come from upstream's translator headers.
add_executable(native-probe
  ${_win}/src/native_probe/main.cpp
  ${_win}/src/native_probe/threads.cpp
  ${_win}/src/native_probe/modules.cpp
  ${_win}/src/native_probe/libraries.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../../guest/bionic/threads.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../../guest/bionic/libraries.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../../guest/loader/faults.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../../guest/loader/elf_loader.cpp)
target_include_directories(native-probe PRIVATE
  "${CMAKE_CURRENT_LIST_DIR}/../../guest/loader" "${CMAKE_CURRENT_LIST_DIR}/../../translator/include")
target_compile_definitions(native-probe PRIVATE WINAPK_VERSION="${WINAPK_VERSION}" UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX _CRT_SECURE_NO_WARNINGS)
target_compile_options(native-probe PRIVATE /W4 /EHsc -Wno-missing-field-initializers)
target_link_libraries(native-probe PRIVATE capstone::capstone psapi mincore)

# P4's real JVM/guest JNI experiment is separate from G2's deliberately JVM-free process.
add_executable(jni-probe
  ${_win}/src/jni_probe.cpp
  ${_win}/src/jvm_probe.cpp
  ${_win}/src/host_probe.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../../guest/jni/bridge.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../../guest/jni/entries.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../../guest/bionic/libraries.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../../guest/bionic/threads.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../../guest/loader/faults.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../../guest/loader/elf_loader.cpp
  ${_win}/winapk.manifest)
target_include_directories(jni-probe PRIVATE "${JAVA_HOME}/include" "${JAVA_HOME}/include/win32"
  "${CMAKE_CURRENT_LIST_DIR}/../../translator/include")
target_compile_definitions(jni-probe PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX _CRT_SECURE_NO_WARNINGS)
target_compile_options(jni-probe PRIVATE /W4 /EHsc -Wno-missing-field-initializers)
target_link_libraries(jni-probe PRIVATE capstone::capstone mincore dxgi advapi32)

# Optional P5a host experiment: absent ANGLE headers/DLLs never block the Java/native baseline.
add_executable(angle-probe EXCLUDE_FROM_ALL
  ${_win}/src/angle_probe.cpp
  ${_win}/src/host_probe.cpp
  ${_win}/winapk.manifest)
target_include_directories(angle-probe PRIVATE
  "${JAVA_HOME}/include" "${JAVA_HOME}/include/win32"
  "${_angle_headers}")
target_compile_definitions(angle-probe PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
target_compile_options(angle-probe PRIVATE /W4 /EHsc -Wno-missing-field-initializers)
target_link_libraries(angle-probe PRIVATE Vulkan::Headers user32 dxgi advapi32)

if(WINAPK_BIN_DIR)
  set_target_properties(winapk winapk-bridge native-probe jni-probe angle-probe PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${WINAPK_BIN_DIR}")
endif()
