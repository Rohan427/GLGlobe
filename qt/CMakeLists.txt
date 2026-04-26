cmake_minimum_required(VERSION 3.16)
project (LegacyGLApp VERSION 1.0 LANGUAGES CXX)

# Force standard C++ for Qt6
set (CMAKE_CXX_STANDARD 17)
set (CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable Qt's automatic meta-object, UI, and resource compilers
set (CMAKE_AUTOMOC ON)
set (CMAKE_AUTOUIC ON)
set (CMAKE_AUTORCC ON)

# 1. Find the necessary Qt6 components
# In Qt6, QOpenGLWidget was moved to its own 'OpenGLWidgets' module
find_package (Qt6 REQUIRED COMPONENTS Widgets OpenGL OpenGLWidgets)

# 2. Find the system OpenGL library (for legacy gl* symbols)
# RHEL 10 uses GLVND by default; setting this to LEGACY ensures 
# it links against libGL.so for legacy compatibility.
set (OpenGL_GL_PREFERENCE LEGACY)
find_package (OpenGL REQUIRED)

set (SOURCE_ROOT ${CMAKE_SOURCE_DIR}/src)
set (HEADER_ROOT ${CMAKE_SOURCE_DIR}/include)

set (HEADER_FILES
    ${HEADER_ROOT}
    "$ENV{SPG4_HEADERS}"
)

# Define your source files
set (SOURCES
    ${SOURCE_ROOT}/Main.cxx
    ${SOURCE_ROOT}/MainWindow.cxx
    ${SOURCE_ROOT}/MyGLWidget.cxx
)

set (HEADERS
    "$ENV{SPG4_HEADERS}/SGP4.h"
    "$ENV{SPG4_HEADERS}/Tle.h"
    ${HEADER_ROOT}/MainWindow.hxx
    ${HEADER_ROOT}/MyGLWidget.hxx
    ${HEADER_ROOT}/LegacyGLApp.hxx
)

# Define output folders
set (CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/../bin)
set (CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/../lib)
set (CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/../lib)

# Create the executable
add_executable (LegacyGLApp ${SOURCES} ${HEADERS})

# 3. Link everything together
# You must link BOTH the Qt modules and the raw OpenGL target
target_link_libraries (LegacyGLApp PRIVATE 
    Qt6::Widgets 
    Qt6::OpenGL 
    Qt6::OpenGLWidgets
    OpenGL::GL
    "$ENV{SPG4_LIB}/libsgp4s.so"
)

target_include_directories (LegacyGLApp PRIVATE ${HEADER_FILES})

add_custom_command (TARGET LegacyGLApp POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${SOURCE_ROOT}/globe/shaders"
            "$<TARGET_FILE_DIR:LegacyGLApp>/shaders"
)

add_custom_command (TARGET LegacyGLApp POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${SOURCE_ROOT}/objects/shaders/Satellite.vert"
            "$<TARGET_FILE_DIR:LegacyGLApp>/shaders"
)

add_custom_command(TARGET LegacyGLApp POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "run"
            "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/run"
)

add_custom_command(TARGET LegacyGLApp POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${SOURCE_ROOT}/../../textures"
            "$<TARGET_FILE_DIR:LegacyGLApp>/textures"
)
