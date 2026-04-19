cmake_minimum_required(VERSION 3.22)
project(AI_Kernel_Gen_Env LANGUAGES CXX)

# 1. HIP/ROCm Discovery
list(APPEND CMAKE_PREFIX_PATH /opt/rocm)
find_package(hip REQUIRED)

# 2. SDL3 & OpenGL Discovery
find_package(SDL3 REQUIRED)
find_package(OpenGL REQUIRED)

# 3. Executable & Source Setup
add_executable(KernelApp main.cpp)

# 4. Linkage
target_link_libraries(KernelApp PRIVATE 
    hip::host 
    SDL3::SDL3 
    OpenGL::GL
)

# 5. Kernel Compilation (HIP-specific)
# For RX 7800 XT (gfx1101)
set_target_properties(KernelApp PROPERTIES 
    HIP_ARCHITECTURES "gfx1101"
)

# 6. Include Paths (ImGui & Understand API)
# Assuming ImGui is a subdirectory or managed via fetch_content
target_include_directories(KernelApp PRIVATE 
    ${OPENGL_INCLUDE_DIR}
    "libs/imgui"
    "libs/understand/api/include"
)

