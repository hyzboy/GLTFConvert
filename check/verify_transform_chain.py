#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
verify_transform_chain.py —— GLTFConvert 变换链检查（节点局部变换 + Y-up→Z-up 转换）

【独立使用】只依赖 Python 3 标准库，只需要一个 GLTFConvert 可执行文件（CLI）。
    python verify_transform_chain.py --exe <GLTFConvert.exe> [--models a.glb b.gltf] [--keep]
    python verify_transform_chain.py --exe <GLTFConvert.exe> --quick          # 只跑合成用例
    python verify_transform_chain.py --exe <...> --report tp_report.json     # 额外产出机器可读结果

【CMake/ctest】配置时打开选项即可注册为测试：
    cmake -DGLTF_BUILD_TRANSFORM_CHECK=ON ...      # 然后 ctest -R GLTFConvertTransformChain

------------------------------------------------------------ 检查什么 ------------------------------------------------------------

判据（唯一正确基准）：顶点被 Rx(+90°) 旋转 v' = R·v，因此节点局部变换必须满足
**M' = R · M_raw · R⁻¹**（这样 M'·v' = R·(M·v)，全链路恰好一次 R）。

合成用例（脚本自己生成，不依赖外部资产）：
    ok.gltf        —— 必须转换成功且逐节点精确：
                     TRS 均匀 / TRS 非均匀+旋转 / matrix 非均匀 / matrix 镜像(det<0) /
                     TRS 镜像 / matrix 单轴退化(scale=0) / 多根
    bad_shear.gltf —— 含剪切：必须**转换失败**（fail-fast）且错误信息点名该节点
    bad_flat.gltf  —— 两轴退化（矩阵塌成平面）：必须转换失败

每个成功用例检查四层：
    [A] 节点局部：导出 TRS 展开 vs R·M_raw·R⁻¹
    [B] 导出自洽：matrixTable 的 localM vs 同节点的 TRS 展开
    [C] 顶点级  ：叶子节点 boundsTable 的 AABB vs worldM·(R·v)（证明顶点只被旋转一次）
    [D] 四元数  ：导出 TRS 的 |q| 必须 ≈ 1（非单位四元数 = 分解出错）
另外 [E]：镜像用例必须**保留镜像**（导出缩放的 det<0），否则是"镜像被静默丢弃"。

退出码：0 = 全部通过；2 = 有检查失败；3 = 环境/用法错误（找不到 exe、转换异常等）。
"""

import argparse
import base64
import json
import math
import os
import re
import shutil
import struct
import subprocess
import sys
import tempfile

DEFAULT_TOLERANCE = 1e-5          # 节点矩阵逐元素容差（绝对项；相对项按数值量级缩放，见 tolerance_for）
DEFAULT_BOUNDS_TOLERANCE = 1e-3   # 顶点级 AABB 容差（绝对项）
DEFAULT_REL_TOLERANCE = 1e-5      # 相对容差：float32 在 |x| 量级上的分辨率 ≈ 1.2e-7·|x|
QUATERNION_TOLERANCE = 1e-3       # |q| 与 1 的容差
MAX_CHECK_POINTS = 4000000        # 顶点级检查的顶点数上限：超过则**跳过**（不允许采样，采样会让 AABB 失真）

SYNTH_VERTICES = [(0.0, 0.0, 0.0), (1.0, 0.0, 0.0), (0.0, 2.0, 0.0)]


# ───────────────────────────────────────────────── 4x4 / quat（列主序 m[col][row]） ──
def m_ident():
    return [[1.0, 0, 0, 0], [0, 1.0, 0, 0], [0, 0, 1.0, 0], [0, 0, 0, 1.0]]


def m_flat(m):
    return [m[c][r] for c in range(4) for r in range(4)]


def m_from_flat(flat):
    assert len(flat) == 16, len(flat)
    return [[float(flat[c * 4 + r]) for r in range(4)] for c in range(4)]


def m_mul(a, b):
    return [[sum(a[k][r] * b[c][k] for k in range(4)) for r in range(4)] for c in range(4)]


def q_to_m(x, y, z, w):
    n = math.sqrt(x * x + y * y + z * z + w * w) or 1.0
    x, y, z, w = x / n, y / n, z / n, w / n
    return [
        [1 - 2 * (y * y + z * z), 2 * (x * y + w * z), 2 * (x * z - w * y), 0.0],
        [2 * (x * y - w * z), 1 - 2 * (x * x + z * z), 2 * (y * z + w * x), 0.0],
        [2 * (x * z + w * y), 2 * (y * z - w * x), 1 - 2 * (x * x + y * y), 0.0],
        [0.0, 0.0, 0.0, 1.0],
    ]


def m_trs(t, r_xyzw, s):
    m = q_to_m(*r_xyzw) if r_xyzw else m_ident()
    for c in range(3):
        for row in range(4):
            m[c][row] *= s[c]
    m[3][0], m[3][1], m[3][2] = float(t[0]), float(t[1]), float(t[2])
    return m


def m_det3(m):
    a, b, c = m[0], m[1], m[2]
    return (a[0] * (b[1] * c[2] - b[2] * c[1])
            - a[1] * (b[0] * c[2] - b[2] * c[0])
            + a[2] * (b[0] * c[1] - b[1] * c[0]))


def m_maxdiff(a, b):
    return max(abs(a[c][r] - b[c][r]) for c in range(4) for r in range(4))


RX90 = q_to_m(math.sin(math.pi / 4), 0.0, 0.0, math.cos(math.pi / 4))
RX90_INV = [[RX90[r][c] for r in range(4)] for c in range(4)]      # 纯旋转的逆 = 转置


def conj_zup(m):
    """M' = R · M · R⁻¹"""
    return m_mul(RX90, m_mul(m, RX90_INV))


