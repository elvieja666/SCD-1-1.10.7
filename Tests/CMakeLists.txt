# Pruebas de audio del SCD-1 (no forman parte del plugin ni de la compilacion de GitHub).
# Uso:   cmake -S Tests -B build-tests -DCMAKE_BUILD_TYPE=Release && cmake --build build-tests --parallel
#        ./build-tests/RoutingTest_artefacts/Release/RoutingTest     (ruteo, parametros, Key Listen, robustez)
#        ./build-tests/DynTest_artefacts/Release/DynTest             (cambios de ruteo en caliente, fuzz, huella de salida)
# Compilan con deteccion de errores de memoria (ASan/UBSan) y sin interfaz grafica.
cmake_minimum_required(VERSION 3.22)
project(SCD1Tests VERSION 1.0.0)
set(CMAKE_CXX_STANDARD 17)

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4
    GIT_SHALLOW ON)
FetchContent_MakeAvailable(JUCE)

foreach(t RoutingTest DynTest)
    juce_add_console_app(${t} PRODUCT_NAME ${t})
    target_sources(${t} PRIVATE ${t}.cpp ../Source/PluginProcessor.cpp)
    target_include_directories(${t} PRIVATE ../Source)
    target_compile_definitions(${t} PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_MODAL_LOOPS_PERMITTED=0 SCD_TEST_BUILD=1)
    target_compile_options(${t} PRIVATE -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(${t} PRIVATE -fsanitize=address,undefined)
    target_link_libraries(${t} PRIVATE juce::juce_audio_utils juce::juce_recommended_config_flags)
endforeach()
