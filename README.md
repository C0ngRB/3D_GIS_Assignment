# 3D GIS Assignment

三维 GIS 综合实习项目，包含点云/网格模型显示、DEM 与遥感影像叠加建模、OSGB 倾斜摄影数据查看，以及实习报告材料。

## 目录结构

| 路径 | 内容 |
| --- | --- |
| `code/` | 课程实习主程序源码，使用 CMake + C++ 实现。 |
| `data/` | 实习数据目录，包含点云、OBJ、DEM、影像和本地 OSGB 数据。 |
| `docs/` | 需求说明、模块契约、任务拆解和 Codex 工作提示。 |
| `report/` | 最终实习报告文档。 |
| `GIS3D3/` | 本地参考工程，MFC + OpenSceneGraph 示例，不作为主程序交付路径。 |

> `code/out/`、`code/.vs/`、`code/cmake-build-*`、二进制 DLL/EXE/PDB、OSGB 转换缓存等都是本地生成内容，不应提交。

## 主程序功能

主程序位于 `code/`，入口为：

```text
code/src/ui/main.cpp
```

无命令行参数运行时，会启动 Win32 桌面窗口：

- `Open OBJ`：加载单个 OBJ 点云或网格模型。
- `Batch OBJ`：批量加载文件夹内 OBJ 模型。
- `Open DEM` + `Open Image` + `Build Terrain`：读取 DEM 和遥感影像，构建纹理地形三角网。
- `Open OSGB`：调用 OpenSceneGraph 的 `osgviewer.exe` 原生打开 OSGB 根瓦片，保留纹理、LOD 和分页效果。
- `Yaw/Pitch/Zoom/Pan/Reset`：进行基础视角操作。
- `Save BMP`：保存当前内置预览结果。

## 构建方式

推荐使用 Visual Studio 打开 `code/` 目录的 CMake 工程。

1. 打开 Visual Studio。
2. 选择“打开本地文件夹”，打开：

```text
F:\3D_GIS_Assignment\code
```

3. 选择有效启动项 `ThreeDGISApp.exe`。
4. 执行“生成 -> 重新生成全部”。
5. 运行 `main()` 对应目标。

GDAL 运行时 DLL 会由 CMake 从本机 GDAL/Anaconda 目录复制到输出目录；如果 Visual Studio 缓存异常，先执行“CMake -> 删除缓存并重新配置”。

## DEM 与影像地形

示例数据：

```text
data/dem_image_processed/DalianDem_Clip.tif
data/dem_image_processed/Dalian_Clip.tif
```

操作顺序：

1. 点击 `Open DEM`，选择 `DalianDem_Clip.tif`。
2. 点击 `Open Image`，选择 `Dalian_Clip.tif`。
3. 设置 `Step` 和 `Scale`。
4. 点击 `Build Terrain`。

建议参数：

- `Step = 15`：默认精度，速度和清晰度平衡。
- `Step = 10`：更细，三角面更多，显示更清晰。
- `Scale = 1`：保持真实高程比例。

程序会把经纬度像元大小换算为近似米单位，避免 DEM 的水平距离和高程单位不一致导致山体失真。

## OSGB 显示

宋卿体育馆 OSGB 数据位于本地：

```text
data/Songqing/OSGB
```

点击 `Open OSGB` 后选择该文件夹。程序会自动查找根瓦片，例如：

```text
Tile_+017_+031/Tile_+017_+031.osgb
```

然后用 `osgviewer.exe` 打开。这样显示效果接近专业 OSGB 软件，因为 OpenSceneGraph 会原生处理：

- 纹理贴图
- LOD 层级
- PagedLOD 子瓦片
- 相机交互
- 模型材质和渲染状态

如果程序提示找不到 `osgviewer.exe`，可设置环境变量：

```text
THREEDGIS_OSGVIEWER=C:\Users\congr\anaconda3\envs\three_gis_osg\Library\bin\osgviewer.exe
```

## 命令行验证

主程序也支持命令行方式用于快速验证：

```powershell
ThreeDGISApp.exe <obj-file>
ThreeDGISApp.exe <dem.tif> <image.tif> <step> <scale>
ThreeDGISApp.exe --batch <obj-folder>
ThreeDGISApp.exe --osgb <osgb-folder> <maxTiles>
```

其中 `--osgb` 命令行路径主要用于转换和统计验证；高质量交互显示推荐使用窗口中的 `Open OSGB`，由 `osgviewer` 原生渲染。

## 报告材料

最终报告放在：

```text
report/三维GIS实习报告.docx
```

报告中可说明：主程序完成了 DEM 影像叠加、地形三角网构建、法向量计算、纹理坐标计算、OBJ 模型加入场景；OSGB 数据量大且自带 LOD/PagedLOD 结构，因此专业显示采用 OpenSceneGraph 原生查看路径。
