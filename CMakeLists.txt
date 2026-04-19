cmake_minimum_required (VERSION 3.21)

project (RocmSdlApp LANGUAGES CXX HIP)

# Set C++ standard to C++14
set (CMAKE_CXX_STANDARD 14)
set (CMAKE_CXX_STANDARD_REQUIRED ON)

set (CMAKE_HIP_STANDARD 14)
set (CMAKE_HIP_STANDARD_REQUIRED ON)

# 1. ROCm/HIP Configuration
# RHEL 10 default path for ROCm 7.2.1 is /etc/alternatives/rocm
list (APPEND CMAKE_PREFIX_PATH /opt/rocm-7.2.1)
find_package (hip REQUIRED)
find_package (ROCmCMakeBuildTools REQUIRED)

# 2. SDL3 and OpenGL Configuration
# Install via: sudo dnf install SDL3-devel mesa-libGL-devel
find_package (SDL3 REQUIRED)
find_package (OpenGL REQUIRED)
find_package (SDL3_ttf REQUIRED)
find_package (GLEW REQUIRED)

if (NOT DEFINED CMAKE_HIP_ARCHITECTURES)
    set (CMAKE_HIP_ARCHITECTURES "native" CACHE STRING "HIP architectures")
endif()

set (GPU_ROOT "GPU")

# 3. Dear ImGui Setup
# ImGui is typically added as source files rather than a system library
set (IMGUI_DIR "$ENV{SRC_ROOT}/imgui")

set (SOURCE_ROOT ${CMAKE_SOURCE_DIR}/src)
set (SDL_HEADERS "/usr/local/include/SDL3")
set (GLM_HEADERS "$ENV{SRC_ROOT}/glm")

set (HEADER_FILES
    ${CMAKE_SOURCE_DIR}/include
    ${SDL_HEADERS}
    ${HIP_PATH}/include
    ${ACE_INCLUDE_DIR}
    ${IMGUI_DIR}
    ${GLM_HEADERS}
    ${IMGUI_DIR}/backends
)

file (GLOB IMGUI_SOURCES 
    "${IMGUI_DIR}/*.cpp"
    "${IMGUI_DIR}/backends/imgui_impl_sdl3.cpp"
    "${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp"
)

# 4. Define Executable
add_executable (RocmSdlApp 
    ${SOURCE_ROOT}/gui/main.cxx
    ${SOURCE_ROOT}/gui/AppConsole.cxx
    ${SOURCE_ROOT}/common/LoadShaders.cxx
##    gpu_kernel.hip  # Your kernel file
    ${SOURCE_ROOT}/globe/Continents.hip
    ${IMGUI_SOURCES}
)

# 5. Linking
target_link_libraries (RocmSdlApp PRIVATE 
    hip::host 
    hip::device
    SDL3::SDL3       # Standard SDL3 target
    OpenGL::GL       # OpenGL for UI rendering
    GLEW::GLEW
)

target_include_directories (RocmSdlApp PRIVATE ${HEADER_FILES})
