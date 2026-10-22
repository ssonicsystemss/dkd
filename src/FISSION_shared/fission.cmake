# FISSION shared look (one copy for all FISSION plugins). In a plugin's CMakeLists.txt:
#   include(${CMAKE_CURRENT_SOURCE_DIR}/../../FISSION_shared/fission.cmake)
#   fission_use(<plugin target>)      # and the same for the render test tool
# Adds Fission.cpp, the include path and the embedded JetBrains Mono fonts (OFL, see fonts/JetBrainsMono-OFL.txt).

set(FISSION_DIR "${CMAKE_CURRENT_LIST_DIR}")

function(fission_use target)
    target_sources(${target} PRIVATE "${FISSION_DIR}/Fission.cpp")
    target_include_directories(${target} PRIVATE "${FISSION_DIR}")
    # OWN SOUND (src/SONICSYSTEMS_shared/OwnSound.h): load your own sample; MP3 needs JUCE's decoder switched on
    target_include_directories(${target} PRIVATE "${FISSION_DIR}/../SONICSYSTEMS_shared")
    target_compile_definitions(${target} PRIVATE JUCE_USE_MP3AUDIOFORMAT=1)
    if(NOT TARGET FissionFonts)
        juce_add_binary_data(FissionFonts HEADER_NAME FissionFonts.h NAMESPACE FissionFonts
            SOURCES "${FISSION_DIR}/fonts/JetBrainsMonoRegular.ttf"
                    "${FISSION_DIR}/fonts/JetBrainsMonoSemiBold.ttf"
                    "${FISSION_DIR}/fonts/JetBrainsMonoExtraBold.ttf")
        set_target_properties(FissionFonts PROPERTIES POSITION_INDEPENDENT_CODE TRUE)
    endif()
    target_link_libraries(${target} PRIVATE FissionFonts)
endfunction()
