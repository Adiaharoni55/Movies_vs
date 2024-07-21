cmake_minimum_required(VERSION 3.27)
project(FinalProject)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set OpenSSL root directory
set(OPENSSL_ROOT_DIR "C:/Users/user/CLionProjects/CPPProjects/FinalProject/openssl-3.3.1/openssl-3/x64")

# Set OpenSSL include and library directories explicitly
set(OPENSSL_INCLUDE_DIR "${OPENSSL_ROOT_DIR}/include")
set(OPENSSL_LIBRARIES "${OPENSSL_ROOT_DIR}/lib")

# Find OpenSSL package
find_package(OpenSSL REQUIRED)

# Set GLFW directory
set(GLFW_DIR "C:/Users/user/CLionProjects/CPPProjects/FinalProject/glfw-3.4/glfw-3.4")

# ImGui Configuration
set(IMGUI_DIR "C:/Users/user/CLionProjects/CPPProjects/FinalProject/imgui-1.90.8/imgui-1.90.8")

# Add GLFW as a subdirectory
add_subdirectory(${GLFW_DIR})

# Add executable
add_executable(FinalProject
        src/movies.cpp
        src/glad.c
        src/tinyxml2.cpp
        ${IMGUI_DIR}/imgui.cpp
        ${IMGUI_DIR}/imgui_demo.cpp
        ${IMGUI_DIR}/imgui_draw.cpp
        ${IMGUI_DIR}/imgui_tables.cpp
        ${IMGUI_DIR}/imgui_widgets.cpp
        ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
        ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
        include/thread_safe_queue.h
)

# Include directories
target_include_directories(FinalProject PRIVATE
        ${OPENSSL_INCLUDE_DIR}
        ${IMGUI_DIR}
        ${IMGUI_DIR}/backends
        ${GLFW_DIR}/include
        ${CMAKE_SOURCE_DIR}/include # Include directory for glad.h, khrplatform.h, stb_image.h, tinyxml2.h, httplib.h, and json.hpp
)

# Link libraries
target_link_libraries(FinalProject PRIVATE
        OpenSSL::SSL
        OpenSSL::Crypto
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
