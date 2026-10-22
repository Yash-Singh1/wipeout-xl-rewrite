# The sound, disc, controller and memory card libraries (libspu, libcd, libpad, libcard, the
# events of libapi). Add with add_subdirectory(platform/psx) and link psxio.
#   PSXIO_SDL=OFF builds without SDL (no audio device, no live input): the unit tests use that.
option(PSXIO_SDL "Audio output and keyboard/gamepad input through SDL3" ON)

add_library(psxio STATIC
    spu.c disc.c cdrom.c psx_io.c libspu.c libcd.c libapi.c libcard.c pad.c)
target_include_directories(psxio PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(psxio PROPERTIES C_STANDARD 11)
if(NOT WIN32)
    target_link_libraries(psxio PUBLIC m pthread)
endif()
if(PSXIO_SDL)
    if(NOT TARGET SDL3::SDL3)
        find_package(SDL3 REQUIRED)
    endif()
    target_sources(psxio PRIVATE audio_sdl.c pad_sdl.c)
    target_link_libraries(psxio PUBLIC SDL3::SDL3)
else()
    target_sources(psxio PRIVATE audio_none.c)
    target_compile_definitions(psxio PUBLIC PSX_NO_SDL)
endif()
