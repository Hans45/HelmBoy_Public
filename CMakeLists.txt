cmake_minimum_required(VERSION 3.15)

# Set a default build type if none was specified
set(default_build_type "Release")

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  message(STATUS "Setting build type to '${default_build_type}' as none was specified.")
  set(CMAKE_BUILD_TYPE "${default_build_type}" CACHE
      STRING "Choose the type of build." FORCE)
  # Set the possible values of build type for cmake-gui
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
    "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

project(HELMBOY
  VERSION 1.0.0
  DESCRIPTION "A cross-platform polyphonic synthesizer, modernised by Marc Scheffer."
  LANGUAGES C CXX)

# Force C++20 for modern features (std::span, concepts, etc.)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Enable aggressive SIMD optimizations
if(MSVC)
  # MSVC optimizations for x64 architecture
  set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /arch:AVX2 /fp:fast")
  set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} /arch:AVX2 /fp:fast")
  # Enable vectorization reports in debug builds
  set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /arch:AVX2")
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  # Clang optimizations
  set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -mavx2 -mfma -ffast-math")
  set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -mavx2 -mfma -ffast-math")
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
  # GCC optimizations
  set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -mavx2 -mfma -ffast-math -ftree-vectorize")
  set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -mavx2 -mfma -ftree-vectorize")
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# It's "bad form" to set this globally, but it's convenient.
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Debug diagnostics for safe DSP migration (disabled by default).
option(HELMBOY_DEBUG_AUDIO_LEVELS "Log periodic engine output peak levels" OFF)
option(HELMBOY_DEBUG_BUFFER_INTEGRITY "Check output buffers for NaN/Inf" OFF)
option(HELMBOY_DEBUG_DSP_PREP "Log invalid DSP preparation state" OFF)
option(HELMBOY_DEBUG_BIQUAD_ROUTING "Log Biquad JUCE-path versus fallback routing" OFF)
option(HELMBOY_DEBUG_SVF_TRANSITIONS "Log SVF style/shelf transitions and reset diagnostics" OFF)
option(HELMBOY_DEBUG_REVERB "Log Reverb wet/dry sanitization and period clamping events" OFF)
option(HELMBOY_ENABLE_BIQUAD_JUCE_PATH "Enable optional JUCE-compatible path in BiquadFilter" ON)
option(HELMBOY_ENABLE_SVF_JUCE_PATH "Enable optional JUCE-compatible path in StateVariableFilter" ON)
option(HELMBOY_ENABLE_REVERB_JUCE_PATH "Enable optional JUCE-compatible path in Reverb" ON)

# VST3 SDK Handling
set(JUCE_VST3_SDK "${CMAKE_CURRENT_SOURCE_DIR}/external/VST3_SDK")

# ==================================================

add_subdirectory(concurrentqueue)
add_subdirectory(mopo)
target_include_directories(mopo PRIVATE externals/xsimd/include)
add_subdirectory(externals/xsimd)
add_subdirectory(externals/JUCE)
target_link_libraries(mopo PRIVATE juce::juce_dsp)
# When the JUCE reverb path is active, mopo needs juce_audio_basics (juce::Reverb).
# Must be placed after add_subdirectory(externals/JUCE) so the target exists.
if(HELMBOY_ENABLE_REVERB_JUCE_PATH)
  target_link_libraries(mopo PRIVATE juce::juce_audio_basics)
endif()
add_subdirectory(externals/VST3_SDK)

juce_add_binary_data(HelmBoyData SOURCES
  fonts/DroidSansMono.ttf
  fonts/Roboto-Regular.ttf
  fonts/Roboto-Light.ttf
  fonts/Roboto-Thin.ttf
  images/helmBoy_icon_16_1x.png
  images/helmBoy_icon_16_2x.png
  images/helmBoy_icon_32_1x.png
  images/helmBoy_icon_32_2x.png
  images/helmBoy_icon_128_1x.png
  images/helmBoy_icon_128_2x.png
  images/helmBoy_icon_256_1x.png
  images/helmBoy_icon_256_2x.png
  images/helmBoy_icon_512_1x.png
  images/helmBoy_icon_512_2x.png)

# ==================================================

option(BUILD_VST3 "Build VST3 plugin" ON)
option(BUILD_AU "Build AU plugin" ON)
option(BUILD_LV2 "Build LV2 plugin" ON)
option(BUILD_AAX "Build AAX plugin" OFF)

set(PLUGIN_FORMATS "")
if(BUILD_AAX)
  list(APPEND PLUGIN_FORMATS AAX)
endif()
if(BUILD_VST3)
  list(APPEND PLUGIN_FORMATS VST3)
