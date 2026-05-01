# CMakeLists.txt defines the buildable project for the 3D GIS assignment.
# Upstream: the Clean Architecture source tree under src.
# Downstream: Visual Studio or command-line CMake builds the app and use cases.
cmake_minimum_required(VERSION 3.16)

# ThreeDGISAssignment is the root project for the 3D GIS assignment.
# Upstream: source files under the four architecture layers.
# Downstream: later batches will add terrain mesh construction and graphics adapters.
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

# ThreeDGISApp is the current executable entry.
# Upstream: UI entry, Application use cases, and Infrastructure adapters.
# Downstream: later UI work can replace the console check with a real desktop window entry.
add_executable(ThreeDGISApp
    src/ui/main.cpp
    src/application/usecases/AddModelToSceneUseCase.cpp
    src/application/usecases/LoadSingleModelUseCase.cpp
    src/application/usecases/LoadTerrainUseCase.cpp
    src/infrastructure/model/ObjModelLoader.cpp
    src/infrastructure/raster/GeoTiffDemReader.cpp
    src/infrastructure/raster/GeoTiffImageReader.cpp
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
    src/application/usecases/AddModelToSceneUseCase.h
    src/application/usecases/LoadSingleModelUseCase.h
    src/application/usecases/LoadTerrainUseCase.h
    src/domain/math/Vec2.h
    src/domain/math/Vec3.h
    src/domain/geometry/Vertex.h
    src/domain/geometry/Triangle.h
    src/domain/geometry/Mesh.h
    src/domain/texture/TextureImage.h
    src/domain/terrain/TerrainRaster.h
    src/domain/terrain/TerrainMesh.h
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
    src/infrastructure/model/ObjModelLoader.h
    src/infrastructure/raster/GeoTiffDemReader.h
    src/infrastructure/raster/GeoTiffImageReader.h
)