def rot_deg(angle, axis=(1.0, 0.0, 0.0)):
    half = math.radians(angle) * 0.5
    s = math.sin(half)
    return (axis[0] * s, axis[1] * s, axis[2] * s, math.cos(half))


# ────────────────────────────────────────────────────────── 合成 glTF 生成 ──
def _write_gltf(path, nodes, roots):
    pos = b"".join(struct.pack("<3f", *v) for v in SYNTH_VERTICES)
    idx = struct.pack("<3H", 0, 1, 2)
    buf = pos + idx

    doc = {
        "asset": {"version": "2.0", "generator": "GLTFConvert transform-chain check"},
        "scene": 0,
        "scenes": [{"nodes": list(roots)}],
        "nodes": nodes,
        "meshes": [{"name": "tri", "primitives": [
            {"attributes": {"POSITION": 0}, "indices": 1, "mode": 4}]}],
        "buffers": [{"byteLength": len(buf),
                     "uri": "data:application/octet-stream;base64,"
                            + base64.b64encode(buf).decode("ascii")}],
        "bufferViews": [
            {"buffer": 0, "byteOffset": 0, "byteLength": len(pos), "target": 34962},
            {"buffer": 0, "byteOffset": len(pos), "byteLength": len(idx), "target": 34963},
        ],
        "accessors": [
            {"bufferView": 0, "componentType": 5126, "count": 3, "type": "VEC3",
             "min": [0.0, 0.0, 0.0], "max": [1.0, 2.0, 0.0]},
            {"bufferView": 1, "componentType": 5123, "count": 3, "type": "SCALAR"},
        ],
    }

    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", encoding="utf-8") as f:
        json.dump(doc, f, indent=2)


def make_ok_scene(path):
    """必须成功且逐节点精确：覆盖 TRS/matrix、非均匀、镜像、单轴退化、多根。"""
    n0 = {"name": "root", "mesh": 0, "children": [1, 2, 3, 4, 5, 6]}
    n1 = {"name": "trs_uniform", "mesh": 0,
          "translation": [1.0, 2.0, 3.0], "rotation": list(rot_deg(30.0, (0, 1, 0))),
          "scale": [1.0, 1.0, 1.0]}
    n2 = {"name": "trs_nonuniform", "mesh": 0,                 # 旧代码在此偏差 0.383
          "translation": [4.0, 0.0, 0.0], "rotation": list(rot_deg(40.0)),
          "scale": [2.0, 0.5, 1.0]}
    n3 = {"name": "matrix_nonuniform", "mesh": 0,              # 旧代码在此偏差 0.500
          "matrix": m_flat(m_trs([5.0, 0.0, 0.0], rot_deg(30.0, (0, 0, 1)), [2.0, 0.5, 1.0]))}
    n4 = {"name": "matrix_mirror", "mesh": 0,                  # 旧代码在此偏差 1.243（镜像丢失）
          "matrix": m_flat(m_trs([0.0, 1.0, 0.0], rot_deg(45.0), [-2.0, 0.5, 1.0]))}
    n5 = {"name": "matrix_zero_axis", "mesh": 0,               # 单轴退化（scale=0 隐藏部件）
          "matrix": m_flat(m_trs([0.0, 0.0, 3.0], None, [0.0, 1.0, 1.0]))}
    n6 = {"name": "trs_mirror", "mesh": 0,
          "translation": [0.0, 0.0, 4.0], "rotation": [0.0, 0.0, 0.0, 1.0],
          "scale": [-1.0, 1.0, 1.0]}
    n7 = {"name": "root2", "mesh": 0,                          # 多根
          "translation": [0.0, 5.0, 0.0]}

    _write_gltf(path, [n0, n1, n2, n3, n4, n5, n6, n7], [0, 7])
    return path


