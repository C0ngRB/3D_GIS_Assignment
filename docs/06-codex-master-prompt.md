---

# 9. 文件六：`docs/architecture/06-codex-master-prompt.md`

```md
# 06-codex-master-prompt.md

# 给 Codex 的总控提示词

你现在负责实现一个三维 GIS 综合实习桌面程序。

你必须先阅读并遵守以下架构文件：

1. docs/architecture/01-overview.md
2. docs/architecture/02-module-contracts.md
3. docs/architecture/03-class-diagram.puml
4. docs/architecture/04-sequence-diagrams.puml
5. docs/architecture/05-implementation-tasks.md

## 一、项目目标

本项目用于完成“三维 GIS 原理与应用”综合实习开发任务。

必须支持：

1. 加载点云处理后生成的 OBJ 模型；
2. 加载宋卿体育馆单个 OBJ 模型；
3. 读取 DEM GeoTIFF；
4. 读取 RGB 遥感影像 GeoTIFF；
5. 根据 DEM 构建地形三角网；
6. 计算三角网法向量；
7. 计算纹理坐标；
8. 将遥感影像贴到 DEM 地形表面；
9. 将模型和地形加入三维场景；
10. 批量加载宋卿体育馆模型；
11. 支持基本三维交互功能。

## 二、强制架构规则

1. 严格遵守 Clean Architecture。
2. UI 层只负责界面和用户交互。
3. Application 层只负责编排用例。
4. Domain 层只保存核心数据结构、接口和几何算法。
5. Infrastructure 层负责文件读取、渲染适配和系统资源访问。
6. Domain 层不得包含 Qt、OpenGL、GDAL、文件系统访问。
7. MainWindow 中不得写 OBJ 解析、DEM 读取、三角网构建、法向量计算和纹理坐标计算。
8. Renderer 不得负责读取文件或构建地形三角网。
9. 不允许引入 Web、REST API、Electron 或远程服务。
10. 不允许重写整个项目。
11. 不允许破坏已有可运行功能。
12. 每次修改必须保证项目可编译。

## 三、执行方式

必须按 Batch 实现，不允许一次实现全部内容。

每个 Batch 开始前，先输出：

1. 本批目标；
2. 计划新增文件；
3. 计划修改文件；
4. 不修改的文件；
5. 可能风险。

每个 Batch 完成后，输出：

1. 实际新增文件；
2. 实际修改文件；
3. 每个文件的修改目的；
4. 核心实现摘要；
5. 编译验证方式；
6. git diff 摘要；
7. 是否影响已有功能。

## 四、当前第一阶段任务

先执行 Batch A：建立 Clean Architecture 骨架。

具体要求：

1. 建立 src/ui、src/application、src/domain、src/infrastructure 四层目录；
2. 新增 Domain 层基础数据结构；
3. 新增 Domain 层接口；
4. 不实现复杂 OBJ 解析；
5. 不实现 DEM 读取；
6. 不实现 OpenGL 渲染；
7. 确保项目可编译；
8. 确保 Domain 层没有 Qt 头文件。

完成 Batch A 后停止，等待下一条任务。
```
