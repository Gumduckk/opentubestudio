cmake_minimum_required(VERSION 3.16)
project(GummVTuber LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

find_package(Qt6 REQUIRED COMPONENTS Widgets OpenGL OpenGLWidgets Multimedia)
find_package(OpenGL REQUIRED)

# ---------------------------------------------------------------------------
# Live2D Cubism SDK (baixado por setup.sh para third_party/)
# ---------------------------------------------------------------------------
set(SDK_PATH ${CMAKE_CURRENT_SOURCE_DIR}/third_party/CubismSdkForNative-5-r.5)
set(CORE_PATH ${SDK_PATH}/Core)
set(FRAMEWORK_PATH ${SDK_PATH}/Framework)

if(NOT EXISTS ${CORE_PATH}/lib/linux/x86_64/libLive2DCubismCore.a)
  message(FATAL_ERROR
    "Cubism SDK não encontrado em ${SDK_PATH}.\n"
    "Rode ./setup.sh uma vez para baixar o SDK oficial da Live2D (grátis).")
endif()

add_library(Live2DCubismCore STATIC IMPORTED)
set_target_properties(Live2DCubismCore PROPERTIES
  IMPORTED_LOCATION ${CORE_PATH}/lib/linux/x86_64/libLive2DCubismCore.a
  INTERFACE_INCLUDE_DIRECTORIES ${CORE_PATH}/include
)

set(FRAMEWORK_SOURCE OpenGL)
add_subdirectory(${FRAMEWORK_PATH} ${CMAKE_CURRENT_BINARY_DIR}/Framework)
target_compile_definitions(Framework PUBLIC CSM_TARGET_LINUX_GL)
target_include_directories(Framework PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/third_party/glew_shim)
target_link_libraries(Framework Live2DCubismCore)

# ---------------------------------------------------------------------------
# Aplicativo
# ---------------------------------------------------------------------------
add_executable(GummVTuber
  src/main.cpp
  src/config/AppConfig.h
  src/config/AppConfig.cpp
  src/live2d/VTuberModel.h
  src/live2d/VTuberModel.cpp
  src/gl/ModelView.h
  src/gl/ModelView.cpp
  src/track/MouseTracker.h
  src/track/MouseTracker.cpp
  src/track/EvdevMouse.h
  src/track/EvdevMouse.cpp
  src/track/MicTracker.h
  src/track/MicTracker.cpp
  src/track/WebcamTracker.h
  src/track/WebcamTracker.cpp
  src/track/FaceTrackerInput.h
  src/track/OneEuroFilter.h
  src/ui/MainWindow.h
  src/ui/MainWindow.cpp
  src/ui/SetupWizard.h
  src/ui/SetupWizard.cpp
)

target_include_directories(GummVTuber PRIVATE
  src
  ${CMAKE_CURRENT_SOURCE_DIR}/third_party/glew_shim
)

target_link_libraries(GummVTuber PRIVATE
  Qt6::Widgets
  Qt6::OpenGL
  Qt6::OpenGLWidgets
  Qt6::Multimedia
  Framework
  OpenGL::GL
)

# Shaders desktop do framework (carregados do diretório do executável)
add_custom_command(TARGET GummVTuber POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          ${FRAMEWORK_PATH}/src/Rendering/OpenGL/Shaders/Standard
          $<TARGET_FILE_DIR:GummVTuber>/FrameworkShaders
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          ${CMAKE_CURRENT_SOURCE_DIR}/tools
          $<TARGET_FILE_DIR:GummVTuber>/tools
  COMMENT "Copiando shaders do Cubism Framework e ferramentas Python"
)
