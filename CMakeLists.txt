cmake_minimum_required(VERSION 3.22)
project(mtasa_android VERSION 0.1.0 LANGUAGES C CXX)

# ---------------------------------------------------------------------------
# Opções
# ---------------------------------------------------------------------------
# Backend de inline hook: and64 (padrão, só arm64) ou none (stub p/ host).
set(MTASA_HOOK_BACKEND "auto" CACHE STRING "Backend de hook: auto|and64|none")
set_property(CACHE MTASA_HOOK_BACKEND PROPERTY STRINGS auto and64 none)
option(MTASA_WITH_RAKNET "Compila o transporte RakNet (experimental)" OFF)
option(MTASA_BUILD_TESTS "Compila os testes de host" OFF)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_C_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(ANDROID AND NOT ANDROID_ABI STREQUAL "arm64-v8a")
  message(FATAL_ERROR "Somente arm64-v8a é suportado (ANDROID_ABI=${ANDROID_ABI}).")
endif()

if(MTASA_HOOK_BACKEND STREQUAL "auto")
  if(ANDROID)
    set(MTASA_HOOK_BACKEND and64)
  else()
    set(MTASA_HOOK_BACKEND none)
  endif()
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(Dependencies)

# ---------------------------------------------------------------------------
# Payload nativo: libmtasa.so
# ---------------------------------------------------------------------------
add_library(mtasa SHARED
  src/core/main.cpp
  src/core/Log.cpp
  src/hook/Hook.cpp
  src/hook/ModuleInfo.cpp
  src/game/Symbols.cpp
  src/game/GameHooks.cpp
  src/gfx/OverlayRenderer.cpp
  src/lua/LuaAllocator.cpp
  src/net/NetTransport.cpp
)

target_include_directories(mtasa PRIVATE include src)
target_compile_options(mtasa PRIVATE
  -Wall -Wextra -Wno-unused-parameter
  -fno-exceptions -fno-rtti
  -ffunction-sections -fdata-sections)
target_compile_definitions(mtasa PRIVATE
  MTASA_VERSION="${PROJECT_VERSION}"
  MTASA_HOOK_BACKEND_NAME="${MTASA_HOOK_BACKEND}")

target_link_libraries(mtasa PRIVATE mtasa::hook_backend ${CMAKE_DL_LIBS})

if(MTASA_WITH_RAKNET)
  target_sources(mtasa PRIVATE src/net/RakNetTransport.cpp)
  target_compile_definitions(mtasa PRIVATE MTASA_WITH_RAKNET=1)
  target_link_libraries(mtasa PRIVATE mtasa::raknet)
endif()

if(ANDROID)
  target_link_libraries(mtasa PRIVATE log android)
  target_link_options(mtasa PRIVATE
    -Wl,--gc-sections
    -Wl,--exclude-libs,ALL            # não reexporta símbolos das libs estáticas
    -Wl,-z,max-page-size=16384        # dispositivos com página de 16 KB (Android 15+)
    -Wl,--build-id=sha1)
else()
  find_package(Threads REQUIRED)
  target_link_libraries(mtasa PRIVATE Threads::Threads)
endif()

# ---------------------------------------------------------------------------
# Testes de host (lógica independente de plataforma)
# ---------------------------------------------------------------------------
if(MTASA_BUILD_TESTS AND NOT ANDROID)
  enable_testing()
  add_executable(mtasa_host_tests tests/host_tests.cpp src/lua/LuaAllocator.cpp src/net/NetTransport.cpp src/core/Log.cpp)
  target_include_directories(mtasa_host_tests PRIVATE include src)
  add_test(NAME host_tests COMMAND mtasa_host_tests)
endif()

message(STATUS "mtasa: hook backend = ${MTASA_HOOK_BACKEND}, raknet = ${MTASA_WITH_RAKNET}")
