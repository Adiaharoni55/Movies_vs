cmake_minimum_required(VERSION 3.27)
project(FinalProject)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set OpenSSL root directory
set(OPENSSL_ROOT_DIR "${CMAKE_SOURCE_DIR}/openssl-3/x64")

# Set OpenSSL include and library directories explicitly
set(OPENSSL_INCLUDE_DIR "${OPENSSL_ROOT_DIR}/include")
set(OPENSSL_LIB_DIR "${OPENSSL_ROOT_DIR}/lib")

# Set GLFW directory
set(GLFW_DIR "${CMAKE_SOURCE_DIR}/glfw-3.4")

# ImGui Configuration
set(IMGUI_DIR "${CMAKE_SOURCE_DIR}/imgui-1.90.8/imgui-1.90.8")

# Check if GLFW directory exists
if(EXISTS ${GLFW_DIR})
    add_subdirectory(${GLFW_DIR})
else()
    message(WARNING "GLFW directory not found. Please make sure it's in the correct location or provide the correct path.")
endif()

# Find source files
file(GLOB_RECURSE SOURCE_FILES
        "${CMAKE_SOURCE_DIR}/src/*.cpp"
        "${CMAKE_SOURCE_DIR}/src/*.c"
)

# Find ImGui source files
file(GLOB IMGUI_SOURCE_FILES
        "${IMGUI_DIR}/*.cpp"
        "${IMGUI_DIR}/backends/imgui_impl_glfw.cpp"
        "${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp"
)

# Add executable
add_executable(FinalProject
        ${SOURCE_FILES}
        ${IMGUI_SOURCE_FILES}
        include/thread_safe_queue.h
)

# Include directories
target_include_directories(FinalProject PRIVATE
        ${OPENSSL_INCLUDE_DIR}
        ${IMGUI_DIR}
        ${IMGUI_DIR}/backends
        ${GLFW_DIR}/include
        ${CMAKE_SOURCE_DIR}/include
)

# Link libraries
target_link_libraries(FinalProject PRIVATE
        ${OPENSSL_LIB_DIR}/libssl.lib
        ${OPENSSL_LIB_DIR}/libcrypto.lib
        ws2_32
        crypt32
        bcrypt
        glfw
        opengl32
)

# Add compiler definitions
target_compile_definitions(FinalProject PRIVATE
        CPPHTTPLIB_OPENSSL_SUPPORT
)

# Copy OpenSSL DLLs to the output directory
add_custom_command(TARGET FinalProject POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${OPENSSL_ROOT_DIR}/bin/libssl-3-x64.dll"
        "${OPENSSL_ROOT_DIR}/bin/libcrypto-3-x64.dll"
        $<TARGET_FILE_DIR:FinalProject>
)

# Copy font files to the output directory
add_custom_command(TARGET FinalProject POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/imgui-1.90.8/imgui-1.90.8/misc/fonts"
        $<TARGET_FILE_DIR:FinalProject>/imgui-1.90.8/misc/fonts
)

# Copy images to the output directory
add_custom_command(TARGET FinalProject POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/images"
        $<TARGET_FILE_DIR:FinalProject>/images
)