endif()
if(BUILD_AU)
  list(APPEND PLUGIN_FORMATS AU)
endif()
if(BUILD_LV2)
  list(APPEND PLUGIN_FORMATS LV2)
endif()

juce_add_plugin(HelmBoyPlugin
  ICON_BIG images/helmBoy_icon_512_2x.png
  ICON_SMALL images/helmBoy_icon_128_2x.png

  IS_SYNTH TRUE
  NEEDS_MIDI_INPUT TRUE
  NEEDS_MIDI_OUTPUT FALSE
  IS_MIDI_EFFECT FALSE
  EDITOR_WANTS_KEYBOARD_FOCUS TRUE

  PRODUCT_NAME "helmBoy"
  COMPANY_NAME "Marc Scheffer"
  BUNDLE_ID "com.MarcScheffer.helmBoy"
  PLUGIN_MANUFACTURER_CODE "Mars"
  FORMATS ${PLUGIN_FORMATS}

  LV2URI "https://github.com/Hans45/helmBoy"
  LV2_WANTS_LATENCY FALSE)

juce_generate_juce_header(HelmBoyPlugin)

target_sources(HelmBoyPlugin PRIVATE
  src/synthesis/helmBoy_engine.h
  src/plugin/helmboy_plugin.h
  src/common/border_bounds_constrainer.cpp
  src/common/file_list_box_model.cpp
  src/common/helmBoy_common.cpp
  src/common/load_save.cpp
  src/common/midi_manager.cpp
  src/common/midi_2_manager.cpp
  src/common/midi_2_phase4.cpp
  src/common/startup.cpp
  src/common/synth_base.cpp
  src/common/synth_gui_interface.cpp
  src/editor_components/bpm_slider.cpp
  src/editor_components/button_modulation_slider.cpp
  src/editor_components/filter_response.cpp
  src/editor_components/filter_selector.cpp
  src/editor_components/global_tool_tip.cpp
  src/editor_components/HelmBoyGraphics.cpp
  src/editor_components/graphical_envelope.cpp
  src/editor_components/graphical_step_sequencer.cpp
  src/editor_components/midi_keyboard.cpp
  src/editor_components/modulation_button.cpp
  src/editor_components/modulation_highlight.cpp
  src/editor_components/modulation_meter.cpp
  src/editor_components/modulation_slider.cpp
  src/editor_components/open_gl_background.cpp
  src/editor_components/open_gl_component.cpp
  src/editor_components/open_gl_envelope.cpp
  src/editor_components/open_gl_modulation_meter.cpp
  src/editor_components/open_gl_oscilloscope.cpp
  src/editor_components/open_gl_peak_meter.cpp
  src/editor_components/open_gl_wave_viewer.cpp
  src/editor_components/oscilloscope.cpp
  src/editor_components/patch_selector.cpp
  src/editor_components/retrigger_selector.cpp
  src/editor_components/synth_button.cpp
  src/editor_components/synth_slider.cpp
  src/editor_components/tempo_selector.cpp
  src/editor_components/text_selector.cpp
  src/editor_components/text_slider.cpp
  src/editor_components/wave_selector.cpp
  src/editor_components/wave_viewer.cpp
  src/editor_components/xy_pad.cpp
  src/editor_sections/about_section.cpp
  src/editor_sections/arp_section.cpp
  src/editor_sections/bpm_section.cpp
  src/editor_sections/chorus_section.cpp
  src/editor_sections/delay_section.cpp
  src/editor_sections/delete_section.cpp
  src/editor_sections/distortion_section.cpp
  src/editor_sections/dynamic_section.cpp
  src/editor_sections/envelope_section.cpp
  src/editor_sections/extra_mod_section.cpp
  src/editor_sections/feedback_section.cpp
  src/editor_sections/filter_section.cpp
  src/editor_sections/formant_section.cpp
  src/editor_sections/full_interface.cpp
  src/editor_sections/lfo_section.cpp
  src/editor_sections/limiter_section.cpp
  src/editor_sections/mixer_section.cpp
  src/editor_sections/modulation_manager.cpp
  src/editor_sections/noise_section.cpp
  src/editor_sections/open_gl_modulation_manager.cpp
  src/editor_sections/oscillator_section.cpp
  src/editor_sections/patch_browser.cpp
  src/editor_sections/reverb_section.cpp
  src/editor_sections/save_section.cpp
  src/editor_sections/step_sequencer_section.cpp
  src/editor_sections/stutter_section.cpp
  src/editor_sections/sub_section.cpp
  src/editor_sections/synthesis_interface.cpp
  src/editor_sections/synth_section.cpp
  src/editor_sections/voice_section.cpp
  src/editor_sections/volume_section.cpp
  src/look_and_feel/browser_look_and_feel.cpp
  src/look_and_feel/colors.cpp
  src/look_and_feel/default_look_and_feel.cpp
  src/look_and_feel/fonts.cpp
  src/look_and_feel/modulation_look_and_feel.cpp
  src/look_and_feel/shaders.cpp
  src/look_and_feel/text_look_and_feel.cpp
  src/plugin/helmBoy_editor.cpp
  src/plugin/helmBoy_plugin.cpp
  src/synthesis/dc_filter.cpp
  src/synthesis/detune_lookup.cpp
  src/synthesis/fixed_point_oscillator.cpp
  src/synthesis/fixed_point_wave.cpp
  src/synthesis/gate.cpp
  src/synthesis/helmBoy_engine.cpp
  src/synthesis/JuceExponentialSmootherWrapper.cpp
  src/synthesis/helmBoy_lfo.cpp
  src/synthesis/helmBoy_module.cpp
  src/synthesis/helmBoy_oscillators.cpp
  src/synthesis/helmBoy_voice_handler.cpp
  src/synthesis/noise_oscillator.cpp
  src/synthesis/peak_meter.cpp
  src/synthesis/resonance_cancel.cpp
  src/synthesis/switch_modulation_processor.cpp
  src/synthesis/trigger_random.cpp
  src/synthesis/value_switch.cpp)

