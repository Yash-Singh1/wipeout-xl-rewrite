# Unit tests for the sound, disc, controller and memory card layers.
#   cmake -S tools/iotest -B build-iotest && cmake --build build-iotest && ctest --test-dir build-iotest
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
foreach(t spu_oracle adpcm adsr voice reverb cd cdda music pad card libspu api)
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_${t}.c)
        add_executable(test_${t} test_${t}.c)
        target_link_libraries(test_${t} testenv psxio)
        add_test(NAME ${t} COMMAND test_${t})
        set_tests_properties(${t} PROPERTIES ENVIRONMENT "SDL_AUDIODRIVER=dummy;WIPEOUT_NO_GAMEPAD=1")
    endif()
endforeach()