def make_bad_shear_scene(path):
    """含剪切：TRS 结构无法表示 ⇒ 必须 fail-fast。"""
    shear = m_trs([0.0, 0.0, 3.0], None, [1.0, 1.0, 1.0])
    shear[0][1] += 0.5
    nodes = [
        {"name": "root", "mesh": 0, "children": [1]},
        {"name": "matrix_shear", "mesh": 0, "matrix": m_flat(shear)},
    ]
    _write_gltf(path, nodes, [0])
    return path


def make_bad_flat_scene(path):
    """两轴退化（矩阵塌成平面）：无法确定旋转 ⇒ 必须 fail-fast。"""
    nodes = [
        {"name": "root", "mesh": 0, "children": [1]},
        {"name": "matrix_two_degenerate", "mesh": 0,
         "matrix": m_flat(m_trs([0.0, 0.0, 0.0], None, [1.0, 0.0, 0.0]))},
    ]
    _write_gltf(path, nodes, [0])
    return path


# ─────────────────────────────────────────────────────── 读取 raw / 导出节点 ──
def load_raw_nodes(gltf_path):
    """读取原始节点表；同时返回 glTF 文档与 buffer 字节（供顶点级检查解码 POSITION）。

    只接受 glTF 2.0（fastgltf 与转换器同口径）；1.x 或畸形产物在这里给出明确错误，
    而不是抛 traceback（实测 res/model/color_teapot_spheres.gltf 是 assimp 导出的 glTF 1.0）。
    """
    bin_chunk = b""
    base = os.path.dirname(os.path.abspath(gltf_path))

    if gltf_path.lower().endswith(".glb"):
        raw = open(gltf_path, "rb").read()
        if raw[:4] != b"glTF":
            raise Failure("不是 GLB 文件（缺少 glTF magic）：%s" % gltf_path)
        off, doc = 12, None
        while off + 8 <= len(raw):
            clen, ctype = struct.unpack_from("<II", raw, off)
            off += 8
            if ctype == 0x4E4F534A:                                  # 'JSON'
                doc = json.loads(raw[off:off + clen].decode("utf-8"))
            elif ctype == 0x004E4942:                                # 'BIN'
                bin_chunk = raw[off:off + clen]
            off += clen
        if doc is None:
            raise Failure("GLB 内没有 JSON chunk：%s" % gltf_path)
    else:
        doc = json.load(open(gltf_path, encoding="utf-8"))

    version = str(doc.get("asset", {}).get("version", ""))
    if not version.startswith("2"):
        raise Failure("只支持 glTF 2.0：%s 的 asset.version=%r（fastgltf 不支持 1.x）"
                      % (os.path.basename(gltf_path), version or "缺失"))

    bufs = doc.get("buffers", [])
    if not isinstance(bufs, list) or any(not isinstance(b, dict) for b in bufs):
        raise Failure("buffers 不是 glTF 2.0 形态（疑似 1.0/畸形产物）：%s" % gltf_path)

    buffers = []
    for b in bufs:
        uri = b.get("uri")
        if not uri:
            buffers.append(bin_chunk)                                # GLB 的内嵌 buffer
        elif uri.startswith("data:"):
            buffers.append(base64.b64decode(uri.split(",", 1)[1]))
        else:
            p = uri if os.path.isabs(uri) else os.path.join(base, uri)
            buffers.append(open(p, "rb").read() if os.path.isfile(p) else b"")

    out = []
    for n in doc.get("nodes", []):
        if "matrix" in n:
            m, kind, trs = m_from_flat(n["matrix"]), "matrix", None
        elif any(k in n for k in ("translation", "rotation", "scale")):
            t = n.get("translation", [0.0, 0.0, 0.0])
            r = n.get("rotation", [0.0, 0.0, 0.0, 1.0])
            s = n.get("scale", [1.0, 1.0, 1.0])
            m, kind, trs = m_trs(t, tuple(r), s), "trs", (t, tuple(r), s)
        else:
            m, kind, trs = m_ident(), "none", ([0.0] * 3, (0, 0, 0, 1.0), [1.0] * 3)
        out.append({"name": n.get("name", ""), "M": m, "kind": kind, "trs": trs,
                    "det": m_det3(m), "mesh": n.get("mesh"),
                    "has_children": bool(n.get("children"))})
    return out, doc, buffers


def m_mag(m):
    """矩阵元素最大绝对值。绝对容差必须按它缩放：float32 在 |x|≈751 处的分辨率就是 9e-5，
    对城市/大尺度资产（VirtualCity 节点平移 751.4）用固定 1e-5 判误差只会得到假阳性。"""
    return max(abs(m[c][r]) for c in range(4) for r in range(4))


