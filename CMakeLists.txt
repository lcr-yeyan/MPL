cmake_minimum_required(VERSION 3.20)

project(MPL VERSION 0.0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(MSVC)
    add_compile_options(/W4 /permissive- /utf-8 /Zc:__cplusplus)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

add_library(mpl_core STATIC
    src/core/Rational.cpp
    src/core/TimeTypes.cpp
    src/music/Pitch.cpp
    src/music/Key.cpp
    src/music/Harmony.cpp
    src/music/Fragment.cpp
    src/music/Transforms.cpp
    src/ir/MusicIR.cpp
    src/ir/ProductionIR.cpp
    src/ir/Assets.cpp
    src/ir/Automation.cpp
    src/ir/MixIR.cpp
    src/temporal/TemporalIR.cpp
    src/passes/PassManager.cpp
    src/backend/MidiBackend.cpp
    src/util/JsonWriter.cpp
    src/Project.cpp
    src/Compiler.cpp
    src/frontend/Diagnostic.cpp
    src/frontend/Lexer.cpp
    src/frontend/Ast.cpp
    src/frontend/Parser.cpp
    src/frontend/Symbols.cpp
    src/frontend/Types.cpp
    src/frontend/ParameterRegistry.cpp
    src/frontend/Sema.cpp
    src/frontend/Lowering.cpp
    src/frontend/Frontend.cpp
    src/audio/WavFile.cpp
    src/audio/AudioExecution.cpp
    src/audio/Processors.cpp
    src/audio/ReferenceAudioBackend.cpp
)
target_include_directories(mpl_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_library(mpl_demo STATIC
    src/examples/CoreDemo.cpp
)
target_link_libraries(mpl_demo PUBLIC mpl_core)

add_executable(mplc
    tools/mplc/main.cpp
)
target_link_libraries(mplc PRIVATE mpl_demo)

enable_testing()

add_executable(mpl_tests
    tests/main.cpp
    tests/test_core.cpp
    tests/test_ir.cpp
    tests/test_lowering.cpp
    tests/test_backend.cpp
    tests/test_semantics.cpp
    tests/test_temporal.cpp
    tests/test_frontend.cpp
    tests/test_audio.cpp
    tests/test_signal.cpp
    tests/test_golden.cpp
)
target_link_libraries(mpl_tests PRIVATE mpl_demo mpl_core)
target_compile_definitions(mpl_tests PRIVATE
    MPL_GOLDEN_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/golden"
    MPL_TEST_OUTPUT_DIR="${CMAKE_CURRENT_BINARY_DIR}/test-output"
    MPL_EXAMPLES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/examples"
)

add_test(NAME mpl_core_tests COMMAND mpl_tests)
