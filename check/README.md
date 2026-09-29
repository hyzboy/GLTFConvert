# check/ —— GLTFConvert 变换链检查

`verify_transform_chain.py` 检查**节点局部变换 + Y-up→Z-up 转换**是否自洽。

## 为什么需要它

导入链有三条容易踩坏的约定（每一条都曾被真实踩坏并被这个脚本抓住）：

1. **配对**：图元顶点被 `R = Rx(+90°)` 旋转（`v' = R·v`），因此节点局部变换必须是
   `M' = R·M_raw·R⁻¹`，这样 `M'·v' = R·(M·v)`（全链路恰好一次 R）。
2. **分解**：局部矩阵必须在导入边界无损分解为 TRS，且**镜像（det<0）不能被丢弃**。
3. **不可表示即失败**：含剪切/两轴以上退化的矩阵无法用 TRS 表示，必须 fail-fast，
   不允许"按最接近的 TRS 导出"这种静默降级。

判据全部是数值对拍，不是"看起来对"。

## 直接运行（不需要 CMake）

```bash
python check/verify_transform_chain.py --exe <GLTFConvert.exe>
python check/verify_transform_chain.py --exe <GLTFConvert.exe> --quick          # 只跑合成用例
python check/verify_transform_chain.py --exe <GLTFConvert.exe> \
       --models path/a.glb path/b.gltf            # 追加真实模型（要求转换成功且误差在容差内）
python check/verify_transform_chain.py --exe <GLTFConvert.exe> --keep          # 保留工作目录
python check/verify_transform_chain.py --exe <GLTFConvert.exe> --report r.json # 机器可读结果
```

- 只依赖 **Python 3 标准库**；除可执行文件外不需要仓库、不需要资产（合成用例自带顶点数据）。
- **只支持 glTF 2.0**（与 fastgltf / 转换器同口径）；1.x 会给出明确错误
  （仓库里的 `res/model/color_teapot_spheres.gltf` 是 assimp 导出的 glTF 1.0，不能用它当样本）。
- 退出码：`0` 通过 / `2` 有检查失败 / `3` 环境或用法错误。
- 脚本自行生成三个合成场景（写入 `--work`，默认临时目录）：`ok.gltf`（必须成功且逐节点精确，
  覆盖 TRS 均匀/非均匀、matrix 非均匀、matrix 镜像、TRS 镜像、单轴退化 scale=0、多根）、
  `bad_shear.gltf`（含剪切 → 必须 fail-fast 且错误信息点名节点）、
  `bad_flat.gltf`（两轴退化 → 必须 fail-fast）。

## 通过 CMake 接入（两种方式）

**1) 自定义目标（推荐）** —— VS 解决方案里可见，不依赖 ctest：

```bash
cmake --build build --config Debug --target GLTFConvertTransformCheck
```

> ⚠ **不要用根级 `ctest`**：ULRE 根没有调用 `enable_testing()`，根目录跑 `ctest` 会输出
> `No tests were found!!!`（既有状态，`src/ecs` 下的 `add_test` 同样如此）。

**2) ctest（需打开选项，且要在本目录跑）**：

```bash
cmake -DGLTF_BUILD_TRANSFORM_CHECK=ON <其它原有参数> .
ctest --test-dir build/src/Tools/GLTFConvert -C Debug -R GLTFConvertTransformChain --output-on-failure
```

选项默认 **OFF**（不改变默认构建行为）。

## 检查项与容差

| 项 | 内容 | 默认容差 |
|---|---|---|
| [A] | 导出 TRS 展开 vs `R·M_raw·R⁻¹`（逐元素） | `--tolerance` = 1e-5 |
| [B] | `matrixTable` 的 `localM` vs 同节点 TRS 展开（导出自洽） | 同上 |
| [C] | 叶子节点 `boundsTable` AABB vs `worldM·(R·v)`（证明顶点只被旋转一次） | `--bounds-tolerance` = 1e-3 |
| [D] | 导出 TRS 的 `\|q\| ≈ 1`（非单位四元数 = 分解出错） | 1e-3 |
| [E] | 镜像用例导出后仍 `det < 0`（镜像没被静默丢弃） | 精确判定 |
| [F] | 源节点无任何变换键 ⇒ 导出不得有 trs 条目，**或**该条目在容差内等于单位矩阵；且 `localM` 必须是单位矩阵 | `--tolerance` |

