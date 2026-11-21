cmake_minimum_required(VERSION 3.28...3.30)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/common/bootstrap.cmake" NO_POLICY_SCOPE)

project(${_name} VERSION ${_version})

option(ENABLE_FRONTEND_API "Use obs-frontend-api for UI functionality" ON)
option(ENABLE_QT "Use Qt functionality" ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(compilerconfig)
include(defaults)
include(helpers)

add_library(${CMAKE_PROJECT_NAME} MODULE)

find_package(libobs REQUIRED)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::libobs)

if(WIN32)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE user32)
endif()

if(ENABLE_FRONTEND_API)
  find_package(obs-frontend-api REQUIRED)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::obs-frontend-api)
endif()

if(ENABLE_QT)
  find_package(Qt6 COMPONENTS Widgets Core)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE Qt6::Core Qt6::Widgets)
  target_compile_options(
    ${CMAKE_PROJECT_NAME}
    PRIVATE $<$<C_COMPILER_ID:Clang,AppleClang>:-Wno-quoted-include-in-framework-header -Wno-comma>
  )
  set_target_properties(
    ${CMAKE_PROJECT_NAME}
    PROPERTIES AUTOMOC ON AUTOUIC ON AUTORCC ON
  )
endif()

target_sources(${CMAKE_PROJECT_NAME} PRIVATE src/plugin-main.cpp)

set_target_properties_plugin(${CMAKE_PROJECT_NAME} PROPERTIES OUTPUT_NAME ${_name})

# Nadpisanie struktury paczki dla Windows: obs-plugins/64bit oraz data/obs-plugins/<nazwa>
if(WIN32)
  set_property(GLOBAL PROPERTY OBS_PLUGIN_INSTALL_PREFIX "")
  install(
    TARGETS ${CMAKE_PROJECT_NAME}
    RUNTIME DESTINATION "obs-plugins/64bit" COMPONENT Runtime
    LIBRARY DESTINATION "obs-plugins/64bit" COMPONENT Runtime
  )
  install(
    DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/data/"
    DESTINATION "data/obs-plugins/${_name}"
    COMPONENT Runtime
  )
endif()