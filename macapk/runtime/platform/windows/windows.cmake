# windows.cmake -- Windows-ARM64.apk's Windows ARM64 host targets, included by
# runtime/CMakeLists.txt in place of the macOS build when WIN32 (AGENTS.md: platform-conditional,
# never breaking the macOS build). Built by scripts/windows/build.ps1 -Component Host.
#
# Inputs: JAVA_HOME (a JDK 21 for ARM64: jni.h only, jvm.dll is loaded at run time) and
# WINAPK_BIN_DIR (where winapk.exe goes). Vulkan headers come from vcpkg (vcpkg.json here).

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
  ${_win}/src/vulkan_probe.cpp)
target_include_directories(winapk PRIVATE "${JAVA_HOME}/include" "${JAVA_HOME}/include/win32")
target_compile_definitions(winapk PRIVATE WINAPK_VERSION="${WINAPK_VERSION}" UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
# Win32 and Vulkan structs are zero-filled from their first field ({sizeof v}, {sType}).
target_compile_options(winapk PRIVATE /W4 /EHsc -Wno-missing-field-initializers)
# vulkan-1.dll and jvm.dll load at run time: no import library for either.
target_link_libraries(winapk PRIVATE Vulkan::Headers ZLIB::ZLIB dxgi advapi32 bcrypt shell32 ole32)

# The Windows host natives (ADR-0001): loaded by winapk.exe and by a test java.exe alike.
add_library(winapk-bridge SHARED
  ${_win}/src/bridge/bridge.cpp
  ${_win}/src/bridge/log.cpp
  ${_win}/src/bridge/window.cpp)
target_include_directories(winapk-bridge PRIVATE "${JAVA_HOME}/include" "${JAVA_HOME}/include/win32")
target_compile_definitions(winapk-bridge PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
target_compile_options(winapk-bridge PRIVATE /W4 /EHsc -Wno-missing-field-initializers)
target_link_libraries(winapk-bridge PRIVATE user32 gdi32)

if(WINAPK_BIN_DIR)
  set_target_properties(winapk winapk-bridge PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${WINAPK_BIN_DIR}")
endif()
