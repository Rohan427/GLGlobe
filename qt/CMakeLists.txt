cmake_minimum_required(VERSION 3.16)

project (LegacyGLApp VERSION 1.2 LANGUAGES CXX HIP)

list (APPEND CMAKE_PREFIX_PATH ${HIP_PATH})

set (CMAKE_AUTOMOC ON)

################## Find HIP ##################
find_package (hip REQUIRED)

if (NOT DEFINED CMAKE_HIP_ARCHITECTURES)
    set (CMAKE_HIP_ARCHITECTURES "native" CACHE STRING "HIP architectures")
endif()




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
set (OpenGL_GL_PREFERENCE LEGACY)
find_package (OpenGL REQUIRED)

# Force standard C++ for Qt6
set (CMAKE_CXX_STANDARD 17)
set (CMAKE_CXX_STANDARD_REQUIRED ON)


# Enable Qt's automatic meta-object, UI, and resource compilers
set (CMAKE_AUTOMOC ON)
set (CMAKE_AUTOUIC ON)
set (CMAKE_AUTORCC ON)

set (SOURCE_ROOT ${CMAKE_SOURCE_DIR}/src)
set (HEADER_ROOT ${CMAKE_SOURCE_DIR}/include)

set (HEADER_FILES
    ${HEADER_ROOT}
    "$ENV{SPG4_HEADERS}"
    "$ENV{XTRA_INCLUDE}/rl/"
)

# Define your source files
set (SOURCES
    ${SOURCE_ROOT}/SimCore/Config.cxx
    ${SOURCE_ROOT}/SimCore/Main.cxx
    ${SOURCE_ROOT}/SimCore/MainWindow.cxx
    ${SOURCE_ROOT}/SimCore/MyGLWidget.cxx
    ${SOURCE_ROOT}/Space/Satellite.cxx
    ${SOURCE_ROOT}/SimCore/EntityManager.cxx
    ${SOURCE_ROOT}/Network/CelesTrakSource.cxx
    ${SOURCE_ROOT}/Globe/FontManager.cxx
)

set (HEADERS
    "$ENV{SPG4_HEADERS}/SGP4.h"
    "$ENV{SPG4_HEADERS}/Tle.h"
    ${HEADER_ROOT}/Tracking.hxx
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
    "$ENV{XTRA_INCLUDE}/rl/Json.hxx"
    "$ENV{XTRA_INCLUDE}/rl/Configuration.hxx"
)

# Define output folders
set (CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/../bin)
set (CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/../lib)
set (CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/../lib)

# Ensure ACE macros expand to real locks
add_definitions (-DACE_HAS_THREADS -DACE_MT_SAFE=1)

# Create the executable
add_executable (LegacyGLApp ${SOURCES} ${HEADERS})

# 3. Link everything together
# You must link BOTH the Qt modules and the raw OpenGL target
target_link_libraries (LegacyGLApp PRIVATE 
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


######################## Copy shader files ###########################
set (SHADER_DEST_PATH "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/shaders")

file (GLOB SHADER_FILES "${SOURCE_ROOT}/globe/shaders/*")

foreach (SHADER_PATH ${SHADER_FILES})
    get_filename_component (SHADER_NAME ${SHADER_PATH} NAME)

    set (DEST_PATH "${SHADER_DEST_PATH}/${SHADER_NAME}")

    add_custom_command (OUTPUT ${SHADER_DEST_PATH}
                        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${SHADER_PATH} ${DEST_PATH}
                        DEPENDS ${SHADER_PATH}
                        COMMENT "Copying ${SHADER_NAME} if changed"
    )

    list (APPEND SHADER_DEST_FILES ${DEST_PATH})
endforeach()

add_custom_target (copy_assets ALL DEPENDS ${SHADER_DEST_FILES})

# add_custom_command (TARGET LegacyGLApp POST_BUILD
#     COMMAND ${CMAKE_COMMAND} -E copy_directory
#             "${SOURCE_ROOT}/globe/shaders"
#             "$<TARGET_FILE_DIR:LegacyGLApp>/shaders"
# )


#add_custom_command (TARGET LegacyGLApp POST_BUILD
#    COMMAND ${CMAKE_COMMAND} -E copy_if_different
#            "${SOURCE_ROOT}/objects/shaders/Satellite.vert"
#            "$<TARGET_FILE_DIR:LegacyGLApp>/shaders"
#)

#add_custom_command (TARGET LegacyGLApp POST_BUILD
#    COMMAND ${CMAKE_COMMAND} -E copy_if_different
#            "run"
#            "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/run"
#)

#add_custom_command (TARGET LegacyGLApp POST_BUILD
#    COMMAND ${CMAKE_COMMAND} -E copy_directory
#            "${SOURCE_ROOT}/../../textures"
#            "$<TARGET_FILE_DIR:LegacyGLApp>/textures"
#)
