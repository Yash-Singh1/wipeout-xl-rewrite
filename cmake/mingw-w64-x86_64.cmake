# Cross-compiling for 64-bit Windows with MinGW-w64 (from macOS or Linux):
#   cmake -S . -B build-windows -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64-x86_64.cmake
# On Windows itself (MSYS2's MinGW-w64 or Clang environment) no toolchain file is needed.
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)
set(CMAKE_C_COMPILER x86_64-w64-mingw32-gcc)
set(CMAKE_CXX_COMPILER x86_64-w64-mingw32-g++)
set(CMAKE_RC_COMPILER x86_64-w64-mingw32-windres)
# libraries and packages only from the MinGW-w64 tree (Linux distributions', Homebrew's), never the host's
set(CMAKE_FIND_ROOT_PATH /usr/x86_64-w64-mingw32 /opt/homebrew/opt/mingw-w64/toolchain-x86_64/x86_64-w64-mingw32
    /usr/local/opt/mingw-w64/toolchain-x86_64/x86_64-w64-mingw32)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
