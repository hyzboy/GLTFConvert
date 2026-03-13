# GLTFConvert 技术文档

## 概述

**GLTFConvert** 是 ULRE 引擎工具链中的 3D 资产转换器，负责将标准 `.gltf` / `.glb` 模型文件转换为引擎专用的 **Pure Model** 格式，并输出到 `.StaticMesh` 目录中。该工具以命令行方式运行，基于 C++20 标准构建，目标平台为 Windows/MSVC，同时支持 GCC/Clang。

---

## 构建系统

### CMake 配置

| 项目 | 值 |
|---|---|
| CMake 最低版本 | 3.16 |
| C++ 标准 | C++20 |
| 构建模式 | MSVC AVX2 + OpenMP(llvm)，支持 Release `/O2 /Oi /Ot` |
| 静态 CRT | 默认开启（`USE_STATIC_CRT=ON`），MSVC `/MT` |

### 构建目标

```
GLTFConvert/
├── PureModel        (STATIC lib)   — 数据模型层 + 导出管线
└── GLTFConvert      (EXECUTABLE)   — GLTF 导入 + 转换 + 导出主程序
```

### 第三方依赖

| 依赖 | 用途 |
|---|---|
| `glm` | 向量/矩阵/四元数数学 |
| `fastgltf` | GLTF/GLB 文件解析 |
| `nlohmann_json` | JSON 序列化/反序列化 |
| `Vulkan SDK` | VkFormat 枚举（顶点属性格式） |
| `minipack_writer` | 二进制打包格式写入器（自研） |
| `TexConv`（可选） | DirectX 纹理格式转换工具，运行期检测 |

---

## 目录结构与模块划分

```
GLTFConvert/
├── gltf/                — GLTF 数据结构 + 入口
│   ├── import/          — GLTF → GLTFModel 导入层
│   └── convert/         — GLTFModel → pure::Model 转换层
├── pure/                — 引擎 Pure Model 数据结构
├── export/              — pure::Model → 文件导出管线
├── math/                — 边界体积、变换工具
├── common/              — 顶点属性、格式、索引类型等公共定义
├── TexConv.h/.cpp       — TexConv 外部工具封装
└── main_gltf.cpp        — 程序入口
```

---

## 命令行用法

```
GLTFConvert [--no-images | --images-only] [--allow-u8-indices] <input.gltf|.glb> [output_dir]
```

| 参数 | 说明 |
|---|---|
| `<input>` | 输入的 `.gltf` 或 `.glb` 文件路径 |
| `[output_dir]` | 输出目录（可选，默认与输入文件同目录） |
| `--no-images` | 跳过图像导出 |
| `--images-only` | 仅导出图像，跳过几何/材质/场景 |
| `--allow-u8-indices` | 允许 uint8 索引缓冲区（默认禁用，推荐 U16/U32） |

---

## 核心数据结构

### GLTF 层（中间表示）

导入的原始 GLTF 数据以 `GLTFModel` 为根，仅作为中间数据，不参与导出。

```
GLTFModel
├── source              — 原始文件路径
├── primitives[]        — GLTFPrimitive（GLTFGeometry + 材质引用）
├── meshes[]            — GLTFMesh（primitive 索引列表）
├── nodes[]             — GLTFNode（变换 + 子节点 + mesh 引用）
├── scenes[]            — GLTFScene（根节点列表）
├── materials[]         — GLTFMaterial（PBR + 扩展）
├── images[]            — GLTFImage（原始数据或 URI）
├── textures[]          — GLTFTexture（image + sampler 映射）
└── samplers[]          — GLTFSampler（过滤/寻址模式）
```

#### GLTFGeometry

```cpp
struct GLTFGeometry {
    PrimitiveType           primitiveType;       // Triangles 等
    vector<GLTFGeometryAttribute> attributes;    // 顶点属性（含 accessorIndex）
    optional<vector<byte>>  indices;             // 原始索引数据
    optional<size_t>        indexCount;
    IndexType               indexType;           // U8/U16/U32
    optional<size_t>        indicesAccessorIndex;
};
```

#### GLTFMaterial（支持的 KHR 扩展）

