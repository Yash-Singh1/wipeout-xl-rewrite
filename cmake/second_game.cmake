# ---- The second player's game (platform/second/): a second copy of the game in the program ----
# The game, its data, the console's libraries and the platform layer compiled again, with every global the first
# copy defines renamed (tools/second_game.py's header, in front of each source) and every variable in sections of
# its own (platform/second/sections.h), which the program puts back as they were before each new start.

set(SECOND_NAMES ${CMAKE_BINARY_DIR}/second_game_names.h)
if(APPLE)
    set(secondPrefix "_") # (what Apple's objects put in front of C names)
else()
    set(secondPrefix "-")
endif()
set(secondFirst game psxio platform)
set(secondFirstFiles $<TARGET_FILE:game> $<TARGET_FILE:psxio> $<TARGET_OBJECTS:platform>)
if(GAME_DATA_SOURCES)
    list(APPEND secondFirst gamedata)
    list(APPEND secondFirstFiles $<TARGET_FILE:gamedata>)
endif()
add_custom_command(OUTPUT ${SECOND_NAMES}
    COMMAND python3 ${CMAKE_SOURCE_DIR}/tools/second_game.py names ${CMAKE_NM} ${secondPrefix} ${SECOND_NAMES}
            ${secondFirstFiles}
    DEPENDS ${secondFirst} ${secondFirstFiles} ${CMAKE_SOURCE_DIR}/tools/second_game.py
    COMMENT "The second player's game's names"
    COMMAND_EXPAND_LISTS
    VERBATIM)
add_custom_target(second_names DEPENDS ${SECOND_NAMES})

# A second copy of a target's sources, compiled as it is, under the second game's names.
function(second_game_copy name original)
    add_library(${name} OBJECT ${ARGN})
    foreach(property COMPILE_OPTIONS COMPILE_DEFINITIONS INCLUDE_DIRECTORIES C_STANDARD C_EXTENSIONS)
        get_target_property(value ${original} ${property})
        if(value)
            set_property(TARGET ${name} PROPERTY ${property} "${value}")
        endif()
    endforeach()
    get_target_property(libraries ${original} LINK_LIBRARIES)
    if(libraries)
        target_link_libraries(${name} PRIVATE ${libraries}) # (their include folders and definitions)
    endif()
    target_compile_options(${name} BEFORE PRIVATE "SHELL:-include ${SECOND_NAMES}")
    target_compile_definitions(${name} PRIVATE PORT_SECOND_GAME SDL_MAIN_HANDLED)
    # (the game's own format() is renamed, and with it the word in SDL's headers' printf attributes: their checks
    # are the first game's)
    if(CMAKE_C_COMPILER_ID MATCHES "Clang")
        target_compile_options(${name} PRIVATE -Wno-unknown-attributes)
    else()
        target_compile_options(${name} PRIVATE -Wno-attributes)
    endif()
    target_include_directories(${name} PRIVATE ${CMAKE_SOURCE_DIR}/platform)
    add_dependencies(${name} second_names)
endfunction()

second_game_copy(game_second game ${GAME_SOURCES})
get_target_property(psxioSources psxio SOURCES)
list(TRANSFORM psxioSources PREPEND ${CMAKE_SOURCE_DIR}/platform/psx/ REGEX "^[^/]")
second_game_copy(psxio_second psxio ${psxioSources})
# (the debug console and the touch controls: the first game's alone, inert.c in the second's place)
set(secondPlatformSources ${PLATFORM_OWN_SOURCES})
list(FILTER secondPlatformSources EXCLUDE REGEX "\\.cpp$")
second_game_copy(platform_second platform ${secondPlatformSources} ${CMAKE_SOURCE_DIR}/platform/second/inert.c)
set(secondObjects $<TARGET_OBJECTS:game_second> $<TARGET_OBJECTS:psxio_second> $<TARGET_OBJECTS:platform_second>)
set(secondTargets game_second psxio_second platform_second)
if(GAME_DATA_SOURCES)
    second_game_copy(gamedata_second gamedata ${GAME_DATA_SOURCES})
    list(APPEND secondObjects $<TARGET_OBJECTS:gamedata_second>)
    list(APPEND secondTargets gamedata_second)
endif()

if(CMAKE_C_COMPILER_ID MATCHES "Clang")
    # (sections.h's pragma has put its variables in their sections)
    target_sources(wipeoutxl PRIVATE ${secondObjects})
    set(secondLinked ${secondObjects})
else()
    # GCC: its objects linked into one, their variables gathered into sections of their own (second.ld)
    set(secondObject ${CMAKE_BINARY_DIR}/second_game.o)
    set(secondLinked ${secondObject})
    if(WIN32)
        set(secondFormat -Wl,--oformat,pe-x86-64) # (an object, not a program)
    else()
        set(secondFormat "")
    endif()
    add_custom_command(OUTPUT ${secondObject}
        COMMAND ${CMAKE_C_COMPILER} -r -nostdlib -Wl,-T,${CMAKE_SOURCE_DIR}/platform/second/second.ld
                ${secondFormat} -o ${secondObject} ${secondObjects}
        DEPENDS ${secondTargets} ${secondObjects} ${CMAKE_SOURCE_DIR}/platform/second/second.ld
        COMMENT "The second player's game"
        COMMAND_EXPAND_LISTS
        VERBATIM)
    set_source_files_properties(${secondObject} PROPERTIES EXTERNAL_OBJECT TRUE GENERATED TRUE)
    target_sources(wipeoutxl PRIVATE ${secondObject})
endif()

# ... and it shares no global with the first game (one not renamed would be the first's)
set(secondChecked ${CMAKE_BINARY_DIR}/second_game_checked)
add_custom_command(OUTPUT ${secondChecked}
    COMMAND python3 ${CMAKE_SOURCE_DIR}/tools/second_game.py check ${CMAKE_NM} ${secondPrefix} ${secondChecked}
            ${secondFirstFiles} -- ${secondLinked}
    DEPENDS ${secondFirst} ${secondFirstFiles} ${secondTargets} ${secondLinked} ${CMAKE_SOURCE_DIR}/tools/second_game.py
    COMMENT "The second player's game shares nothing with the first"
    COMMAND_EXPAND_LISTS
    VERBATIM)
add_custom_target(second_checked ALL DEPENDS ${secondChecked})
add_dependencies(wipeoutxl second_checked)
