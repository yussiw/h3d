cmake_minimum_required(VERSION 3.18)
project(h3d C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Dear ImGui e stb_image nao tem CMakeLists; apenas baixamos o codigo-fonte.
include(FetchContent)
FetchContent_Declare(imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.90.9
    GIT_SHALLOW TRUE)
FetchContent_Declare(stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG master
    GIT_SHALLOW TRUE)
FetchContent_Declare(lua
    GIT_REPOSITORY https://github.com/lua/lua.git
    GIT_TAG v5.4.6
    GIT_SHALLOW TRUE)
FetchContent_Declare(miniaudio
    GIT_REPOSITORY https://github.com/mackron/miniaudio.git
    GIT_TAG 0.11.21
    GIT_SHALLOW TRUE)
FetchContent_GetProperties(imgui)
if(NOT imgui_POPULATED)
    FetchContent_Populate(imgui)
endif()
FetchContent_GetProperties(stb)
if(NOT stb_POPULATED)
    FetchContent_Populate(stb)
endif()

FetchContent_GetProperties(lua)
if(NOT lua_POPULATED)
    FetchContent_Populate(lua)
endif()

FetchContent_GetProperties(miniaudio)
if(NOT miniaudio_POPULATED)
    FetchContent_Populate(miniaudio)
endif()

# Lua 5.4: compila todos os .c da biblioteca (menos o interpretador de linha de comando)
file(GLOB LUA_SOURCES "${lua_SOURCE_DIR}/*.c")
list(FILTER LUA_SOURCES EXCLUDE REGEX ".*/(lua|luac|onelua|ltests)\\.c$")
add_library(lua_lib STATIC ${LUA_SOURCES})
target_include_directories(lua_lib PUBLIC ${lua_SOURCE_DIR})

if(DEFINED CMAKE_ANDROID_NDK)
    set(H3D_NDK_ROOT "${CMAKE_ANDROID_NDK}")
elseif(DEFINED ANDROID_NDK)
    set(H3D_NDK_ROOT "${ANDROID_NDK}")
else()
    message(FATAL_ERROR "Caminho do Android NDK nao foi definido pelo Android Gradle Plugin")
endif()
set(GLUE_DIR "${H3D_NDK_ROOT}/sources/android/native_app_glue")
if(NOT EXISTS "${GLUE_DIR}/android_native_app_glue.c")
    message(FATAL_ERROR "android_native_app_glue.c nao encontrado em ${GLUE_DIR}")
endif()
add_library(native_app_glue STATIC ${GLUE_DIR}/android_native_app_glue.c)
target_include_directories(native_app_glue PUBLIC ${GLUE_DIR})

add_library(h3d SHARED
    main.cpp
    audio.cpp
    audio_impl.c
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_android.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp)

target_include_directories(h3d PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
    ${stb_SOURCE_DIR}
    ${lua_SOURCE_DIR}
    ${miniaudio_SOURCE_DIR})

target_compile_definitions(h3d PRIVATE IMGUI_IMPL_OPENGL_ES3 MA_NO_ENCODING MA_NO_GENERATION)

set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -u ANativeActivity_onCreate -Wl,-z,max-page-size=16384")

target_link_libraries(h3d native_app_glue lua_lib android log EGL GLESv3 dl m)