| 扩展名 | 数据字段 |
|---|---|
| 核心 PBR | baseColorFactor, metallicFactor, roughnessFactor, textures |
| `KHR_materials_emissive_strength` | emissiveStrength |
| `KHR_materials_unlit` | （无数据，仅标记存在） |
| `KHR_materials_ior` | ior |
| `KHR_materials_transmission` | transmissionFactor, transmissionTexture |
| `KHR_materials_diffuse_transmission` | diffuseTransmissionFactor |
| `KHR_materials_specular` | specularFactor, specularColorFactor |
| `KHR_materials_clearcoat` | clearcoatFactor, roughness, normals |
| `KHR_materials_sheen` | sheenColorFactor, sheenRoughnessFactor |

---

### Pure Model 层（导出中间表示）

`pure::Model` 是最终导出前的引擎本地表示，由 `PureModel` 静态库定义。

```
pure::Model
├── model_source          — 来源文件路径
├── materials[]           — unique_ptr<Material>（多态，含 PBRMaterial 子类）
├── scenes[]              — Scene（name + 根节点索引列表）
├── nodes[]               — Node（name, transform, children, mesh, materials）
├── meshes[]              — Mesh（name, primitives 索引列表, bounding_volume）
├── geometry[]            — Geometry（唯一几何体，含顶点属性、包围体、索引）
├── primitives[]          — Primitive（geometry 索引 + material 索引）
├── images[]              — Image（name, mimeType, data, uri）
├── textures[]            — Texture（sampler + image 映射）
└── samplers[]            — Sampler（wrapS/T, magFilter, minFilter）
```

#### pure::Geometry

```cpp
struct Geometry {
    PrimitiveType                    primitiveType;
    vector<GeometryAttribute>        attributes;       // 顶点属性（VkFormat + 原始bytes）
    BoundingVolumes                  bounding_volume;  // AABB + OBB + Sphere
    optional<vector<byte>>           indicesData;      // 原始索引缓冲
    optional<GeometryIndicesMeta>    indices;          // 索引元数据（count + type）
    optional<vector<glm::vec3>>      positions;        // 解码后的位置数据（用于计算包围体）
    optional<int32_t>                material;
};
```

#### pure::PBRMaterial（继承 Material）

```cpp
struct PBRMaterial : Material {
    PBRMetallicRoughness pbr;           // baseColor/metallic/roughness + textures
    optional<TextureRef> normalTexture;
    optional<TextureRef> occlusionTexture;
    optional<TextureRef> emissiveTexture;
    float                emissiveFactor[3];
    AlphaMode            alphaMode;     // Opaque / Mask / Blend
    float                alphaCutoff;
    bool                 doubleSided;
    // 资源引用索引（用于导出时重映射）
    vector<size_t>       usedTextures;
    vector<size_t>       usedImages;
    vector<size_t>       usedSamplers;
};
```

---

### 公共类型（`common/`）

| 类型 | 说明 |
|---|---|
| `GeometryAttribute` | 顶点属性：name / count / VkFormat / raw data |
| `IndexType` | `U8` / `U16` / `U32` / `AUTO` / `ERR` |
| `PrimitiveType` | 图元类型（Triangles 等） |
| `VkFormat` | Vulkan 顶点 format 枚举（如 VK_FORMAT_R32G32B32_SFLOAT） |
| `VertexAttrib` | 向量基类型 + 分量数编码（历史兼容，新代码用 VkFormat） |

---

### 数学工具（`math/`）

| 结构 | 说明 |
|---|---|
| `AABB` | 轴对齐包围盒（float，min/max） |
| `OBB` | 朝向包围盒（center + axisX/Y/Z + halfSize） |
| `BoundingSphere` | 包围球（center + radius） |
| `BoundingVolumes` | 三合一：AABB + OBB + Sphere，支持 `fromPoints()` |
| `TRS` | 变换三元组（translation/rotation/scale），可转 `glm::mat4` |
| `NodeTransform` | 节点变换（矩阵或 TRS 形式） |
| `PackedBounds` | 用于二进制序列化的紧凑包围体 |

---

## 处理流程