**容差 = 绝对项 + 相对项·max\|量级\|**（`--rel-tolerance`，默认 1e-5）。
真实资产里必须这样：`VirtualCity` 的节点平移达 **751.4**，float32 在该量级的分辨率就是 **9.0e-05**，
用固定 1e-5 判"节点矩阵误差"只会得到假阳性（实测 6.104e-05 = 0.7 ULP）。
[A]/[B] 的报错信息会打印该节点的 `|M|max`，便于判断是真错还是量级问题。

## [C] 顶点级检查的前提（对真实资产很关键）

只有同时满足以下条件的节点才参与顶点级对拍，其余**跳过并计数**（不是失败）：

- **非蒙皮**：primitive 含 `JOINTS_0`/`WEIGHTS_0` 时顶点由图元 skin 矩阵驱动，与节点世界矩阵无关
  （`RecursiveSkeletons` 若不跳过会差 **60.0**）。
- **不采样**：顶点**全量**读入；超过 4M 点时整节点跳过。截断顶点集会让 AABB 缺极值
  （`ABeautifulGame` 的 `King_B` 有 28,901 点，只读 4096 点即差 **3.194e-03**）。
- **单 primitive、无子节点**、POSITION 为未压缩 FLOAT VEC3、非 sparse。
- **该 `boundsIndex` 只被本节点引用**：被多节点共享时"本节点世界 AABB"前提不成立（单独计数）。

## 输入预筛

- **只支持 glTF 2.0**：先校验 `asset.version`，1.x 直接报错（不喂给转换器）。
- **`extensionsRequired` 非空 ⇒ `[SKIP]`**：转换器用的是裸 `fastgltf::Parser{}`（无 `enableExtensions`），
  这类资产在解析阶段就失败，**而且失败路径不干净**（实测挂住 >120s 或 abort rc=3，且不打印
  `[Error] Conversion failed:`）。官方样本 142 个里有 14 个属于这一类，跳过它们可避免每次白等 300s。


## 导出格式约定（写检查代码时容易踩）

- `matrixTable[i]`：**列主序** 16 个浮点（`m[c][r]` → `flat[c*4+r]`）。
- `trsTable[i].r`：**`[w, x, y, z]`** —— 与 glTF 文件里的 `[x, y, z, w]` **相反**，读的时候必须换序。
- 产物路径：`<outdir>/<模型名>/<模型名>.StaticMesh/<场景名>.Scene.json`
  （**场景名**决定 json/scene 文件名，与输入文件名不一定相同）。
- 顶点数据被旋转后，`boundsTable` 是**世界空间** AABB（`worldM · (R·v)`），不是节点局部。

## 已知的"正常差异"（不要当回归）

- `trsTable`/`matrixTable`/AABB 允许 float32 末位差（实测 ≤1e-6；大尺度资产按量级放大）。
- `*.Scene.json` 的**字节数与文本**可能变化（浮点十进制表示长度不同），键集合与元素数应一致。
- **OBB 轴向量**在旋转对称形状（cone/cylinder）上可以大幅不同：垂直于对称轴的平面内存在
  规范自由度（对称轴分量逐位相同、`obbHalf` 只有末位差即可确认）。
- **转换器输出不是逐次确定的**：同一二进制度连跑两次同一模型，20 个产物文件里**7 个不同**。
  稳定的是 `trsTable`/`matrixTable`/`nodes`/`rootNodes`/AABB/`sphere`/`obbHalf` 与顶点数据；
  不稳定的是 `boundsTable` 的 **OBB 轴**（Δ 最大 2.0）与 `obbCenter`（~1e-6，`math/OBB.cpp`
  的并行穷举朝向搜索在平面/对称形状上大量 tie，赢家由线程写入顺序决定），并连带
  `*.geometry`（内嵌 BoundingVolumes）与 `*.scene` 的字节。
  ⇒ **回归比对只能按字段做**，且只对变换/结构表要求 0 差；不要用字节比对当判据。
- **无变换键的节点仍会占一行 `trsTable`**（实测 120/120）：其 TRS 是"单位变换 + 1 ULP"
  （`max|t|=0`、`max|q−1|=0`、`max|s−1|=1.19e-07`= 2⁻²³），因为导入边界的共轭 `R·M·R⁻¹` +
  分解必然留下 ULP 级误差，而 `TRS::empty()` 是**精确比较**。这与删 `NodeTransform::Type` 无关
  （改动前后逐位相同），`[F]` 只要求它 ≤ 容差。想让它们真正零成本需在导入边界做**带容差的恒等规约**。
