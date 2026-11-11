cmake_minimum_required(VERSION 3.21)

# Évite /ZI (incompatible avec ASan) si CMake >= 3.25
if(POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>")
endif()

project(uwu LANGUAGES C)

option(UWU_WERROR   "Traiter les warnings comme des erreurs" OFF)
option(UWU_SANITIZE "Activer les sanitizers en Debug"        ON)
option(UWU_NODEBUG  "Définir NODEBUG en Release"             ON)
set(UWU_DISABLE_DEBUG "" CACHE STRING "Valeur de DISABLE_DEBUG (Release)")

file(GLOB_RECURSE UWU_SOURCES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/src/*.c")

add_executable(uwu ${UWU_SOURCES})
set_target_properties(uwu PROPERTIES
  OUTPUT_NAME "main.uwu"
  # $<1:...> empêche les générateurs multi-config d'ajouter Debug/ ou Release/
  RUNTIME_OUTPUT_DIRECTORY "$<1:${CMAKE_SOURCE_DIR}/bin>")

target_compile_definitions(uwu PRIVATE
  $<$<AND:$<CONFIG:Release>,$<BOOL:${UWU_NODEBUG}>>:NODEBUG>
  $<$<AND:$<CONFIG:Release>,$<BOOL:${UWU_DISABLE_DEBUG}>>:DISABLE_DEBUG=${UWU_DISABLE_DEBUG}>)

if(MSVC)
  target_compile_options(uwu PRIVATE /W4 /utf-8
    $<$<BOOL:${UWU_WERROR}>:/WX>
    $<$<CONFIG:Release>:/O2>)
  target_compile_definitions(uwu PRIVATE _CRT_SECURE_NO_WARNINGS)

  if(UWU_SANITIZE)
    # ASan est incompatible avec /RTC1 (activé par défaut en Debug)
    string(REPLACE "/RTC1" "" CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG}")
    target_compile_options(uwu PRIVATE $<$<CONFIG:Debug>:/fsanitize=address>)
  endif()
else()
  target_compile_options(uwu PRIVATE -Wall -Wextra -Wundef -Wpedantic
    $<$<BOOL:${UWU_WERROR}>:-Werror>
    $<$<CONFIG:Debug>:-g>
    $<$<CONFIG:Release>:-O3>)

  if(UWU_SANITIZE)
    target_compile_options(uwu PRIVATE $<$<CONFIG:Debug>:-fsanitize=address,undefined>)
    target_link_options(uwu PRIVATE $<$<CONFIG:Debug>:-fsanitize=address,undefined>)
  endif()
endif()