def tolerance_for(abs_tol, rel_tol, *vals):
    """绝对 + 相对容差：tol = abs_tol + rel_tol · max|vals|（vals 为参与比较的量）"""
    return abs_tol + rel_tol * max([abs(v) for v in vals] + [0.0])


def node_positions(doc, buffers, raw_node, max_count=MAX_CHECK_POINTS):
    """节点自身网格第一个 primitive 的 POSITION（FLOAT VEC3）。不支持的类型返回 None（跳过检查）。

    只对"单一 primitive、无子节点"的节点做顶点级检查，避免 bounds 是子树并集时误判。
    **蒙皮网格（JOINTS_0/WEIGHTS_0）返回 None**：蒙皮顶点由图元 skin 矩阵驱动，
    与节点自己的世界矩阵无关（RecursiveSkeletons 就是这种情况）。
    **不采样**：只读全量顶点，超过 max_count 直接跳过——截断顶点集会让 AABB 缺极值，
    表现为"世界 AABB 对不上"的假阳性（ABeautifulGame King_B 28901 点只读 4096 点即差 3.2e-3）。
    """
    if raw_node.get("mesh") is None or raw_node.get("has_children"):
        return None

    mesh = doc["meshes"][raw_node["mesh"]]
    if len(mesh.get("primitives", [])) != 1:
        return None

    attrs = mesh["primitives"][0].get("attributes", {})
    if "JOINTS_0" in attrs or "WEIGHTS_0" in attrs:
        return None                                                  # 蒙皮：不受节点世界矩阵支配

    acc_idx = attrs.get("POSITION")
    if acc_idx is None:
        return None

    acc = doc["accessors"][acc_idx]
    if acc.get("componentType") != 5126 or acc.get("type") != "VEC3":
        return None                                                  # 只支持 FLOAT VEC3
    if "sparse" in acc:
        return None
    if acc["count"] > max_count:
        return None                                                  # 超大网格：跳过（不采样）

    bv = doc["bufferViews"][acc["bufferView"]]
    buf = buffers[bv.get("buffer", 0)]
    stride = bv.get("byteStride") or 12
    off = bv.get("byteOffset", 0) + acc.get("byteOffset", 0)

    return [struct.unpack_from("<3f", buf, off + i * stride) for i in range(acc["count"])]


def load_export_nodes(json_path):
    """导出 JSON 的字段约定：matrixTable[i] = 列主序 16 浮点；trsTable 的 r = [w,x,y,z]。"""
    j = json.load(open(json_path, encoding="utf-8"))
    names = j.get("nameTable", [])
    trs_tbl, mat_tbl = j.get("trsTable", []), j.get("matrixTable", [])
    out = []
    for n in j.get("nodes", []):
        e = {"index": n["index"],
             "name": names[n["nameIndex"]] if "nameIndex" in n else "",
             "has_children": bool(n.get("children")),
             "boundsIndex": n.get("boundsIndex"),
             "localM": m_from_flat(mat_tbl[n["localM"]]) if "localM" in n else None,
             "worldM": m_from_flat(mat_tbl[n["worldM"]]) if "worldM" in n else None}
        if "trs" in n:
            t = trs_tbl[n["trs"]]
            rw = t["r"]                                              # [w,x,y,z]
            e["trs"] = t
            e["trsM"] = m_trs(t["t"], (rw[1], rw[2], rw[3], rw[0]), t["s"])
            e["quat_len"] = math.sqrt(sum(v * v for v in rw))
        out.append(e)
    return out, j


def safe_name(s):
    """把显示名变成合法的目录名（Windows 路径不允许 : * ? " < > | 等）"""
    return "".join(ch if (ch.isalnum() or ch in "._-") else "_" for ch in s)


# ── 与转换器 `gltf/import/GLTFExtensions.cpp` 的清单保持一致 ────────────────────
# 转换器会启用解析的必需扩展（"能消费或可安全忽略"）
ENABLED_EXT = {
    "KHR_texture_transform", "KHR_materials_unlit", "KHR_materials_ior",
    "KHR_materials_specular", "KHR_materials_iridescence", "KHR_materials_volume",
    "KHR_materials_transmission", "KHR_materials_clearcoat",
    "KHR_materials_emissive_strength", "KHR_materials_sheen",
    "KHR_materials_anisotropy", "KHR_materials_dispersion",
    "KHR_materials_diffuse_transmission", "KHR_materials_variants",
    "KHR_lights_punctual", "KHR_mesh_quantization", "EXT_texture_webp",
    "KHR_texture_basisu", "MSFT_texture_dds", "GODOT_single_root",
}
# 其中转换器**真正实现**了效果的（其余会打印"效果未实现"告警）
HANDLED_EXT = {"KHR_materials_unlit", "KHR_mesh_quantization",
               "KHR_materials_variants", "GODOT_single_root"}


