cmake_minimum_required(VERSION 3.25)

project(ClipForge
    VERSION 0.1.0
    DESCRIPTION "Offline audio inspection and batch-processing toolkit"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(CLIPFORGE_ENABLE_ASM   "Enable NASM x86-64 SSE2 backend when available" ON)
option(CLIPFORGE_BUILD_CLI    "Build clipforge-cli executable" ON)
option(CLIPFORGE_BUILD_TESTS  "Build CTest unit and differential test suite" ON)
option(CLIPFORGE_BUILD_BENCH  "Build clipforge-bench benchmark executable" ON)
option(CLIPFORGE_BUILD_QT_UI  "Placeholder option for future Phase 2 Qt 6 Quick UI target" OFF)

set(CLIPFORGE_HAS_NASM_X64 0)
if(CLIPFORGE_ENABLE_ASM AND WIN32 AND CMAKE_SIZEOF_VOID_P EQUAL 8)
    include(CheckLanguage)
    check_language(ASM_NASM)
    if(CMAKE_ASM_NASM_COMPILER)
        enable_language(ASM_NASM)
        set(CLIPFORGE_HAS_NASM_X64 1)
        message(STATUS "ClipForge: NASM x86-64 enabled (${CMAKE_ASM_NASM_COMPILER})")
    else()
        message(STATUS "ClipForge: NASM not found; falling back cleanly to C++23 DSP backend.")
    endif()
else()
    message(STATUS "ClipForge: NASM x86-64 backend not active for this platform/configuration; using C++23 DSP backend.")
endif()

set(CLIPFORGE_CORE_SOURCES
    src/dsp/cpp/kernels_cpp.cpp
    src/dsp/dispatch.cpp
    src/audio/wav_reader.cpp
    src/audio/wav_writer.cpp
    src/processing/analyzer.cpp
    src/processing/normalizer.cpp
    src/processing/silence_trimmer.cpp
    src/services/engine_facade.cpp
)

if(CLIPFORGE_HAS_NASM_X64)
    list(APPEND CLIPFORGE_CORE_SOURCES src/dsp/asm/win64/kernels_sse2.asm)
endif()

add_library(clipforge_core STATIC ${CLIPFORGE_CORE_SOURCES})
target_include_directories(clipforge_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)
target_compile_definitions(clipforge_core
    PUBLIC
        CLIPFORGE_HAS_NASM_X64=${CLIPFORGE_HAS_NASM_X64}
)

if(MSVC)
    # Scope MSVC flags to C++ only: they are not valid NASM options and
    # CMake would otherwise forward them to the ASM_NASM compile line.
    target_compile_options(clipforge_core PRIVATE $<$<COMPILE_LANGUAGE:CXX>:/W4 /permissive- /EHsc>)
else()
    target_compile_options(clipforge_core PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-Wall -Wextra -Wpedantic>)
endif()

if(CLIPFORGE_BUILD_CLI)
    add_executable(clipforge-cli apps/cli/main.cpp)
    target_link_libraries(clipforge-cli PRIVATE clipforge_core)
endif()

if(CLIPFORGE_BUILD_BENCH)
    add_executable(clipforge-bench benchmarks/main_bench.cpp)
    target_link_libraries(clipforge-bench PRIVATE clipforge_core)
endif()

if(CLIPFORGE_BUILD_TESTS)
    enable_testing()
    add_executable(clipforge-tests
        tests/test_main.cpp
        tests/audio/test_wav_io.cpp
        tests/processing/test_processing.cpp
        tests/dsp/test_dsp_differential.cpp
        tests/services/test_batch.cpp
    )
    target_link_libraries(clipforge-tests PRIVATE clipforge_core)

    add_test(NAME clipforge.wav_io           COMMAND clipforge-tests wav)
    add_test(NAME clipforge.processing       COMMAND clipforge-tests processing)
    add_test(NAME clipforge.dsp_differential COMMAND clipforge-tests dsp)
    add_test(NAME clipforge.batch            COMMAND clipforge-tests batch)
endif()

if(CLIPFORGE_BUILD_QT_UI)
    find_package(Qt6 6.8 COMPONENTS Quick QuickTest Test Gui Concurrent REQUIRED)
    qt_standard_project_setup(REQUIRES 6.5)

    add_library(clipforge_qt STATIC
        apps/qt/BatchQueueModel.cpp
        apps/qt/EngineBridge.cpp
    )
    target_include_directories(clipforge_qt PUBLIC apps/qt)
    target_link_libraries(clipforge_qt PUBLIC Qt6::Core Qt6::Concurrent clipforge_core)

    qt_add_executable(clipforge-ui apps/qt/main.cpp)
    target_link_libraries(clipforge-ui PRIVATE Qt6::Gui Qt6::Quick clipforge_qt clipforge_qtplugin clipforge_core)

    # QML lives in the static lib so both the app and the smoke test link it.
    qt_add_qml_module(clipforge_qt
        URI ClipForge
        VERSION 1.0
        QML_FILES
            apps/qt/qml/Main.qml
            apps/qt/qml/Theme.qml
        RESOURCES
            assets/clipforge-logo.svg
    )

    add_executable(tst_bridge apps/qt/tests/tst_bridge.cpp)
    target_link_libraries(tst_bridge PRIVATE Qt6::Test Qt6::Core clipforge_qt clipforge_core)
    add_test(NAME clipforge.qt_bridge COMMAND tst_bridge)

    add_executable(tst_smoke apps/qt/tests/tst_smoke.cpp)
    target_link_libraries(tst_smoke PRIVATE Qt6::Test Qt6::Gui Qt6::Qml Qt6::Quick clipforge_qt clipforge_qtplugin clipforge_core)
    add_test(NAME clipforge.qt_smoke COMMAND tst_smoke)

    message(STATUS "ClipForge: Qt 6 Quick UI enabled.")
endif()
