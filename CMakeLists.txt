# CMakeLists.txt --- CMake project settings
#    ex) cmake -G "Visual Studio 9 2008" .
#    ex) cmake -DCMAKE_BUILD_TYPE=Release -G "MSYS Makefiles" .
##############################################################################

# CMake minimum version
cmake_minimum_required(VERSION 3.0)

# project name and languages
project(CBTHooker CXX RC)

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    # using Clang
    set(CMAKE_C_FLAGS "-static")
    set(CMAKE_CXX_FLAGS "-static")
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    # using GCC
    set(CMAKE_C_FLAGS "-static")
    set(CMAKE_CXX_FLAGS "-static")
elseif (MSVC)
    # replace "/MD" with "/MT" (building without runtime DLLs)
    set(CompilerFlags
        CMAKE_C_FLAGS
        CMAKE_C_FLAGS_DEBUG
        CMAKE_C_FLAGS_RELEASE
        CMAKE_C_FLAGS_RELWITHDEBINFO
        CMAKE_CXX_FLAGS
        CMAKE_CXX_FLAGS_DEBUG
        CMAKE_CXX_FLAGS_RELEASE
        CMAKE_CXX_FLAGS_RELWITHDEBINFO)
    foreach(CompilerFlags ${CompilerFlags})
        string(REPLACE "/MD" "/MT" ${CompilerFlags} "${${CompilerFlags}}")
    endforeach()
endif()

##############################################################################

# Unicode support
add_definitions(-DUNICODE -D_UNICODE)

# dll32.dll and dll64.dll
add_library(dll SHARED dll/dll.cpp dll/dll.def)
target_link_libraries(dll PRIVATE)
set_target_properties(dll PROPERTIES PREFIX "")
target_compile_definitions(dll PRIVATE -DCBTHOOK_DLL)

# watcher32.exe and watcher64.exe
add_executable(watcher watcher/watcher.cpp)
target_link_libraries(watcher PRIVATE dll)
target_compile_definitions(watcher PRIVATE)

# 32-bit or 64-bit?
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set_target_properties(dll PROPERTIES OUTPUT_NAME "dll64")
    set_target_properties(watcher PROPERTIES OUTPUT_NAME "watcher64")
elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
    set_target_properties(dll PROPERTIES OUTPUT_NAME "dll32")
    set_target_properties(watcher PROPERTIES OUTPUT_NAME "watcher32")

    # CBTHooker.exe
    add_executable(CBTHooker WIN32 CBTHooker/CBTHooker.cpp CBTHooker/CBTHooker_res.rc)
    target_link_libraries(CBTHooker PRIVATE comctl32 shlwapi)
else()
    message(FATAL_ERROR "You lose.")
endif()

##############################################################################
