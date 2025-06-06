cmake_minimum_required(VERSION 3.22)

project(MOPANNING VERSION 0.0.1)

add_subdirectory(JUCE)

juce_add_gui_app(MoPanning
    # VERSION ...                 # Set this if the app version is different to the project version
    # ICON_BIG ...                # ICON_* arguments specify a path to an image file to use as an icon
    # ICON_SMALL ...
    # DOCUMENT_EXTENSIONS ...     # Specify file extensions that should be associated with this app
    # COMPANY_NAME ...            # Specify the name of the app's author
    PRODUCT_NAME "MoPanning")     # The name of the final executable, which can differ from the target name

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
        # JUCE_WEB_BROWSER and JUCE_USE_CURL would be on by default, but you might not need them.
        JUCE_WEB_BROWSER=0  # If you remove this, add `NEEDS_WEB_BROWSER TRUE` to the `juce_add_gui_app` call
        JUCE_USE_CURL=0     # If you remove this, add `NEEDS_CURL TRUE` to the `juce_add_gui_app` call
        JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:MoPanning,JUCE_PRODUCT_NAME>"
        JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:MoPanning,JUCE_VERSION>")

target_include_directories(MoPanning 
    PRIVATE 
        ${CMAKE_CURRENT_SOURCE_DIR}/source)

target_link_libraries(MoPanning
    PRIVATE
        # GuiAppData            # If we'd created a binary data target, we'd link to it here
        juce::juce_audio_devices
        juce::juce_audio_formats
        juce::juce_dsp
        juce::juce_gui_extra
        juce::juce_opengl
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