target_include_directories(HelmBoyPlugin PUBLIC
  src/common
  src/editor_components
  src/editor_sections
  src/look_and_feel
  src/plugin
  src/synthesis)

target_include_directories(HelmBoyPlugin PRIVATE
 externals/xsimd/include)

  # Spécifie le chemin du VST3 SDK
set(VST3_SDK_PATH "${CMAKE_CURRENT_SOURCE_DIR}/externals/VST3_SDK")
set(JUCE_DEFINES
  JUCE_VST3_CAN_REPLACE_VST2=0
  JUCE_WEB_BROWSER=0
  JUCE_USE_CURL=0
  JUCE_USE_XRANDR=0
  JUCE_OPENGL=1
  JUCE_USE_OPENGL_SHADERS=1
  JUCE_VST3_SDK_PATH="${VST3_SDK_PATH}")

target_compile_definitions(HelmBoyPlugin PUBLIC
  # HelmBoy tries to detect at runtime whether it's actually installed,
  # or whether it's just being queried for its factory presets by
  # the LV2 .ttl generator. If it's the latter, Helm needs to know
  # where its factory presets are on the developer's disk. Not ideal
  # for a number of reasons, but it'll have to do for now.
  helmBoy_LV2GEN_FACTORY_PRESET_PATH="${CMAKE_CURRENT_SOURCE_DIR}/patches/Factory Presets"
  ${JUCE_DEFINES}
  $<$<BOOL:${HELMBOY_DEBUG_AUDIO_LEVELS}>:HELMBOY_DEBUG_AUDIO_LEVELS=1>
  $<$<BOOL:${HELMBOY_DEBUG_BUFFER_INTEGRITY}>:HELMBOY_DEBUG_BUFFER_INTEGRITY=1>
  $<$<BOOL:${HELMBOY_DEBUG_DSP_PREP}>:HELMBOY_DEBUG_DSP_PREP=1>
)



target_link_libraries(HelmBoyPlugin
  PUBLIC
    HelmBoyData
    concurrentqueue
    mopo

    juce::juce_audio_basics
    juce::juce_audio_devices
    juce::juce_audio_formats
    juce::juce_audio_plugin_client
    juce::juce_audio_processors
    juce::juce_audio_utils
    juce::juce_core
    juce::juce_data_structures
    juce::juce_events
    juce::juce_graphics
    juce::juce_gui_basics
    juce::juce_gui_extra
    juce::juce_opengl

    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    # juce::juce_recommended_warning_flags
    )

# ==================================================

juce_add_gui_app(HelmBoyStandalone
  ICON_BIG images/helmBoy_icon_512_2x.png
  ICON_SMALL images/helmBoy_icon_128_2x.png

  PRODUCT_NAME "helmBoy"
  COMPANY_NAME "Marc Scheffer")

juce_generate_juce_header(HelmBoyStandalone)

target_sources(HelmBoyStandalone PRIVATE
  src/standalone/helmBoy_editor.cpp
  src/standalone/main.cpp)