```
┌────────────────────────┐
│  读取 .gltf / .glb     │  fastgltf::Parser
└──────────┬─────────────┘
           ▼
┌────────────────────────┐
│  导入到 GLTFModel      │  gltf::ImportFastGLTF()
│  - Materials           │    fastgltf::Options:
│  - Primitives          │      LoadExternalBuffers
│  - Meshes / Nodes      │      LoadGLBBuffers
│  - Scenes              │      DecomposeNodeMatrices
│  - Images/Textures     │      GenerateMeshIndices
│  - Samplers            │
└──────────┬─────────────┘
           ▼
┌────────────────────────┐
│  坐标系转换            │  Y-up → Z-up
│  - 节点本地变换旋转    │  RotateNodeLocalTransformsYUpToZUp()
│  - Primitive 顶点旋转  │  RotatePrimitivesYUpToZUp()
└──────────┬─────────────┘
           ▼
┌────────────────────────┐
│  转换到 pure::Model    │  gltf::ConvertFromGLTF()
│  - CopyImages          │
│  - CopyTextures        │
│  - CopySamplers        │
│  - CopyMaterials       │  GLTFMaterial → PBRMaterial
│  - CopyScenes/Nodes    │
│  - BuildUniqueGeometry │  相同几何体去重映射
│  - BuildPrimitives     │
│  - BuildMeshes         │
│  - ComputeMeshBounds   │  从 position 属性计算 AABB/OBB/Sphere
└──────────┬─────────────┘
           ▼
┌────────────────────────┐
│  导出文件              │  ExportPureModel()
│  - ExportMaterials     │  → .material JSON
│  - ExportGeometries    │  → .geometry 二进制
│  - ExportMeshes        │  → .mesh JSON
│  - ExportImages        │  → 图像文件（复制或 TexConv 转换）
│  - BuildSceneExportData│  构建场景导出数据
│  - WriteSceneJson      │  → .json
│  - WriteScenePack      │  → .scene 二进制
└────────────────────────┘
```

---

## 输出格式

所有文件输出到 `<output_dir>/<baseName>.StaticMesh/` 目录下。

### 文件命名规则

| 文件类型 | 命名格式 | 示例 |
|---|---|---|
| 几何体 | `<baseName>.<index>.geometry` | `Sword.0.geometry` |
| 材质 | `<baseName>.<matName>.<index>.material` | `Sword.Metal.0.material` |
| Mesh | `<meshName>.mesh` 或 `mesh.<index>.mesh` | `Body.mesh` |
| 图像 | `<baseName>.<imgName>.<index>.<ext>` | `Sword.Albedo.0.png` |
| 场景 JSON | `<baseName>.<sceneName>.json` | `Sword.Scene.json` |
| 场景 Pack | `<baseName>.<sceneName>.scene` | `Sword.Scene.scene` |

名称会经过 `SanitizeName()` 清理（去除非法字符）。

---

### `.geometry` 二进制格式

基于 **MiniPack** 容器格式，由 `minipack_writer` 库生成（自研二进制打包库）。包含：

- 顶点各属性通道的原始字节数据（按 VkFormat 编码）
- 可选索引缓冲（U8/U16/U32）
- 包围体数据（`PackedBounds`）

### `.material` JSON 格式

每个材质独立一个 JSON 文件，仅包含该材质实际引用的 texture/image/sampler（重新编号为本地 0-based 索引）。示例结构：

```json
{
  "type": "PBR",
  "name": "Metal",
  "alphaMode": "Opaque",
  "doubleSided": false,
  "pbr": {
    "baseColorFactor": [1.0, 1.0, 1.0, 1.0],
    "metallicFactor": 1.0,
    "roughnessFactor": 0.5,
    "baseColorTexture": { "index": 0 }
  },
  "images": [ { "name": "Albedo", "uri": "..." } ],
  "textures": [ { "sampler": 0, "image": 0 } ],
  "samplers": [ { "wrapS": "Repeat", "wrapT": "Repeat", ... } ]
}
```

### `.mesh` JSON 格式

```json
{
  "index": 0,
  "name": "Body",
  "primitives": [
    { "primitiveIndex": 3, "geometry": "Sword.0.geometry", "material": "Sword.Metal.0.material" }
  ]
}
```

