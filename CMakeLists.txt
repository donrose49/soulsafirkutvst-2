cmake_minimum_required(VERSION 3.22)
project(SoulKut VERSION 1.0.0)

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 7.0.12
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

# --- Formats ---------------------------------------------------------------
# VST3 + application autonome par défaut.
# VST2 : uniquement si tu possèdes le SDK VST2 2.4 de Steinberg (il n'est plus distribué).
#   cmake -B build -DSOULKUT_VST2_SDK="C:/SDKs/vstsdk2.4"
set(SOULKUT_VST2_SDK "" CACHE PATH "Chemin du SDK VST2 (optionnel)")
set(SOULKUT_FORMATS VST3 Standalone)
if(SOULKUT_VST2_SDK)
    juce_set_vst2_sdk_path(${SOULKUT_VST2_SDK})
    list(APPEND SOULKUT_FORMATS VST)
endif()

option(SOULKUT_COPY "Copier le plugin dans le dossier VST3 du systeme apres compilation" TRUE)

juce_add_plugin(SoulKut
    COMPANY_NAME "SoulKut"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    PLUGIN_MANUFACTURER_CODE Soul
    PLUGIN_CODE Skut
    FORMATS ${SOULKUT_FORMATS}
    PRODUCT_NAME "SoulKut"
    COPY_PLUGIN_AFTER_BUILD ${SOULKUT_COPY})

juce_generate_juce_header(SoulKut)

target_sources(SoulKut PRIVATE
    Analyzer.h
    PluginProcessor.h
    PluginProcessor.cpp
    PluginEditor.h
    PluginEditor.cpp)

target_compile_definitions(SoulKut PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(SoulKut PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)