def classify_required(gltf):
    """返回 (会被拒的必需扩展, 启用但效果未实现的必需扩展)。空 ⇒ 可转换。"""
    try:
        _raw, doc, _bufs = load_raw_nodes(gltf)
    except Exception:
        return [], []
    req = list(doc.get("extensionsRequired", []))
    rejected = [e for e in req if e not in ENABLED_EXT]
    unhandled = [e for e in req if e in ENABLED_EXT and e not in HANDLED_EXT]
    return rejected, unhandled


def find_scene_jsons(outdir):
    """产物里**全部**场景 JSON：`<outdir>/<模型名>/<模型名>.StaticMesh/<场景名>.json`。

    A4 起转换器把源文件的**每个场景**都导出（多场景命名 `scene<N>`，单场景沿用源场景名），
    所以这里返回列表；用"含 nodes/geometries"判定是不是场景文件（贴图清单等 json 排除）。
    """
    out = []
    for dp, _dns, fns in os.walk(outdir):
        for f in fns:
            if not f.endswith(".json"):
                continue
            p = os.path.join(dp, f)
            try:
                with open(p, encoding="utf-8") as fp:
                    j = json.load(fp)
            except Exception:
                continue
            if isinstance(j, dict) and "nodes" in j and ("boundsTable" in j or "geometries" in j):
                out.append(p)
    return sorted(out)


def scene_index_of(jpath, doc):
    """导出文件 → 源场景序号。多场景命名是 `<base>.scene<N>` ⇒ N；
    单场景命名沿用源场景名 ⇒ 视为源文件的默认场景（glTF `scene` 字段）。"""
    stem = os.path.splitext(os.path.basename(jpath))[0]
    m = re.search(r"scene(\d+)$", stem)
    return int(m.group(1)) if m else doc.get("scene", 0)


# ──────────────────────────────────────────────────────────────── 检查用例 ──
class Failure(Exception):
    pass


def check_good_scene(name, gltf, exe, work, tolerance, bounds_tol, config, dname, timeout,
                     rel_tolerance=DEFAULT_REL_TOLERANCE):
    # 先校验源文件（可读 + glTF 2.0），再转换：避免把"不支持的输入"变成一次几分钟的挂起
    raw, doc, buffers = load_raw_nodes(gltf)

    outdir = os.path.join(work, "out_" + dname)
    rc, log = run_convert(exe, gltf, outdir, config, timeout)
    if rc != 0:
        raise Failure("转换应成功但 rc=%d\n%s" % (rc, log[-1500:]))

    jpaths = find_scene_jsons(outdir)
    if not jpaths:
        raise Failure("找不到导出 JSON（%s）" % outdir)

    scenes = doc.get("scenes", [])
    if scenes:
        # A4：源文件的**每个**场景都必须有一份产物（多场景命名 `scene<N>`），缺一即失败
        got = sorted(scene_index_of(p, doc) for p in jpaths)
        if len(jpaths) != len(scenes) or got != list(range(len(scenes))):
            raise Failure("[场景] 导出场景数 %d ≠ 源场景数 %d（导出序号 %s）"
                          % (len(jpaths), len(scenes), got))

    total = {"nodes": len(raw), "scenes_exported": len(jpaths),
             "leaf_bounds": 0, "bounds_skipped": 0, "bounds_shared_skipped": 0,
             "worst_A": 0.0, "worst_B": 0.0, "worst_C": 0.0, "worst_q": 0.0,
             "worst_node": ""}
    no_xform, no_xform_residual = set(), set()
    for jp in jpaths:
        st = check_exports_of_scene(jp, raw, doc, buffers, scene_index_of(jp, doc),
                                    tolerance, bounds_tol, rel_tolerance)
        for k in ("leaf_bounds", "bounds_skipped", "bounds_shared_skipped"):
            total[k] += st[k]
        for k in ("worst_A", "worst_B", "worst_C", "worst_q"):
            if st[k] > total[k]:
                total[k] = st[k]
                if k == "worst_A":
                    total["worst_node"] = st["worst_node"]
        no_xform |= set(st["no_xform_nodes"])
        no_xform_residual |= set(st["no_xform_residual_nodes"])

    total["no_xform"] = len(no_xform)
    total["no_xform_residual"] = len(no_xform_residual)
    total["nodes_unreachable"] = (len(raw) - len(reachable_union(raw, doc, scenes))) if scenes else 0
    return total


def reachable_union(raw, doc, scenes):
    """所有场景可达节点的**并集**（报告用：避免多场景逐场景重复计数）"""
    if not scenes:
        return set(range(len(raw)))
    seen = set()
    for s in scenes:
        stack = list(s.get("nodes", []))
        while stack:
            i = stack.pop()
            if i in seen or i < 0 or i >= len(raw):
                continue
            seen.add(i)
            stack.extend(doc["nodes"][i].get("children", []))
    return seen


