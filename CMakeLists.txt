cmake_minimum_required(VERSION 3.20)

project(MPL VERSION 0.5.0 LANGUAGES CXX)
option(MPL_BUILD_EXPERIMENTAL_AI "Build model/context experiment layer" ON)

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
    src/core/MeterMap.cpp
    src/core/TimeTypes.cpp
    src/music/Pitch.cpp
    src/music/Key.cpp
    src/music/Harmony.cpp
    src/music/Chord.cpp
    src/music/HarmonyTimeline.cpp
    src/music/Analysis.cpp
    src/music/Instrument.cpp
    src/music/Fragment.cpp
    src/music/Rhythm.cpp
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
    src/util/JsonValue.cpp
    src/util/Sha256.cpp
    src/observe/Observations.cpp
    src/lab/Task.cpp
    src/lab/FlatEvents.cpp
    src/lab/RenderArtifacts.cpp
    src/lab/TrustedExecution.cpp
    src/analysis/SemanticDelta.cpp
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
    src/audio/AudioAnalysis.cpp
    src/audio/AudioExecution.cpp
    src/audio/Processors.cpp
    src/audio/ReferenceAudioBackend.cpp
    src/mastering/Mastering.cpp
)
target_include_directories(mpl_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_definitions(mpl_core PRIVATE
    MPL_STDLIB_DIR="${CMAKE_CURRENT_SOURCE_DIR}/stdlib"
)

# Music knowledge and contract verification are semantic infrastructure. They
# depend on the core but are not required by the compiler/runtime.
add_library(mpl_knowledge STATIC
    src/knowledge/Knowledge.cpp
    src/knowledge/TaskRequirements.cpp
    src/knowledge/KnowledgeVerifier.cpp
)
target_link_libraries(mpl_knowledge PUBLIC mpl_core)

if(MPL_BUILD_EXPERIMENTAL_AI)
# Model-agnostic experiment runner, repair loop, statistics and the optional
# provider adapter. No model code is linked into mpl_core.
add_library(mpl_experiment STATIC
    src/lab/Harness.cpp
    src/lab/Repair.cpp
    src/lab/ModelAdapters.cpp
    src/lab/Statistics.cpp
    src/knowledge/KnowledgeResolver.cpp
    src/knowledge/AdaptiveContext.cpp
)
target_link_libraries(mpl_experiment PUBLIC mpl_core mpl_knowledge)
if(WIN32)
    target_link_libraries(mpl_experiment PUBLIC winhttp)
endif()
endif()

add_library(mpl_demo STATIC
    src/examples/CoreDemo.cpp
)
target_link_libraries(mpl_demo PUBLIC mpl_core)

add_executable(mplc
    tools/mplc/main.cpp
)
target_link_libraries(mplc PRIVATE mpl_demo)

if(MPL_BUILD_EXPERIMENTAL_AI)
add_executable(mpl_lab
    tools/mpl_lab/main.cpp
)
target_link_libraries(mpl_lab PRIVATE mpl_demo mpl_knowledge mpl_experiment)
endif()

enable_testing()

set(MPL_TEST_SOURCES
    tests/main.cpp
    tests/test_core.cpp
    tests/test_ir.cpp
    tests/test_lowering.cpp
    tests/test_backend.cpp
    tests/test_semantics.cpp
    tests/test_temporal.cpp
    tests/test_frontend.cpp
    tests/test_meter.cpp
    tests/test_rhythm.cpp
    tests/test_harmony.cpp
    tests/test_modules.cpp
    tests/test_observe.cpp
    tests/test_collections.cpp
    tests/test_ties.cpp
    tests/test_sma.cpp
    tests/test_stdlib.cpp
    tests/test_lab.cpp
    tests/test_delta.cpp
    tests/test_knowledge.cpp
    tests/test_render.cpp
    tests/test_repair.cpp
    tests/test_stats.cpp
    tests/test_relations.cpp
    tests/test_benchmark.cpp
    tests/test_audio.cpp
    tests/test_signal.cpp
    tests/test_analysis.cpp
    tests/test_functions.cpp
    tests/test_golden.cpp
)
if(NOT MPL_BUILD_EXPERIMENTAL_AI)
    list(REMOVE_ITEM MPL_TEST_SOURCES
        tests/test_lab.cpp
        tests/test_repair.cpp
        tests/test_stats.cpp
    )
endif()
add_executable(mpl_tests ${MPL_TEST_SOURCES})
if(MPL_BUILD_EXPERIMENTAL_AI)
    target_link_libraries(mpl_tests PRIVATE mpl_demo mpl_core mpl_knowledge mpl_experiment)
else()
    target_link_libraries(mpl_tests PRIVATE mpl_demo mpl_core mpl_knowledge)
endif()
target_compile_definitions(mpl_tests PRIVATE
    MPL_GOLDEN_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/golden"
    MPL_TEST_OUTPUT_DIR="${CMAKE_CURRENT_BINARY_DIR}/test-output"
    MPL_EXAMPLES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/examples"
    MPL_BENCH_DIR="${CMAKE_CURRENT_SOURCE_DIR}/bench"
    MPL_KNOWLEDGE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/knowledge"
)
if(MPL_BUILD_EXPERIMENTAL_AI)
    target_compile_definitions(mpl_tests PRIVATE MPL_EXPERIMENTAL_AI=1)
endif()

add_test(NAME mpl_core_tests COMMAND mpl_tests)
set_tests_properties(mpl_core_tests PROPERTIES
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
add_test(NAME mpl_architecture_independence
    COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/verify_dependencies.cmake
)