### `.json` 场景格式（调试可读）

```json
{
  "sceneNameIndex": 0,
  "nameTable": ["Scene", "RootNode", ...],
  "trsTable": [{ "t": [0,0,0], "r": [1,0,0,0], "s": [1,1,1] }, ...],
  "matrixTable": [[...16 floats...]],
  "boundsTable": [{
    "aabbMin": [...], "aabbMax": [...],
    "sphere": [...],
    "obbCenter": [...], "obbAxisX": [...], "obbAxisY": [...], "obbAxisZ": [...], "obbHalf": [...]
  }],
  "rootNodes": [0],
  "nodes": [{
    "originalIndex": 0, "nameIndex": 1,
    "localMatrixIndex": 0, "worldMatrixIndex": 0,
    "trsIndex": 0, "boundsIndex": 0,
    "primitives": [0], "children": [1, 2]
  }],
  "primitives": [{ "originalIndex": 0, "geometryIndex": 0, "geometryFile": "Sword.0.geometry", "materialIndex": 0 }],
  "materials": [{ "originalIndex": 0, "file": "Sword.Metal.0.material" }],
  "geometries": [{ "originalIndex": 0, "file": "Sword.0.geometry" }]
}
```

### `.scene` 二进制格式（I/O 最小化重设计）

当前格式可用，但运行时仍需要根据文件名再读取多个 `.geometry` 文件，磁盘 I/O 次数偏多。  
建议新增 **ScenePackV2**（保留 V1 兼容读取），目标是：

- `MiniPackReader` 路径：**最多两次读取**（一次 Header，一次连续 Payload）
- `MiniPackMemory` 路径：**一次 mmap** 后全部基于指针解包
- CPU 侧尽量不做二次打包，不创建临时 `vector<byte>`
- 让几何/索引数据在文件内连续布局，便于直接上传显存

#### V2 逻辑布局

```text
ScenePackV2
├── Header          (固定大小)
├── TableDirectory  (每个逻辑表的 offset/size)
├── FixedTables     (Node/TRS/Matrix/Bounds/Primitive/GeometryView...)
├── StringPool      (所有字符串集中池化)
└── GeometryBlob    (所有顶点/索引原始字节，按对齐连续拼接)
```

#### V2 Header（示意）

```cpp
struct ScenePackHeader
{
  uint32_t magic;            // 'SCN2'
  uint16_t versionMajor;     // 2
  uint16_t versionMinor;     // 0
  uint32_t flags;            // bit0: little-endian, bit1: hasExternalGeometry
  uint32_t sceneNameStrOff;  // in StringPool

  uint32_t nodeCount;
  uint32_t rootCount;
  uint32_t primitiveCount;
  uint32_t materialCount;
  uint32_t geometryCount;

  uint32_t dirOffset;        // TableDirectory 文件偏移
  uint32_t dirCount;
  uint64_t payloadOffset;    // 连续大块起始偏移
  uint64_t payloadSize;
  uint32_t crc32;            // 可选完整性校验
  uint32_t reserved;
};
```

#### TableDirectory（统一索引）

每个表都用 `(type, offset, size, stride)` 描述，读取端不需要 `FindFile` 多次查找。

```cpp
struct TableDesc
{
  uint32_t type;      // NodeTable / MatrixTable / GeometryBlob ...
  uint32_t flags;
  uint64_t offset;    // 相对文件起始
  uint64_t size;
  uint32_t stride;    // 定长表的元素大小，变长表填 0
  uint32_t reserved;
};
```

#### 关键数据表设计

- `NodeTable`：固定结构数组，不再使用变长 int 流
- `NodeChildIndex`：所有 children 扁平拼接，节点仅保存 `firstChild + childCount`
- `NodePrimIndex`：所有 primitive 索引扁平拼接，节点仅保存 `firstPrim + primCount`
- `PrimitiveTable`：固定结构，引用 `GeometryViewTable` 和材质索引
- `MaterialTable`：固定结构 + 字符串偏移
- `GeometryViewTable`：描述顶点/索引在 `GeometryBlob` 中的偏移与长度
- `StringPool`：统一字符串池，所有名称/URI 都只存 `(offset,length)`
- `GeometryBlob`：所有几何字节连续存放，按 16/64 字节对齐

