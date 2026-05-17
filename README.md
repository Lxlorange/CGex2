# Classroom Renderer — 计算机图形学实验二

基于 C++17 + OpenGL 3.3 Core 的轻量级 3D 实时渲染框架。

## 项目架构

```
src/
  main.cpp                    # 入口：资源配置 + 场景组装
  core/
    Application.h/.cpp        # 窗口、输入（鼠标回调+键盘轮询）、主循环
    Camera.h                  # 自由漫游相机（自身旋转 + 水平移动 + 垂直升降）
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
| **按住鼠标左键或右键拖拽** | 围绕相机自身旋转视角，不再围绕场景中心点旋转 |
| **Tab** | 切换鼠标视角锁定；锁定后移动鼠标即可转向 |
| **W / S** | 沿当前朝向的水平投影前进 / 后退 |
| **A / D** | 沿当前右向量的水平投影左移 / 右移 |
| **Space / Left Shift** | 沿世界 Y 轴上升 / 下降 |
| **滚轮** | 调整视野 FOV |
| **1 / 2** | 切换白天 / 黑夜环境 |
| **O / C** | 夜晚模式下开启 / 关闭教室点光源 |
| **Esc** | 退出 |

## TODO

- **添加模型**：编辑 `main.cpp` 中 `scene.addModel(...)` 区域
- **多光源/阴影**：扩展 `Renderer` 类 + 修改 `model.frag`
- **材质系统**：扩展 `Material` 结构体并传入着色器
- **碰撞检测**：在 `Application` 或 `Scene` 中添加物理逻辑
- **动画**：继承 `Model` 或创建 `AnimatedModel` 类


