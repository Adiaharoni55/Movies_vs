cmake_minimum_required(VERSION 3.28)
project(final)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set the path to ImGui
set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/imgui)

# Add the include directory to the include path
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(${IMGUI_DIR})

# Add stb_image.h, json.hpp, http and openssl directory to include path
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include/stb)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include/json)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include/httplib)

# Platform-specific configurations
if(APPLE)
    set(OPENSSL_ROOT_DIR /opt/homebrew/opt/openssl@3)
    set(OPENSSL_LIBRARIES ${OPENSSL_ROOT_DIR}/lib)
    set(OPENSSL_INCLUDE_DIR ${OPENSSL_ROOT_DIR}/include)
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include/OpenSSL)
    set(GLFW_PREFIX "/opt/homebrew/opt/glfw")
elseif(WIN32)
    # Adjust these paths for your Windows setup
    set(OPENSSL_ROOT_DIR "C:/Program Files/OpenSSL-Win64")
    set(GLFW_PREFIX "C:/path/to/glfw")
endif()

# Add GLAD source to your project
set(GLAD_SRC ${CMAKE_CURRENT_SOURCE_DIR}/src/glad.c)

# Add ImGui source files
set(IMGUI_SRC
        ${IMGUI_DIR}/imgui.cpp
        ${IMGUI_DIR}/imgui_demo.cpp
        ${IMGUI_DIR}/imgui_draw.cpp
        ${IMGUI_DIR}/imgui_tables.cpp
        ${IMGUI_DIR}/imgui_widgets.cpp
        ${IMGUI_DIR}/imgui_impl_glfw.cpp
        ${IMGUI_DIR}/imgui_impl_opengl3.cpp
)

# Copy images folder to build directory
file(COPY ${CMAKE_SOURCE_DIR}/images DESTINATION ${CMAKE_BINARY_DIR})

# Find required packages
find_package(OpenGL REQUIRED)

# GLFW configuration
include_directories(${GLFW_PREFIX}/include)
link_directories(${GLFW_PREFIX}/lib)
find_library(GLFW_LIBRARY glfw HINTS ${GLFW_PREFIX}/lib)

if(NOT GLFW_LIBRARY)
    message(FATAL_ERROR "GLFW library not found. GLFW_PREFIX: ${GLFW_PREFIX}")
else()
    message(STATUS "Found GLFW: ${GLFW_LIBRARY}")
endif()

# OpenSSL configuration
if(WIN32)
    find_package(OpenSSL REQUIRED)
else()
    include_directories(${OPENSSL_INCLUDE_DIR})
endif()

# Add your executable
add_executable(final movies.cpp ${GLAD_SRC} ${IMGUI_SRC})

# Link against required libraries
target_link_libraries(final
        OpenGL::GL
        ${GLFW_LIBRARY}
)

if(APPLE)
    target_link_libraries(final
            "-framework Cocoa"
            "-framework IOKit"
            "-framework CoreVideo"
            ${OPENSSL_LIBRARIES}/libssl.dylib
            ${OPENSSL_LIBRARIES}/libcrypto.dylib
    )
elseif(WIN32)
    target_link_libraries(final
            OpenSSL::SSL
            OpenSSL::Crypto
    )
endif()

# Print architecture and other debug info
message(STATUS "CMAKE_SYSTEM_PROCESSOR: ${CMAKE_SYSTEM_PROCESSOR}")
message(STATUS "CMAKE_HOST_SYSTEM_PROCESSOR: ${CMAKE_HOST_SYSTEM_PROCESSOR}")
if(APPLE)
    message(STATUS "CMAKE_OSX_ARCHITECTURES: ${CMAKE_OSX_ARCHITECTURES}")
    execute_process(COMMAND uname -m OUTPUT_VARIABLE UNAME_M OUTPUT_STRIP_TRAILING_WHITESPACE)
    message(STATUS "uname -m: ${UNAME_M}")
endif()