# CMakeLists.txt defines the buildable project for the 3D GIS assignment.
# Upstream: the Clean Architecture source tree under src.
# Downstream: Visual Studio or command-line CMake builds the app and use cases.
cmake_minimum_required(VERSION 3.16)

# ThreeDGISAssignment is the root project for the 3D GIS assignment.
# Upstream: source files under the four architecture layers.
# Downstream: later batches will add full desktop UI integration.
project(ThreeDGISAssignment LANGUAGES CXX)

# C++17 is the common language level for all project code.
# Upstream: every C++ source file in this project.
# Downstream: the compiler validates Domain, Application, Infrastructure, and UI code consistently.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# The local Anaconda installation provides the GDAL C headers and import library.
# Upstream: GeoTiffDemReader and GeoTiffImageReader include GDAL only in Infrastructure.
# Downstream: the executable links the raster adapters without exposing GDAL to Domain.
set(DEFAULT_GDAL_ROOT "$ENV{USERPROFILE}/anaconda3/Library")
find_path(GDAL_INCLUDE_DIR
    NAMES gdal.h
    HINTS "${DEFAULT_GDAL_ROOT}/include"
    REQUIRED
)
find_library(GDAL_LIBRARY
    NAMES gdal gdal_i
    HINTS "${DEFAULT_GDAL_ROOT}/lib"
    REQUIRED
)
set(GDAL_RUNTIME_BIN_DIR "${DEFAULT_GDAL_ROOT}/bin")
find_file(GDAL_RUNTIME_DLL
    NAMES gdal.dll
    HINTS "${GDAL_RUNTIME_BIN_DIR}"
    REQUIRED
)
file(GLOB GDAL_RUNTIME_DEPENDENCY_DLLS
    "${GDAL_RUNTIME_BIN_DIR}/*.dll"
)

# ThreeDGISApp is the current executable entry.
# Upstream: UI entry, Application use cases, Domain algorithms, and Infrastructure adapters.
# Downstream: later UI work can replace the console check with a real desktop window entry.
add_executable(ThreeDGISApp
    src/ui/main.cpp
    src/ui/MainWindow.cpp
    src/ui/ViewportWidget.cpp
    src/ui/Win32DesktopWindow.cpp
    src/application/usecases/AddModelToSceneUseCase.cpp
    src/application/usecases/AddTerrainToSceneUseCase.cpp
    src/application/usecases/BuildTerrainMeshUseCase.cpp
    src/application/usecases/LoadBatchModelsUseCase.cpp
    src/application/usecases/LoadOsgbTilesUseCase.cpp
    src/application/usecases/LoadSingleModelUseCase.cpp
    src/application/usecases/LoadTerrainUseCase.cpp
    src/domain/geometry/NormalCalculator.cpp
    src/domain/geometry/TextureCoordinateCalculator.cpp
    src/domain/terrain/TerrainMeshBuilder.cpp
    src/infrastructure/model/FileSystemModelBatchRepository.cpp
    src/infrastructure/model/ObjModelLoader.cpp
    src/infrastructure/model/OsgbToObjConverter.cpp
    src/infrastructure/raster/GeoTiffDemReader.cpp
    src/infrastructure/raster/GeoTiffImageReader.cpp
    src/infrastructure/rendering/OpenGLSceneRenderer.cpp
    src/infrastructure/rendering/SceneScreenshotWriter.cpp
)

# The src folder is the include root for domain/... headers.
# Upstream: Domain types, Application use cases, and Infrastructure adapters.
# Downstream: all layers include contracts by stable project-relative paths.
target_include_directories(ThreeDGISApp PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${GDAL_INCLUDE_DIR}
)

# GDAL is linked only by the executable that owns the Infrastructure adapters.
# Upstream: GeoTIFF readers call the GDAL C API.
# Downstream: DEM and image data are returned as Domain structures.
target_link_libraries(ThreeDGISApp PRIVATE
    ${GDAL_LIBRARY}
)

