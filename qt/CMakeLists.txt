cmake_minimum_required (VERSION 3.16)

project (LegacyGLApp VERSION 1.2 LANGUAGES CXX C HIP)

list (APPEND CMAKE_PREFIX_PATH ${HIP_PATH})

set (CMAKE_AUTOMOC ON)

################## Find HIP ##################
find_package (hip REQUIRED)

if (NOT DEFINED CMAKE_HIP_ARCHITECTURES)
    set (CMAKE_HIP_ARCHITECTURES "native" CACHE STRING "HIP architectures")
endif()


################ Find shder compiler ##############
# 1. Locate the native RHEL 10 Vulkan/OpenGL shader compiler tool
find_program (GLSLANG_VALIDATOR glslangValidator REQUIRED)


################## Find ACE (and TAO if we need it) ##################
# ACE/TAO typically don't ship with CMake 'config' files. 
# We manually find headers and libraries based on ACE_ROOT/TAO_ROOT.

#    find_path (TAO_INCLUDE_DIR tao/ORB.h PATHS "$ENV{TAO_ROOT}" NO_DEFAULT_PATH)
#    find_library (TAO_LIBRARY NAMES TAO PATHS "$ENV{TAO_ROOT}/lib" NO_DEFAULT_PATH)

find_path (ACE_INCLUDE_DIR ace/ACE.h PATHS "$ENV{ACE_ROOT}/include" NO_DEFAULT_PATH)
find_library (ACE_LIBRARY NAMES libACE.so PATHS "$ENV{ACE_ROOT}/lib" NO_DEFAULT_PATH)

#    find_path (TAO_INCLUDE_DIR tao/ORB.h PATHS ${TAO_ROOT} NO_DEFAULT_PATH)
#    find_library (TAO_LIBRARY NAMES TAO PATHS ${TAO_ROOT}/lib NO_DEFAULT_PATH)


if (NOT ACE_INCLUDE_DIR) # OR NOT TAO_LIBRARY)
    message (FATAL_ERROR "ACE include not found. Please set ACE_ROOT environment variable.")
endif()

#if (NOT TAO_INCLUDE_DIR) # OR NOT TAO_LIBRARY)
#    message (FATAL_ERROR "TAO include not found. Please set TAO_ROOT environment variable.")
#endif()

if (NOT ACE_LIBRARY) # OR NOT TAO_LIBRARY)
    message (FATAL_ERROR "ACE library not found. Please set ACE_ROOT environment variable.")
endif()


################## Find QT6 and OpenGL ##################
# In Qt6, QOpenGLWidget was moved to its own 'OpenGLWidgets' module
find_package (Qt6 REQUIRED COMPONENTS Widgets OpenGL OpenGLWidgets Network)

# 2. Find the system OpenGL library (for legacy gl* symbols)
# RHEL 10 uses GLVND by default; setting this to LEGACY ensures 
# it links against libGL.so for legacy compatibility.
##set (OpenGL_GL_PREFERENCE LEGACY)
set (OpenGL_GL_PREFERENCE GLVND)
find_package (OpenGL REQUIRED)

# Force standard C++ for Qt6
set (CMAKE_CXX_STANDARD 17)
set (CMAKE_CXX_STANDARD_REQUIRED ON)

# Define shader paths
set (SHADER_SRC_DIR_1 "${CMAKE_SOURCE_DIR}/src/Globe/shaders")
set (SHADER_SRC_DIR_2 "${CMAKE_SOURCE_DIR}/src/Objects/shaders")
set (SHADER_BIN_DIR "${CMAKE_CURRENT_BINARY_DIR}/../../bin/shaders")


# Enable Qt's automatic meta-object, UI, and resource compilers
set (CMAKE_AUTOMOC ON)
set (CMAKE_AUTOUIC ON)
set (CMAKE_AUTORCC ON)

set (SOURCE_ROOT ${CMAKE_SOURCE_DIR}/src)
set (HEADER_ROOT ${CMAKE_SOURCE_DIR}/include)

# Set GL extensions
set (GLAD_INCLUDE "$ENV{SRC_ROOT}/glad/include")
set (GLAD_SRC "$ENV{SRC_ROOT}/glad/src")

set (HEADER_FILES
    ${HEADER_ROOT}
    "$ENV{SPG4_HEADERS}"
    "$ENV{XTRA_INCLUDE}/rl/"
    ${GLAD_INCLUDE}
)

# Define your source files
set (SOURCES
    ${SOURCE_ROOT}/SimCore/Utility.cxx
    ${SOURCE_ROOT}/SimCore/Config.cxx
    ${SOURCE_ROOT}/SimCore/Main.cxx
    ${SOURCE_ROOT}/SimCore/MainWindow.cxx
    ${SOURCE_ROOT}/SimCore/MyGLWidget.cxx
    ${SOURCE_ROOT}/Space/Satellite.cxx
    ${SOURCE_ROOT}/SimCore/EntityManager.cxx
    ${SOURCE_ROOT}/Network/CelesTrakSource.cxx
    ${SOURCE_ROOT}/Globe/FontManager.cxx
    ${SOURCE_ROOT}/Objects/GuidedMissile.cxx
)

