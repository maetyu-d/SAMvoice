cmake_minimum_required(VERSION 3.22)
project(SpeakNSpellSynth VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
option(WINDOWS_STANDALONE_ONLY "Build standalone app only (skip AU/VST3 plugin targets)" OFF)

# Point JUCE_DIR to your JUCE checkout, e.g.
# cmake -B build -DJUCE_DIR=/path/to/JUCE
if (NOT JUCE_DIR)
    message(FATAL_ERROR "Set JUCE_DIR to your JUCE CMake folder.")
endif()

add_subdirectory(${JUCE_DIR} JUCE)

juce_add_gui_app(SpeakNSpellSynth
    PRODUCT_NAME "SAM-style Voice Synthesizer"
)

if (NOT WINDOWS_STANDALONE_ONLY)
    juce_add_plugin(SAMVoiceSynthPlugin
        COMPANY_NAME "MD"
        BUNDLE_ID "com.md.samvoicesynth"
        IS_SYNTH TRUE
        NEEDS_MIDI_INPUT TRUE
        NEEDS_MIDI_OUTPUT FALSE
        IS_MIDI_EFFECT FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS FALSE
        COPY_PLUGIN_AFTER_BUILD FALSE
        PLUGIN_MANUFACTURER_CODE MdVs
        PLUGIN_CODE SmVp
        FORMATS AU VST3 Standalone
        PRODUCT_NAME "SAM-style Voice Synthesizer Plugin"
    )
endif()

target_sources(SpeakNSpellSynth
    PRIVATE
        Source/Main.cpp
        Source/MainComponent.h
        Source/MainComponent.cpp
        Source/SpeakNSpellVoice.h
)

if (NOT WINDOWS_STANDALONE_ONLY)
    target_sources(SAMVoiceSynthPlugin
        PRIVATE
            Source/PluginProcessor.h
            Source/PluginProcessor.cpp
            Source/PluginEditor.h
            Source/PluginEditor.cpp
            Source/SpeakNSpellVoice.h
    )
endif()

target_compile_definitions(SpeakNSpellSynth
    PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        SAM_PROJECT_ROOT="${CMAKE_SOURCE_DIR}"
)

if (NOT WINDOWS_STANDALONE_ONLY)
    target_compile_definitions(SAMVoiceSynthPlugin
        PRIVATE
            JUCE_WEB_BROWSER=0
            JUCE_USE_CURL=0
            JUCE_VST3_CAN_REPLACE_VST2=0
            SAM_PROJECT_ROOT="${CMAKE_SOURCE_DIR}"
    )
endif()

target_link_libraries(SpeakNSpellSynth
    PRIVATE
        juce::juce_gui_extra
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

if (NOT WINDOWS_STANDALONE_ONLY)
    target_link_libraries(SAMVoiceSynthPlugin
        PRIVATE
            juce::juce_audio_utils
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags
            juce::juce_recommended_warning_flags
    )
endif()

function(copy_sam_runtime_assets bundleTarget)
    add_custom_command(TARGET ${bundleTarget} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_BUNDLE_CONTENT_DIR:${bundleTarget}>/Resources/Source"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_BUNDLE_CONTENT_DIR:${bundleTarget}>/Resources/third_party"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/Source/sam_bridge.js"
            "$<TARGET_BUNDLE_CONTENT_DIR:${bundleTarget}>/Resources/Source/sam_bridge.js"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/third_party/samjs.common.js"
            "$<TARGET_BUNDLE_CONTENT_DIR:${bundleTarget}>/Resources/third_party/samjs.common.js"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/third_party/better-samjs.common.js"
            "$<TARGET_BUNDLE_CONTENT_DIR:${bundleTarget}>/Resources/third_party/better-samjs.common.js"
        VERBATIM
    )
endfunction()

copy_sam_runtime_assets(SpeakNSpellSynth)
if (NOT WINDOWS_STANDALONE_ONLY)
    copy_sam_runtime_assets(SAMVoiceSynthPlugin_AU)
    copy_sam_runtime_assets(SAMVoiceSynthPlugin_VST3)
    copy_sam_runtime_assets(SAMVoiceSynthPlugin_Standalone)
endif()