#### GeometryView（示意）

```cpp
struct GeometryView
{
  uint64_t vertexOffset;     // in GeometryBlob
  uint32_t vertexBytes;
  uint64_t indexOffset;      // in GeometryBlob, 无索引则 UINT64_MAX
  uint32_t indexBytes;
  uint32_t indexType;        // U16/U32
  uint32_t vertexLayoutId;   // 指向顶点布局描述
  uint32_t boundsIndex;      // 指向 BoundsTable
  uint32_t reserved;
};
```

#### 与 MiniPack 的对接方式

为减少改动，可继续放在 MiniPack 容器中，但 Entry 精简为两块：

- `SceneHeaderV2`：固定头 + 目录（小块）
- `ScenePayloadV2`：所有表和 GeometryBlob 的连续大块

这样：

- `MiniPackReader`：读取 `SceneHeaderV2` 一次，再按 `payloadOffset/payloadSize` 一次读取 `ScenePayloadV2`
- `MiniPackMemory`：直接 `Map` 两个 entry，或实现 `MapPayload` 一次拿到整块

#### 运行时加载路径（目标）

1. 读取/映射 Header，校验 magic/version，拿到目录
2. 读取/映射单一 Payload 连续块
3. 所有表用指针视图（span）直接访问，不做二次拷贝
4. 按 `GeometryViewTable` 对 `GeometryBlob` 建 staging 上传任务
5. `vkCmdCopyBuffer` 分批提交到 GPU，CPU 端仅保留元数据

#### 兼容策略

- 读取端优先识别 `SCN2`，失败则回退到现有 V1 Entry 解析
- 导出端短期支持双写：`--scene-v1` / `--scene-v2` 或同时导出
- 验证通过后默认切到 V2

---

## 图像处理

图像导出时会对每张图像推断其主要用途（`ImageUsage`），支持的用途类别：

`BaseColor`, `MetallicRoughness`, `Normal`, `Occlusion`, `Emissive`, `Specular`, `SpecularColor`, `Clearcoat`, `ClearcoatRoughness`, `ClearcoatNormal`, `SheenColor`, `SheenRoughness`, `Transmission`, `DiffuseTransmission`, `Thickness`, `Anisotropy`, `Iridescence`, `IridescenceThickness`, `Unknown`

如果检测到 `TexConv.exe`（DirectXTex 配套工具），则优先调用其进行格式转换；否则直接复制原始文件。

---

## 几何体去重机制

`gltf::BuildUniqueGeometryMapping()` 对所有 `GLTFPrimitive` 进行去重分析，相同 accessor 来源的几何体映射到同一个 `pure::Geometry` 条目，避免重复存储。

`UniqueGeometryMapping` 维护 `primitiveIndex → uniqueGeometryIndex` 的映射关系，供后续 `BuildPrimitives()` 使用。

---

## 坐标系约定

- **GLTF 原始坐标系**：Y-up，右手系
- **ULRE 引擎坐标系**：Z-up
- **转换时机**：在 `ImportFastGLTF()` 完成后、`ConvertFromGLTF()` 之前，分别对节点变换矩阵和顶点 position/normal 等属性执行旋转

---

## 与引擎的集成点

| 输出文件 | 引擎侧用途 |
|---|---|
| `.geometry` | 顶点/索引缓冲加载，VkFormat 直接对应 Vulkan 管线属性 |
| `.material` | PBR 材质参数与贴图绑定，支持各 KHR 扩展 |
| `.mesh` | Mesh 与 Primitive 的组合关系，供场景节点引用 |
| `.scene` | 运行时快速加载的场景树（节点变换、包围体、Primitive 文件列表） |
| `.json` | 调试/编辑器可读的场景数据（与 `.scene` 内容等价） |

---

## 扩展性说明

- `pure::Material` 为虚基类，可派生新材质类型，实现 `toJson()` 即可接入导出管线
- MiniPack 格式以 Entry 名称为键，新增数据流只需添加新 Entry，向后兼容
- `ImageUsage` 枚举和 `BuildImageUsage()` 可扩展支持新 KHR 纹理扩展的用途分类