set (HEADERS
    "$ENV{SPG4_HEADERS}/SGP4.h"
    "$ENV{SPG4_HEADERS}/Tle.h"
    ${GLAD_INCLUDE}/glad/gl.h
    ${HEADER_ROOT}/Tracking.hxx
    ${HEADER_ROOT}/GuidedMissile.hxx
    ${HEADER_ROOT}/MainWindow.hxx
    ${HEADER_ROOT}/MyGLWidget.hxx
    ${HEADER_ROOT}/LegacyGLApp.hxx
    ${HEADER_ROOT}/BaseEntity.hxx
    ${HEADER_ROOT}/EntityManager.hxx
    ${HEADER_ROOT}/Utility.hxx
    ${HEADER_ROOT}/CelesTrakSource.hxx
    ${HEADER_ROOT}/Globe.hxx
    ${HEADER_ROOT}/Config.hxx
    ${HEADER_ROOT}/BaseDataSource.hxx
    ${HEADER_ROOT}/FontManager.hxx
    ${HEADER_ROOT}/SystemCapabilities.hxx
    ${HEADER_ROOT}/DataObjects.hxx
    "$ENV{XTRA_INCLUDE}/rl/Json.hxx"
    "$ENV{XTRA_INCLUDE}/rl/Configuration.hxx"
)

# Define output folders
set (CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/../bin)
set (CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/../lib)
set (CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/../lib)

# Ensure ACE macros expand to real locks
add_definitions (-DACE_HAS_THREADS -DACE_MT_SAFE=1)

include_directories (
    ${HEADER_ROOT}
    ${GLAD_INCLUDE}
)

######################## Build shader files ##########################

message (STATUS "--------------------------------------------------------")
message (STATUS "SHADER DIAGNOSTIC START")
message (STATUS "Target Source Directory 1 Path: ${SHADER_SRC_DIR_1}")
message (STATUS "Target Source Directory 2 Path: ${SHADER_SRC_DIR_2}")
message (STATUS "Target Output Directory Path: ${SHADER_BIN_DIR}")

# Create the output shader directory in your build folder at configuration time
file (MAKE_DIRECTORY "${SHADER_BIN_DIR}")

# DYNAMIC DISCOVERY: Find all .vert, .frag, and .comp files in the folder
file (GLOB SHADER_SOURCES 
      "${SHADER_SRC_DIR_1}/*.vert"
      "${SHADER_SRC_DIR_1}/*.frag"
      "${SHADER_SRC_DIR_1}/*.comp"
      "${SHADER_SRC_DIR_2}/*.vert"
      "${SHADER_SRC_DIR_2}/*.frag"
      "${SHADER_SRC_DIR_2}/*.comp"
)

# 3. Print out exactly how many files CMake located
list (LENGTH SHADER_SOURCES TOTAL_SHADERS_FOUND)
message (STATUS "Total Shader Source Files Located: ${TOTAL_SHADERS_FOUND}")

if (TOTAL_SHADERS_FOUND EQUAL 0)
    message (WARNING "CRITICAL WARNING: No shader files found! Verify that your shader files reside inside the actual folder path: ${SHADER_SRC_DIR}")
endif()

# Initialize an empty list to track all compiled binary artifacts
set (SPIRV_BINARY_OUTPUTS "")

# ITERATION LOOP: Generate custom compilation rules for each discovered file
foreach (SHADER_SRC_FILE ${SHADER_SOURCES})
    # Extract the raw file name without the path directory (e.g., "globe.vert")
    get_filename_component (SHADER_NAME "${SHADER_SRC_FILE}" NAME)
    
    # Define the exact target destination file path format you requested (e.g., "globe.vert.spv")
    set (SHADER_SPV_FILE "${SHADER_BIN_DIR}/${SHADER_NAME}.spv")
    
    # Append this specific file rule output to our master list
    list (APPEND SPIRV_BINARY_OUTPUTS "${SHADER_SPV_FILE}")
    
    # Create the automated compilation target rule for this specific shader stage
    add_custom_command (
        OUTPUT "${SHADER_SPV_FILE}"
        COMMAND ${GLSLANG_VALIDATOR} -G "${SHADER_SRC_FILE}" -o "${SHADER_SPV_FILE}"
        DEPENDS "${SHADER_SRC_FILE}"
        COMMENT "Pre-compiling GLSL asset into SPIR-V bytecode: ${SHADER_NAME} -> ${SHADER_NAME}.spv"
    )
endforeach()

# This tells CMake that LegacyGLApp cannot compile until Compile_SPIRV_Shaders finishes.
# It forces glslangValidator to run first every time you build.
add_custom_target (Compile_SPIRV_Shaders DEPENDS ${SPIRV_BINARY_OUTPUTS})



# Creating a separate library target forces the compiler to build gl.c 
# independently, bypassing the Qt automoc scan entirely.
add_library (Glad2_Loader STATIC "${GLAD_SRC}/gl.c")
set_target_properties (Glad2_Loader PROPERTIES 
    AUTOMOC OFF
    AUTOUIC OFF
    AUTORCC OFF
)
set_source_files_properties ("${GLAD_SRC}/gl.c" PROPERTIES LANGUAGE C)


# Create the executable + shaders
add_executable (LegacyGLApp ${SOURCES} ${HEADERS} ${SPIRV_BINARY_OUTPUTS})

# 3. Link everything together
# You must link BOTH the Qt modules and the raw OpenGL target
target_link_libraries (LegacyGLApp PRIVATE 
    Glad2_Loader
    ${CMAKE_DL_LIBS} # Required under Linux for dynamic library driver symbol lookups
    Qt6::Widgets 
    Qt6::OpenGL 
    Qt6::OpenGLWidgets
    Qt6::Network
    OpenGL::GL
    "$ENV{SPG4_LIB}/libsgp4s.so"
    ${ACE_LIBRARY}
    "$ENV{XTRA_LIB}/rel/rl/librllibconfig.so"
)

target_include_directories (LegacyGLApp PRIVATE
                            ${HEADER_FILES}
                            ${ACE_INCLUDE_DIR}
)

target_link_options (LegacyGLApp PRIVATE -rdynamic)

add_custom_target (copy_assets ALL DEPENDS ${SHADER_DEST_FILES})
