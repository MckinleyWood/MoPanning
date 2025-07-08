cmake_minimum_required(VERSION 3.22)
project(MOPANNING VERSION 0.0.1)

add_subdirectory(JUCE)

juce_add_gui_app(MoPanning
    # VERSION ... # Set if the app version is different to the project version
    ICON_BIG resources/Mo.png # image file to use as an icon
    # ICON_SMALL ... 
    # DOCUMENT_EXTENSIONS ... # Asscociated file extensions
    # COMPANY_NAME ... # The name of the app's author
    PRODUCT_NAME "MoPanning") # The name of the final executable

juce_generate_juce_header(MoPanning)

target_sources(MoPanning
    PRIVATE
        app/Main.cpp
        source/AudioAnalyzer.cpp
        source/AudioAnalyzer.h
        source/AudioEngine.cpp
        source/AudioEngine.h
        source/GLVisualizer.cpp
        source/GLVisualizer.h
        source/MainComponent.cpp
        source/MainComponent.h
        source/MainController.cpp
        source/MainController.h
        source/SettingsComponent.cpp
        source/SettingsComponent.h)

target_compile_definitions(MoPanning
    PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:MoPanning,JUCE_PRODUCT_NAME>"
        JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:MoPanning,JUCE_VERSION>")

target_include_directories(MoPanning 
    PRIVATE 
        ${CMAKE_CURRENT_SOURCE_DIR}/source)

target_link_libraries(MoPanning
    PRIVATE
        juce::juce_audio_devices
        juce::juce_audio_formats
        juce::juce_dsp
        juce::juce_gui_extra
        juce::juce_opengl
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        # juce::juce_recommended_warning_flags # Annoying
)

target_compile_options(MoPanning
  PUBLIC
    # core warning flags
    -Wall
    -Wshadow-all
    # -Wshorten-64-to-32
    -Wstrict-aliasing
    -Wuninitialized
    -Wunused-parameter
    # -Wconversion
    -Wsign-compare
    -Wint-conversion
    -Wconditional-uninitialized
    -Wconstant-conversion
    # -Wsign-conversion # Annoying
    -Wbool-conversion
    -Wextra-semi
    -Wunreachable-code
    -Wcast-align
    -Wshift-sign-overflow
    -Wmissing-prototypes
    -Wnullable-to-nonnull-conversion
    -Wswitch-enum
    -Wpedantic
    -Wdeprecated
    -Wfloat-equal
    -Wmissing-field-initializers
    -Wno-ignored-qualifiers
    -Wmissing-field-initializers
    -Wunused-private-field
    -Woverloaded-virtual
    -Wreorder
    -Winconsistent-missing-destructor-override

    # language‐specific guards:
    $<$<OR:$<COMPILE_LANGUAGE:CXX>,$<COMPILE_LANGUAGE:OBJCXX>>:-Wzero-as-null-pointer-constant>
    $<$<OR:$<COMPILE_LANGUAGE:OBJ C>,$<COMPILE_LANGUAGE:OBJCXX>>:-Wunguarded-availability>
    $<$<OR:$<COMPILE_LANGUAGE:OBJ C>,$<COMPILE_LANGUAGE:OBJCXX>>:-Wunguarded-availability-new>
)