def check_exports_of_scene(jpath, raw, doc, buffers, scene_idx, tolerance, bounds_tol, rel_tolerance):
    """校验**单个**导出场景：该场景可达的源节点逐个对拍 [A]–[F]，再对拍 [C] 世界 AABB。"""
    exp_nodes, exp_json = load_export_nodes(jpath)
    by_index = {e["index"]: e for e in exp_nodes}

    # 源节点可达性：**本场景**可达的节点必须都在这份产物里；其它场景的节点不在本文件里 ⇒ 跳过。
    # 可达却缺失才是 bug。
    scenes = doc.get("scenes", [])
    reachable = set()
    if scenes and 0 <= scene_idx < len(scenes):
        stack = list(scenes[scene_idx].get("nodes", []))
    else:
        stack = list(range(len(raw)))                    # 无场景信息：按"全部可达"处理（保守）
    while stack:
        i = stack.pop()
        if i in reachable or i < 0 or i >= len(raw):
            continue
        reachable.add(i)
        stack.extend(doc["nodes"][i].get("children", []))

    worst_a = worst_b = worst_q = 0.0
    loser = ""
    no_xform_nodes, no_xform_residual_nodes = [], []

    for i, rn in enumerate(raw):
        if scenes and i not in reachable:                # 非本场景可达：不在这份产物里
            continue
        e = by_index.get(i)
        if e is None:
            raise Failure("节点 %d (%s) 未出现在导出节点表中" % (i, rn["name"]))

        expect = conj_zup(rn["M"])
        actual = e.get("trsM") or e["localM"]

        tol_a = tolerance_for(tolerance, rel_tolerance, m_mag(expect))          # [A]
        a = m_maxdiff(expect, actual)
        if a > worst_a:
            worst_a, loser = a, rn["name"]
        if a > tol_a:
            raise Failure("[A] 节点 %d (%s) 局部变换误差 %.3e > %.1e（容差随 |M|max=%.4g 缩放）"
                          "（旧实现就是在这里歪掉）"
                          % (i, rn["name"], a, tol_a, m_mag(expect)))

        b = m_maxdiff(e["localM"], actual)                 # [B]
        worst_b = max(worst_b, b)
        tol_b = tolerance_for(tolerance, rel_tolerance, m_mag(e["localM"]))
        if b > tol_b:
            raise Failure("[B] 节点 %d (%s) 的 matrixTable 与 TRS 展开不一致：%.3e > %.1e"
                          % (i, rn["name"], b, tol_b))

        if rn["kind"] == "none":                            # [F] 源文件无任何变换键的节点
            no_xform_nodes.append(i)
            if "trs" in e:
                # 允许"仅剩数值残差"的 TRS：导入边界的共轭 R·M·R⁻¹ + 分解会引入 ~1 ULP 误差
                # （实测 120/120 个无变换节点的 |s-1| = 1.19e-07 = 2^-23，t 与 r 精确为 0），
                # 使**精确比较**的 TRS::empty() 判不出单位变换 ⇒ 本该零成本的节点仍占 trsTable 一行。
                # 这里只要求它确实是单位变换（残差在容差内），并把这类节点计数出来。
                no_xform_residual_nodes.append(i)
                resid = m_maxdiff(e["trsM"], m_ident())
                if resid > tolerance:
                    raise Failure("[F] 节点 %d (%s) 源文件无任何变换键，但导出 TRS 与单位矩阵差 "
                                  "%.3e > %.1e（不是数值残差，是真错）" % (i, rn["name"], resid, tolerance))
            d_id = m_maxdiff(e["localM"], m_ident())
            if d_id > tolerance:
                raise Failure("[F] 节点 %d (%s) 源文件无任何变换键，导出 localM 不是单位矩阵（差 %.3e）"
                              % (i, rn["name"], d_id))

        if "quat_len" in e:                                # [D]
            d = abs(e["quat_len"] - 1.0)
            worst_q = max(worst_q, d)
            if d > QUATERNION_TOLERANCE:
                raise Failure("[D] 节点 %d (%s) 的 TRS 四元数非单位：|q|=%.6f（分解出错）"
                              % (i, rn["name"], e["quat_len"]))

        if rn["det"] < 0.0:                                # [E] 镜像必须被保留
            if m_det3(actual) > 0.0:
                raise Failure("[E] 节点 %d (%s) 是镜像（原始 det=%.3f）但导出后 det=%.3f"
                              " ⇒ 镜像被丢弃" % (i, rn["name"], rn["det"], m_det3(actual)))

    # [C] 顶点级：节点自身网格的顶点 × R 再乘世界矩阵，与 boundsTable 的 AABB 对拍
    #     （同时证明顶点只被旋转了一次、且世界矩阵与顶点同坐标系）
    #     前提：该 bounds 条目是"本节点的世界 AABB"。同一 boundsIndex 被多个节点共享时前提不成立 ⇒ 跳过。
    bcount = {}
    for e in exp_nodes:
        if e.get("boundsIndex") is not None:
            bcount[e["boundsIndex"]] = bcount.get(e["boundsIndex"], 0) + 1

    worst_c = 0.0
    checked = skipped = shared_skipped = 0
    for i, rn in enumerate(raw):
        e = by_index.get(i)
        if e is None or e.get("boundsIndex") is None or e.get("worldM") is None \
                or e.get("has_children"):
            continue
        if bcount.get(e["boundsIndex"], 0) > 1:
            shared_skipped += 1
            continue

        raw_pts = node_positions(doc, buffers, rn)
        if not raw_pts:
            skipped += 1
            continue

        w = e["worldM"]
        pts = []
        for v in raw_pts:
            rv = (v[0], -v[2], v[1])                      # R = Rx(+90°)
            pts.append(tuple(w[0][k] * rv[0] + w[1][k] * rv[1] + w[2][k] * rv[2] + w[3][k]
                             for k in range(3)))
        lo = [min(p[k] for p in pts) for k in range(3)]
        hi = [max(p[k] for p in pts) for k in range(3)]
        b = exp_json["boundsTable"][e["boundsIndex"]]
        d = max(max(abs(b["aabbMin"][k] - lo[k]) for k in range(3)),
                max(abs(b["aabbMax"][k] - hi[k]) for k in range(3)))
        tol_c = tolerance_for(bounds_tol, rel_tolerance,
                              max(abs(x) for x in list(b["aabbMin"]) + list(b["aabbMax"])))
        worst_c = max(worst_c, d)
        checked += 1
        if d > tol_c:
            raise Failure("[C] 节点 %d (%s) 的世界 AABB 与 worldM·(R·v) 不符：%.3e > %.1e"
                          % (e["index"], e["name"], d, tol_c))

    return {"leaf_bounds": checked, "bounds_skipped": skipped,
            "bounds_shared_skipped": shared_skipped,
            "no_xform_nodes": no_xform_nodes, "no_xform_residual_nodes": no_xform_residual_nodes,
            "worst_A": worst_a, "worst_B": worst_b, "worst_C": worst_c, "worst_q": worst_q,
            "worst_node": loser}


