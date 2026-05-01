# 05-implementation-tasks.md

# 分批实现任务清单

## 总规则

Codex 必须按批次实现，不允许一次性重写整个项目。

每一批开始前必须输出：

1. 本批目标；
2. 计划新增文件；
3. 计划修改文件；
4. 不会修改的文件；
5. 风险点。

每一批完成后必须输出：

1. 实际新增文件；
2. 实际修改文件；
3. 核心实现摘要；
4. 是否影响已有功能；
5. 编译或运行验证方式；
6. git diff 摘要。

---

## Batch A：建立 Clean Architecture 骨架

### 目标

建立四层目录结构和核心接口，不实现复杂逻辑。

### 建议目录

```text
src/
  ui/
  application/
  domain/
  infrastructure/
新增文件
src/domain/math/Vec2.h
src/domain/math/Vec3.h
src/domain/geometry/Mesh.h
src/domain/geometry/Vertex.h
src/domain/geometry/Triangle.h
src/domain/texture/TextureImage.h
src/domain/terrain/TerrainRaster.h
src/domain/terrain/TerrainMesh.h
src/domain/model/ModelAsset.h
src/domain/scene/SceneNode.h
src/domain/scene/SceneGraph.h

src/domain/ports/IModelLoader.h
src/domain/ports/IDemReader.h
src/domain/ports/IImageReader.h
src/domain/ports/IModelBatchRepository.h
src/domain/ports/ISceneRenderer.h
验收标准
项目可以编译；
只有数据结构和接口；
不写 OBJ 解析；
不写 DEM 读取；
不写 OpenGL 实现；
Domain 层不包含 Qt 头文件。
Batch B：实现 OBJ 单模型加载
目标

实现单个 OBJ 模型加载，并将模型加入场景。

新增文件
src/application/usecases/LoadSingleModelUseCase.h
src/application/usecases/LoadSingleModelUseCase.cpp
src/application/usecases/AddModelToSceneUseCase.h
src/application/usecases/AddModelToSceneUseCase.cpp

src/infrastructure/model/ObjModelLoader.h
src/infrastructure/model/ObjModelLoader.cpp
功能要求
支持读取 OBJ 顶点；
支持读取 OBJ 面片；
支持三角面；
对非三角面可先报错或进行简单三角化；
支持缺失法向量时后续生成法向量；
返回 ModelAsset；
可以加入 SceneGraph。
验收标准
可以加载点云生成的 OBJ；
可以加载宋卿体育馆单个 OBJ；
模型可以出现在三维视图；
加载失败时返回错误消息，不直接崩溃。
Batch C：实现 DEM 和影像读取
目标

实现 DEM GeoTIFF 和 RGB 影像读取。

新增文件
src/application/usecases/LoadTerrainUseCase.h
src/application/usecases/LoadTerrainUseCase.cpp

src/infrastructure/raster/GeoTiffDemReader.h
src/infrastructure/raster/GeoTiffDemReader.cpp
src/infrastructure/raster/GeoTiffImageReader.h
src/infrastructure/raster/GeoTiffImageReader.cpp
功能要求
读取 DEM 宽度、高度、像元大小；
读取 DEM elevation 数组；
读取影像 width、height、channels、pixels；
检查 DEM 和影像是否为空；
检查二者范围是否大致一致；
若无法读取坐标信息，仍允许作为普通纹理读取，但要给出 warning。
验收标准
可以读取 DalianDem_Clip.tif；
可以读取 Dalian_Clip.tif；
可以输出 DEM 行列数；
可以输出影像行列数；
出错时有明确错误信息。
Batch D：实现地形三角网构建、法向量和纹理坐标
目标

根据 DEM 构建 TerrainMesh，并计算法向量和纹理坐标。

新增文件
src/application/usecases/BuildTerrainMeshUseCase.h
src/application/usecases/BuildTerrainMeshUseCase.cpp

src/domain/terrain/TerrainMeshBuilder.h
src/domain/terrain/TerrainMeshBuilder.cpp
src/domain/geometry/NormalCalculator.h
src/domain/geometry/NormalCalculator.cpp
src/domain/geometry/TextureCoordinateCalculator.h
src/domain/geometry/TextureCoordinateCalculator.cpp
功能要求
samplingStep 控制 DEM 降采样；
verticalScale 控制垂直夸张；
每个 DEM 网格单元生成两个三角形；
顶点包含 position、normal、texCoord；
法向量使用三角面叉乘累加后归一化；
纹理坐标范围为 [0, 1]。
验收标准
TerrainMesh 顶点数正确；
Triangle 索引不越界；
normal 不为零向量；
texCoord 在 [0,1] 范围内；
地形可以被加入 SceneGraph；
可在三维窗口显示。
Batch E：实现影像贴图和 OpenGL 渲染适配
目标

将 TerrainMesh 和 ModelAsset 转换为 OpenGL 可绘制对象。

新增或修改文件
src/infrastructure/rendering/OpenGLSceneRenderer.h
src/infrastructure/rendering/OpenGLSceneRenderer.cpp
src/ui/ViewportWidget.h
src/ui/ViewportWidget.cpp
功能要求
支持 Mesh 渲染；
支持地形纹理贴图；
支持模型和地形同时显示；
支持简单光照；
支持相机旋转、缩放、平移；
支持重置视角。
验收标准
OBJ 模型能显示；
DEM 地形能显示；
影像能贴到地形表面；
设置 verticalScale 后地形起伏明显；
交互操作正常。
Batch F：实现批量模型加载
目标

实现宋卿体育馆模型批量加载。

新增文件
src/application/usecases/LoadBatchModelsUseCase.h
src/application/usecases/LoadBatchModelsUseCase.cpp
src/infrastructure/model/FileSystemModelBatchRepository.h
src/infrastructure/model/FileSystemModelBatchRepository.cpp
功能要求
输入模型文件夹；
搜索 .obj 文件；
可选递归搜索；
逐个调用 IModelLoader；
加载成功的模型加入结果；
加载失败的模型记录错误，不影响其他模型；
最终批量加入 SceneGraph。
验收标准
可以一次性加载多个 OBJ；
部分模型失败不导致程序崩溃；
成功加载的模型显示在场景中；
UI 显示加载数量和失败数量。
Batch G：UI 集成和最小报告截图功能
目标

完成课程演示所需 UI 功能。

UI 功能
打开单个 OBJ；
打开 DEM；
打开影像；
构建地形；
批量加载模型；
清空场景；
重置视角；
设置采样间隔；
设置垂直夸张系数；
截图保存当前视图。
验收标准
老师要求的四类功能均可通过 UI 操作完成；
程序可稳定运行；
能输出至少三类截图：
单模型加载截图；
DEM + 影像三维地形截图；
批量模型加载截图。
```
