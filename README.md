# Classroom Renderer — 计算机图形学实验二

基于 C++17 + OpenGL 3.3 Core 的轻量级 3D 实时渲染框架。

## 项目架构

```
src/
  main.cpp                    # 入口：资源配置 + 场景组装
  core/
    Application.h/.cpp        # 窗口、输入（鼠标回调+键盘轮询）、主循环
    Camera.h                  # 双模式相机（FPS / 轨道）
    Transform.h               # 位置 / 旋转 / 缩放
  render/
    Renderer.h/.cpp           # 渲染编排（逐帧设置 uniform）
    Shader.h/.cpp             # 着色器编译、链接、uniform 绑定
    Mesh.h/.cpp               # VAO / VBO / EBO 管理 & 绘制
    Texture.h/.cpp            # stb_image 解码 PNG → OpenGL 纹理
    Material.h                # 材质属性结构体（供后续扩展）
  scene/
    Scene.h/.cpp              # 多模型场景管理器（共享同名模型缓存）
    Model.h/.cpp              # Assimp 导入 .fbx/.obj，解析顶点/法线/UV
shaders/
    model.vert                # 顶点着色器：MVP 变换 + 法线/UV 传递
    model.frag                # 片元着色器：Blinn-Phong 光照 + 纹理采样
resources/
    models/                   # .fbx / .obj / .mtl 模型文件
    textures/                 # .png / .jpg 贴图
    material/                 # 测试用简单立方体
ThirdParty/                   # glad, glfw, glm, assimp, stb
```

## 编译与运行

### 依赖

| 库 | 用途 | 版本 |
|----|------|------|
| GLFW | 窗口 + 输入 | 3.4 |
| GLAD | OpenGL 加载器 | 4.1 |
| GLM | 数学库 | 1.0+ |
| Assimp | 3D 模型导入 | 5.4.3 |
| stb_image | PNG/JPG 解码 | 最新 |

### 构建

```bash
mkdir build
cd build
cmake ..
cmake --build . --config Release
```

### 资源目录

```
resources/
├── models/          # 放置 .fbx / .obj 模型
└── textures/        # 放置 .png / .jpg 贴图
└── material/        # 测试用简单立方体
```

## 操作按键

| 按键 | 功能 |
|------|------|
| **左/右键拖拽** | 旋转视角（FPS）/ 围绕目标旋转（轨道） |
| **F** | 切换 FPS ↔ 轨道相机 |
| **W A S D** | 移动 / 平移 |
| **Tab** | 粘滞锁定鼠标（仅 FPS 模式） |
| **滚轮** | 缩放 FOV（FPS）/ 拉近拉远（轨道） |
| **Esc** | 退出 |

## TODO

- **添加模型**：编辑 `main.cpp` 中 `scene.addModel(...)` 区域
- **多光源/阴影**：扩展 `Renderer` 类 + 修改 `model.frag`
- **材质系统**：扩展 `Material` 结构体并传入着色器
- **碰撞检测**：在 `Application` 或 `Scene` 中添加物理逻辑
- **动画**：继承 `Model` 或创建 `AnimatedModel` 类