target_include_directories(HelmBoyStandalone PRIVATE
  externals/xsimd/include
  src/common
  src/editor_components
  src/editor_sections
  src/look_and_feel
  src/standalone
  src/synthesis
  mopo/src
  concurrentqueue)




# Define a user-configurable path for the ASIO SDK
set(HELMBOY_ASIO_SDK_PATH "${CMAKE_CURRENT_SOURCE_DIR}/externals/ASIOSDK" CACHE PATH "Path to the Steinberg ASIO SDK")

# Conditionally enable ASIO support
if(EXISTS "${HELMBOY_ASIO_SDK_PATH}/common/asiosys.h")
    message(STATUS "ASIO SDK found at ${HELMBOY_ASIO_SDK_PATH}. Enabling ASIO support.")
    target_include_directories(HelmBoyStandalone PRIVATE
        "${HELMBOY_ASIO_SDK_PATH}/common"
        "${HELMBOY_ASIO_SDK_PATH}/host")
    target_compile_definitions(HelmBoyStandalone PRIVATE
        JUCE_ASIO=1
        JUCE_ASIO_SDK="${HELMBOY_ASIO_SDK_PATH}")
else()
    message(STATUS "ASIO SDK not found. Building without ASIO support. (Set HELMBOY_ASIO_SDK_PATH to enable)")
    target_compile_definitions(HelmBoyStandalone PRIVATE JUCE_ASIO=0)
endif()

target_compile_definitions(HelmBoyStandalone PRIVATE
  JUCE_VST3_CAN_REPLACE_VST2=0
  JUCE_WEB_BROWSER=0
  JUCE_USE_CURL=0
  JUCE_USE_XRANDR=0
  JUCE_OPENGL=1
  JUCE_USE_OPENGL_SHADERS=1
  JUCE_WASAPI=1
  JUCE_DIRECTSOUND=1
  JUCE_ALSA=0
  JUCE_JACK=0
  JUCE_JACK_CLIENT_NAME="HelmBoy"
  JUCE_ALSA_MIDI_NAME="HelmBoy"
  $<$<BOOL:${HELMBOY_DEBUG_AUDIO_LEVELS}>:HELMBOY_DEBUG_AUDIO_LEVELS=1>
  $<$<BOOL:${HELMBOY_DEBUG_BUFFER_INTEGRITY}>:HELMBOY_DEBUG_BUFFER_INTEGRITY=1>
  $<$<BOOL:${HELMBOY_DEBUG_DSP_PREP}>:HELMBOY_DEBUG_DSP_PREP=1>)

target_link_libraries(HelmBoyStandalone PRIVATE HelmBoyPlugin)
target_link_libraries(HelmBoyStandalone PRIVATE HelmBoyData)

option(HELMBOY_BUILD_MIDI_TESTS "Build focused MIDI 2.0 decoder tests" OFF)
if(HELMBOY_BUILD_MIDI_TESTS)
  enable_testing()
  juce_add_console_app(HelmBoyMidiTests
    PRODUCT_NAME "HelmBoy MIDI Tests")
  juce_generate_juce_header(HelmBoyMidiTests)
  target_sources(HelmBoyMidiTests PRIVATE
    tests/midi_2_manager_tests.cpp
    src/common/midi_2_manager.cpp)
  target_include_directories(HelmBoyMidiTests PRIVATE
    src/common
    mopo/src)
  target_compile_definitions(HelmBoyMidiTests PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0)
  target_link_libraries(HelmBoyMidiTests PRIVATE
    juce::juce_audio_basics
    juce::juce_audio_devices
    juce::juce_recommended_config_flags)
  add_test(NAME Midi2ManagerTests COMMAND HelmBoyMidiTests)
endif()

option(HELMBOY_BUILD_EFFECT_TESTS "Build focused chorus and stereo effects tests" OFF)
if(HELMBOY_BUILD_EFFECT_TESTS)
  enable_testing()
  juce_add_console_app(HelmBoyEffectTests
    PRODUCT_NAME "HelmBoy Effect Tests")
  target_sources(HelmBoyEffectTests PRIVATE tests/stereo_effects_tests.cpp)
  target_include_directories(HelmBoyEffectTests PRIVATE externals/xsimd/include)
  target_compile_options(HelmBoyEffectTests PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/fp:precise>
    $<$<CXX_COMPILER_ID:GNU,Clang>:-fno-finite-math-only>)
  target_compile_definitions(HelmBoyEffectTests PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0)
  target_link_libraries(HelmBoyEffectTests PRIVATE
    mopo
    juce::juce_dsp
    juce::juce_recommended_config_flags)
  add_test(NAME StereoEffectsTests COMMAND HelmBoyEffectTests)
endif()
