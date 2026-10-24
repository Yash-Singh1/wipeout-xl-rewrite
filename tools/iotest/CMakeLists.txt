# Unit tests for the sound, disc, controller and memory card layers.
#   cmake -S tools/iotest -B build/iotest && cmake --build build/iotest && ctest --test-dir build/iotest
# Tests that need the PS1 disc image look for it next to the repository (../wipeout-xl) or at
# $WIPEOUT_XL_DISC / $WIPEOUT_XL_ROOT, and skip when it is not there.
cmake_minimum_required(VERSION 3.20)
project(psxio_tests C)
set(CMAKE_C_STANDARD 11)
set(PSXIO_SDL ON CACHE BOOL "")
add_subdirectory(../../platform/psx psxio)
target_compile_options(psxio PRIVATE -Wall -Wextra)

get_filename_component(REPO_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../.. ABSOLUTE)
add_library(testenv STATIC testenv.c)
target_link_libraries(testenv PUBLIC psxio)
target_compile_definitions(testenv PUBLIC REPO_DIR="${REPO_DIR}")
enable_testing()
foreach(t spu_oracle adpcm adsr voice reverb disc cd cdda music pad card libspu api soak)
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_${t}.c)
        add_executable(test_${t} test_${t}.c)
        target_link_libraries(test_${t} testenv psxio)
        add_test(NAME ${t} COMMAND test_${t})
        set_tests_properties(${t} PROPERTIES ENVIRONMENT "SDL_AUDIODRIVER=dummy;WIPEOUT_NO_GAMEPAD=1")
    endif()
endforeach()

# Disc-image conversion by itself: no psxio link, and generated fixtures need no game disc. The first build
# exercises CHD's raw-DEFLATE fallback; the other uses zlib when it is present.
if(NOT TARGET SDL3::SDL3)
    find_package(SDL3 CONFIG QUIET)
endif()
find_package(Python3 COMPONENTS Interpreter QUIET)
if(TARGET SDL3::SDL3 AND Python3_Interpreter_FOUND)
    set(CHD_SOURCES chd_decode.c ../../platform/chd.c
        ../../platform/third_party/lzma/LzmaDec.c ../../platform/third_party/lzma/7zAlloc.c)
    add_executable(chd_decode ${CHD_SOURCES})
    target_include_directories(chd_decode PRIVATE ../../platform)
    target_compile_options(chd_decode PRIVATE -Wall -Wextra)
    target_link_libraries(chd_decode PRIVATE SDL3::SDL3)
    add_test(NAME chd COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/test_chd.py $<TARGET_FILE:chd_decode>)
    find_package(ZLIB QUIET)
    if(ZLIB_FOUND)
        add_executable(chd_decode_zlib ${CHD_SOURCES})
        target_include_directories(chd_decode_zlib PRIVATE ../../platform)
        target_compile_definitions(chd_decode_zlib PRIVATE PORT_HAVE_ZLIB)
        target_compile_options(chd_decode_zlib PRIVATE -Wall -Wextra)
        target_link_libraries(chd_decode_zlib PRIVATE SDL3::SDL3 ZLIB::ZLIB)
        add_test(NAME chd_zlib COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/test_chd.py $<TARGET_FILE:chd_decode_zlib>)
    endif()
endif()
