# Cross-compiling for Windows on Arm (64-bit) with llvm-mingw (https://github.com/mstorsjo/llvm-mingw), from
# macOS or Linux:
#   cmake -S . -B build/windows-arm64 -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64-aarch64.cmake \
#         -DLLVM_MINGW=/path/to/llvm-mingw
# LLVM_MINGW (or the environment's LLVM_MINGW) is the unpacked release; without it its bin/ must be on PATH.
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR ARM64)
if(NOT LLVM_MINGW AND DEFINED ENV{LLVM_MINGW})
    set(LLVM_MINGW $ENV{LLVM_MINGW})
endif()
if(LLVM_MINGW)
    set(LLVM_MINGW_BIN ${LLVM_MINGW}/bin/)
else()
    find_program(LLVM_MINGW_CLANG aarch64-w64-mingw32-clang REQUIRED)
    get_filename_component(LLVM_MINGW_BIN ${LLVM_MINGW_CLANG} DIRECTORY)
    get_filename_component(LLVM_MINGW ${LLVM_MINGW_BIN} DIRECTORY)
    set(LLVM_MINGW_BIN ${LLVM_MINGW_BIN}/)
endif()
set(LLVM_MINGW ${LLVM_MINGW} CACHE PATH "The llvm-mingw toolchain")
list(APPEND CMAKE_TRY_COMPILE_PLATFORM_VARIABLES LLVM_MINGW)
set(CMAKE_C_COMPILER ${LLVM_MINGW_BIN}aarch64-w64-mingw32-clang)
set(CMAKE_CXX_COMPILER ${LLVM_MINGW_BIN}aarch64-w64-mingw32-clang++)
set(CMAKE_RC_COMPILER ${LLVM_MINGW_BIN}aarch64-w64-mingw32-windres)
# libraries and packages only from the toolchain's Windows tree, never the host's (and from the roots given with
# -DCMAKE_FIND_ROOT_PATH: tools/release.py's static libraries)
list(APPEND CMAKE_FIND_ROOT_PATH ${LLVM_MINGW}/aarch64-w64-mingw32)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
