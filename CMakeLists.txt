cmake_minimum_required(VERSION 3.10)

project(VulkanFlappyBird VERSION 0.1)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

set(NODEBUG 0)

if (WIN32)
  set(IS_WINDOWS 1)
else ()
  set(IS_WINDOWS 0)
endif ()

configure_file("src/cfg.hxx.in" "cfg.hxx")

add_subdirectory("${PROJECT_SOURCE_DIR}/lib/glfw-3.4")
add_subdirectory("${PROJECT_SOURCE_DIR}/lib/glm")


# macOS-specific configurations
if (APPLE)
    # Set macOS bundle properties if creating an app bundle
    set(MACOSX_BUNDLE_BUNDLE_NAME "VulkanFlappyBird")
    set(MACOSX_BUNDLE_COPYRIGHT "Copyright © 2024")
    set(MACOSX_BUNDLE_GUI_IDENTIFIER "com.yourapp.vulkanflappybird")
    set(MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}")
    
    # Required for Vulkan on macOS (MoltenVK)
    find_library(COCOA_LIBRARY Cocoa)
    find_library(IOKIT_LIBRARY IOKit)
    find_library(COREVIDEO_LIBRARY CoreVideo)
    find_library(QUARTZCORE_LIBRARY QuartzCore)
    
    # Use Metal backend for Vulkan on macOS (MoltenVK)
    add_compile_definitions(VK_USE_PLATFORM_METAL_EXT)
endif()

if (APPLE)
  # For macOS, you might want to create an app bundle
  add_executable(${PROJECT_NAME} MACOSX_BUNDLE src/vulkan.cxx)
else ()
  add_executable(${PROJECT_NAME} src/vulkan.cxx)
endif ()

find_package(Vulkan REQUIRED)

target_include_directories(${PROJECT_NAME} 
  PRIVATE 
  "${PROJECT_SOURCE_DIR}/lib/Base64CPPLib/include"
  ${Vulkan_INCLUDE_DIR}
  ${PROJECT_BINARY_DIR}
)

target_link_libraries(${PROJECT_NAME}
  PUBLIC 
  glfw
  glm
  Vulkan::Vulkan)

# Add macOS-specific frameworks
if (APPLE)
    target_link_libraries(${PROJECT_NAME} PRIVATE
        ${COCOA_LIBRARY}
        ${IOKIT_LIBRARY}
        ${COREVIDEO_LIBRARY}
        ${QUARTZCORE_LIBRARY}
        "-framework Foundation"
        "-framework Metal"
        "-framework QuartzCore"
    )
    
    # Set deployment target for better compatibility
    set_target_properties(${PROJECT_NAME} PROPERTIES
        MACOSX_RPATH ON
        OSX_ARCHITECTURES "x86_64;arm64"  # Universal binary for Intel and Apple Silicon
    )
    
    # Optional: Set minimum macOS version
    if (NOT CMAKE_OSX_DEPLOYMENT_TARGET)
        set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "Minimum macOS version" FORCE)
    endif()
endif()


set_target_properties(${PROJECT_NAME} PROPERTIES
  CXX_STANDARD ${CMAKE_CXX_STANDARD}
  CXX_STANDARD_REQUIRED ${CMAKE_CXX_STANDARD_REQUIRED}
)

