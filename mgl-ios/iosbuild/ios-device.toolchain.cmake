# iOS device arm64 toolchain for MGL/glslang/SPIRV-Tools/SPIRV-Cross
# Non-Xcode generators: emulate Xcode's -sdk iphoneos behavior.
set(CMAKE_SYSTEM_NAME Darwin)
set(CMAKE_SYSTEM_PROCESSOR arm64)

execute_process(COMMAND xcrun --sdk iphoneos --show-sdk-path
                OUTPUT_VARIABLE IOS_SDK
                OUTPUT_STRIP_TRAILING_WHITESPACE)
message(STATUS "ios-device SDK: ${IOS_SDK}")

set(CMAKE_OSX_SYSROOT "${IOS_SDK}" CACHE STRING "" FORCE)
set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "" FORCE)
set(CMAKE_OSX_DEPLOYMENT_TARGET "16.0" CACHE STRING "" FORCE)

set(CMAKE_C_COMPILER clang)
set(CMAKE_CXX_COMPILER clang++)
set(CMAKE_OBJC_COMPILER clang)

add_compile_options(-arch arm64 -isysroot "${IOS_SDK}" -miphoneos-version-min=16.0)
add_link_options(-arch arm64 -isysroot "${IOS_SDK}" -miphoneos-version-min=16.0)

set(CMAKE_FIND_ROOT_PATH "${IOS_SDK}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
