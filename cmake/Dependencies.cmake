# Dependências externas, fixadas por commit para builds reproduzíveis.
include(FetchContent)
set(FETCHCONTENT_QUIET ON)

# ---------------------------------------------------------------------------
# Backend de hook -> alvo mtasa::hook_backend
# ---------------------------------------------------------------------------
add_library(mtasa_hook_backend INTERFACE)
add_library(mtasa::hook_backend ALIAS mtasa_hook_backend)

if(MTASA_HOOK_BACKEND STREQUAL "and64")
  if(NOT CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
    message(FATAL_ERROR "And64InlineHook só funciona em arm64.")
  endif()
  FetchContent_Declare(and64inlinehook
    GIT_REPOSITORY https://github.com/Rprop/And64InlineHook.git
    GIT_TAG        2787585191552b291c508e7ae639557164441a13)
  FetchContent_MakeAvailable(and64inlinehook)   # sem CMakeLists: só baixa
  add_library(and64inlinehook STATIC ${and64inlinehook_SOURCE_DIR}/And64InlineHook.cpp)
  target_include_directories(and64inlinehook PUBLIC ${and64inlinehook_SOURCE_DIR})
  target_compile_options(and64inlinehook PRIVATE -w)
  target_link_libraries(mtasa_hook_backend INTERFACE and64inlinehook)
  target_compile_definitions(mtasa_hook_backend INTERFACE MTASA_HOOK_AND64=1)

elseif(MTASA_HOOK_BACKEND STREQUAL "dobby")
  set(DOBBY_BUILD_EXAMPLE OFF CACHE BOOL "" FORCE)
  set(DOBBY_BUILD_TEST    OFF CACHE BOOL "" FORCE)
  set(DOBBY_DEBUG         OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(dobby
    GIT_REPOSITORY https://github.com/jmpews/Dobby.git
    GIT_TAG        5dfc8546954ce3b3198132ab13fddb89ee92cdd7)
  FetchContent_MakeAvailable(dobby)
  target_link_libraries(mtasa_hook_backend INTERFACE dobby_static)
  target_include_directories(mtasa_hook_backend INTERFACE ${dobby_SOURCE_DIR}/include)
  target_compile_definitions(mtasa_hook_backend INTERFACE MTASA_HOOK_DOBBY=1)

elseif(MTASA_HOOK_BACKEND STREQUAL "none")
  target_compile_definitions(mtasa_hook_backend INTERFACE MTASA_HOOK_NONE=1)
else()
  message(FATAL_ERROR "MTASA_HOOK_BACKEND inválido: ${MTASA_HOOK_BACKEND}")
endif()

# ---------------------------------------------------------------------------
# RakNet (opcional) -> alvo mtasa::raknet
# O CMake do upstream é antigo (2.6) e compila samples; montamos o alvo à mão.
# ---------------------------------------------------------------------------
if(MTASA_WITH_RAKNET)
  FetchContent_Declare(raknet
    GIT_REPOSITORY https://github.com/facebookarchive/RakNet.git
    GIT_TAG        1a169895a900c9fc4841c556e16514182b75faf8
    SOURCE_SUBDIR  _no_upstream_cmake_)          # baixa sem add_subdirectory
  FetchContent_MakeAvailable(raknet)
  file(GLOB RAKNET_SOURCES ${raknet_SOURCE_DIR}/Source/*.cpp)
  # ReplicaManager3 não compila em clang moderno e não é usado.
  list(FILTER RAKNET_SOURCES EXCLUDE REGEX "ReplicaManager3\\.cpp$")
  add_library(raknet STATIC ${RAKNET_SOURCES})
  add_library(mtasa::raknet ALIAS raknet)
  target_include_directories(raknet SYSTEM PUBLIC ${raknet_SOURCE_DIR}/Source)
  target_compile_options(raknet PRIVATE -w)
  set_target_properties(raknet PROPERTIES CXX_STANDARD 11 CXX_VISIBILITY_PRESET hidden)
endif()