def check_bad_scene(name, gltf, exe, work, expect_node, config, dname, timeout):
    outdir = os.path.join(work, "out_" + dname)
    rc, log = run_convert(exe, gltf, outdir, config, timeout)
    if rc == 0:
        raise Failure("含不可表示变换的输入应当 fail-fast，但转换返回 rc=0（错误被吞掉）")
    if expect_node not in log:
        raise Failure("转换失败但错误信息里没有点名节点 %r：\n%s" % (expect_node, log[-1200:]))
    return {"rc": rc}


def run_convert(exe, gltf, outdir, config, timeout=300):
    cmd = [exe, "--no-images", "--no-meshlet"]
    if config:
        cmd.append("--config=" + config)
    cmd += [gltf, outdir]
    try:
        p = subprocess.run(cmd, capture_output=True, timeout=timeout)
        log = (p.stdout or b"").decode("utf-8", "replace") + (p.stderr or b"").decode("utf-8", "replace")
        return p.returncode, log
    except subprocess.TimeoutExpired:
        raise Failure("转换超时（%ds）：%s" % (timeout, " ".join(cmd)))


# ───────────────────────────────────────────────────────────────────── main ──
def main():
    ap = argparse.ArgumentParser(description="GLTFConvert 变换链检查（节点局部变换 + Y-up→Z-up）")
    ap.add_argument("--exe", required=True, help="GLTFConvert 可执行文件（CLI）")
    ap.add_argument("--models", nargs="*", default=[],
                    help="额外的真实模型（.gltf/.glb），要求转换成功且逐节点误差在容差内")
    ap.add_argument("--work", default=None, help="工作目录（默认用临时目录，--keep 可保留）")
    ap.add_argument("--keep", action="store_true", help="保留工作目录（排查用）")
    ap.add_argument("--quick", action="store_true", help="只跑合成用例，跳过 --models")
    ap.add_argument("--config", default=None, help="传给转换器的 --config=<ini>")
    ap.add_argument("--tolerance", type=float, default=DEFAULT_TOLERANCE)
    ap.add_argument("--bounds-tolerance", type=float, default=DEFAULT_BOUNDS_TOLERANCE)
    ap.add_argument("--rel-tolerance", type=float, default=DEFAULT_REL_TOLERANCE,
                    help="相对容差（容差 = 绝对项 + 相对项·max|量级|），默认 %g" % DEFAULT_REL_TOLERANCE)
    ap.add_argument("--report", default=None, help="把机器可读结果写到该 JSON 文件")
    ap.add_argument("--timeout", type=int, default=300, help="单次转换的超时秒数（默认 300）")
    args = ap.parse_args()

    exe = os.path.abspath(args.exe)
    if not os.path.isfile(exe):
        print("找不到转换器可执行文件：%s" % exe)
        return 3

    work = args.work or tempfile.mkdtemp(prefix="gltf_tfcheck_")
    os.makedirs(work, exist_ok=True)

    synth_dir = os.path.join(work, "synth")
    cases = [
        # (显示名, gltf, 类型, 期望, 目录安全名)
        ("ok", make_ok_scene(os.path.join(synth_dir, "ok.gltf")), "good", None, "ok"),
        ("bad_shear", make_bad_shear_scene(os.path.join(synth_dir, "bad_shear.gltf")),
         "bad", "matrix_shear", "bad_shear"),
        ("bad_flat", make_bad_flat_scene(os.path.join(synth_dir, "bad_flat.gltf")),
         "bad", "matrix_two_degenerate", "bad_flat"),
    ]
    if not args.quick:
        for m in args.models:
            label = "model:" + os.path.basename(m)
            cases.append((label, os.path.abspath(m), "good", None, safe_name(label)))

    print("=" * 78)
    print("GLTFConvert transform-chain check")
    print("  exe      : %s" % exe)
    print("  work     : %s%s" % (work, "" if args.keep else "  (临时，结束即删；--keep 保留)"))
    print("  容差     : 节点 %.1e(+%.1e·|M|) / AABB %.1e(+相对) / |q| %.1e" %
          (args.tolerance, args.rel_tolerance, args.bounds_tolerance, QUATERNION_TOLERANCE))
    print("=" * 78)

    report, failed, skipped_cases = {}, [], []
    for name, path, kind, expect, dname in cases:
        try:
            rejected, unhandled = classify_required(path) if kind == "good" else ([], [])
            if rejected:
                print("  [SKIP] %-26s 转换器无法转换的必需扩展：%s" % (name, ", ".join(rejected)))
                report[name] = {"result": "skip", "extensions": rejected}
                skipped_cases.append(name)
                continue
            note = None
            if unhandled:
                note = "        %-26s ⚠ 效果未实现（已启用解析）：%s" % ("", ", ".join(unhandled))
            if kind == "good":
                r = check_good_scene(name, path, exe, work, args.tolerance,
                                     args.bounds_tolerance, args.config, dname, args.timeout,
                                     rel_tolerance=args.rel_tolerance)
                print("  [PASS] %-26s 节点=%-3d 无变换=%-3d(残差占行%d) AABB=%-3d(跳过%d/共享%d) "
                      "max[A]=%.2e max[B]=%.2e max[C]=%.2e max||q|-1|=%.2e"
                      % (name, r["nodes"], r.get("no_xform", 0), r.get("no_xform_residual", 0),
                         r["leaf_bounds"], r.get("bounds_skipped", 0),
                         r.get("bounds_shared_skipped", 0),
                         r["worst_A"], r["worst_B"], r["worst_C"], r["worst_q"]))
            else:
                r = check_bad_scene(name, path, exe, work, expect, args.config, dname, args.timeout)
                print("  [PASS] %-28s fail-fast 生效（rc=%d，错误信息点名 %s）" % (name, r["rc"], expect))
            if note:
                print(note)
            report[name] = {"result": "pass", **r}
        except Failure as f:
            failed.append(name)
            report[name] = {"result": "fail", "reason": str(f)}
            print("  [FAIL] %-28s %s" % (name, f))

    print("-" * 78)
    n_pass = len(cases) - len(failed) - len(skipped_cases)
    tail = "，跳过 %d：%s" % (len(skipped_cases), ", ".join(skipped_cases)) if skipped_cases else ""
    if failed:
        print("CHECK RESULT: FAIL (%d/%d) -> %s%s" % (len(failed), n_pass, ", ".join(failed), tail))
    else:
        print("CHECK RESULT: PASS (%d/%d)%s  判据 M' = R·M_raw·R⁻¹" % (n_pass, n_pass, tail))

    if args.report:
        with open(args.report, "w", encoding="utf-8") as f:
            json.dump(report, f, indent=2, ensure_ascii=False)
        print("  报告已写入：%s" % args.report)

    if not args.keep and not args.work:
        shutil.rmtree(work, ignore_errors=True)

    return 2 if failed else 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except Failure as e:                      # 环境级失败
        print("环境错误：%s" % e)
        sys.exit(3)