# The GDAL runtime DLL must be discoverable when Visual Studio starts the app.
# Upstream: GeoTIFF infrastructure links against the GDAL import library.
# Downstream: the debugger and direct exe runs can resolve gdal.dll at startup.
set_property(TARGET ThreeDGISApp PROPERTY
    VS_DEBUGGER_ENVIRONMENT "PATH=${GDAL_RUNTIME_BIN_DIR};%PATH%"
)

# Copy GDAL and Anaconda runtime DLLs next to the executable as a direct-run fallback.
# Upstream: CMake finds the Anaconda Library/bin runtime folder.
# Downstream: ThreeDGISApp.exe can resolve transitive dependencies such as liblzma.dll.
add_custom_command(TARGET ThreeDGISApp POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${GDAL_RUNTIME_DEPENDENCY_DLLS}
        "$<TARGET_FILE_DIR:ThreeDGISApp>"
)

# Win32 user interface libraries provide the native desktop window and file dialogs.
# Upstream: Win32DesktopWindow.cpp calls User32, Comdlg32, Shell32, and Ole32 APIs.
# Downstream: the same executable can be double-clicked as an interactive app.
if(WIN32)
    target_link_libraries(ThreeDGISApp PRIVATE
        user32
        gdi32
        comdlg32
        shell32
        ole32
    )
endif()

# MinGW builds are linked with their language runtime libraries statically.
# Upstream: developers may run ThreeDGISApp.exe by double-clicking outside a shell.
# Downstream: the executable still loads GDAL dynamically from the installed environment.
if(MINGW)
    target_link_options(ThreeDGISApp PRIVATE
        -static-libgcc
        -static-libstdc++
    )
endif()

# Header files are listed for IDE visibility only.
# Upstream: generated Domain data structures, ports, use cases, and adapters.
# Downstream: Visual Studio shows the architecture skeleton in the target tree.
target_sources(ThreeDGISApp PRIVATE
    src/ui/MainWindow.h
    src/ui/ViewportWidget.h
    src/ui/Win32DesktopWindow.h
    src/application/usecases/AddModelToSceneUseCase.h
    src/application/usecases/AddTerrainToSceneUseCase.h
    src/application/usecases/BuildTerrainMeshUseCase.h
    src/application/usecases/LoadBatchModelsUseCase.h
    src/application/usecases/LoadOsgbTilesUseCase.h
    src/application/usecases/LoadSingleModelUseCase.h
    src/application/usecases/LoadTerrainUseCase.h
    src/domain/math/Vec2.h
    src/domain/math/Vec3.h
    src/domain/geometry/NormalCalculator.h
    src/domain/geometry/TextureCoordinateCalculator.h
    src/domain/geometry/Vertex.h
    src/domain/geometry/Triangle.h
    src/domain/geometry/Mesh.h
    src/domain/texture/TextureImage.h
    src/domain/terrain/TerrainRaster.h
    src/domain/terrain/TerrainMesh.h
    src/domain/terrain/TerrainMeshBuilder.h
    src/domain/model/Material.h
    src/domain/model/Transform.h
    src/domain/model/ModelAsset.h
    src/domain/scene/SceneNode.h
    src/domain/scene/SceneGraph.h
    src/domain/ports/IModelLoader.h
    src/domain/ports/IDemReader.h
    src/domain/ports/IImageReader.h
    src/domain/ports/IModelBatchRepository.h
    src/domain/ports/ISceneRenderer.h
    src/domain/ports/IOsgbConverter.h
    src/infrastructure/model/FileSystemModelBatchRepository.h
    src/infrastructure/model/ObjModelLoader.h
    src/infrastructure/model/OsgbToObjConverter.h
    src/infrastructure/raster/GeoTiffDemReader.h
    src/infrastructure/raster/GeoTiffImageReader.h
    src/infrastructure/rendering/OpenGLSceneRenderer.h
    src/infrastructure/rendering/SceneScreenshotWriter.h
)
