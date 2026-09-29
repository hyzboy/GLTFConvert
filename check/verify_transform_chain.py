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
import shutil
import struct
import subprocess
import sys
import tempfile

DEFAULT_TOLERANCE = 1e-5          # 节点矩阵逐元素容差（实测 float32 末位约 1e-6）
DEFAULT_BOUNDS_TOLERANCE = 1e-3   # 顶点级 AABB 容差（bounds 由转换器内部计算）
QUATERNION_TOLERANCE = 1e-3       # |q| 与 1 的容差

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


def node_positions(doc, buffers, raw_node, max_count=4096):
    """节点自身网格第一个 primitive 的 POSITION（FLOAT VEC3）。不支持的类型返回 None（跳过检查）。

    只对“单一 primitive、无子节点”的节点做顶点级检查，避免 bounds 是子树并集时误判。
    """
    if raw_node.get("mesh") is None or raw_node.get("has_children"):
        return None

    mesh = doc["meshes"][raw_node["mesh"]]
    if len(mesh.get("primitives", [])) != 1:
        return None

    acc_idx = mesh["primitives"][0].get("attributes", {}).get("POSITION")
    if acc_idx is None:
        return None

    acc = doc["accessors"][acc_idx]
    if acc.get("componentType") != 5126 or acc.get("type") != "VEC3":
        return None                                                  # 只支持 FLOAT VEC3
    if "sparse" in acc:
        return None

    bv = doc["bufferViews"][acc["bufferView"]]
    buf = buffers[bv.get("buffer", 0)]
    stride = bv.get("byteStride") or 12
    off = bv.get("byteOffset", 0) + acc.get("byteOffset", 0)

    pts = []
    for i in range(min(acc["count"], max_count)):
        pts.append(struct.unpack_from("<3f", buf, off + i * stride))
    return pts


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


def find_scene_json(outdir, src_name):
    """产物路径：<outdir>/<模型名>/<模型名>.StaticMesh/<场景名>.Scene.json（**场景名**决定文件名）"""
    cands = []
    for dp, _dns, fns in os.walk(outdir):
        for f in fns:
            if f.endswith(".json"):
                cands.append(os.path.join(dp, f))
    for c in cands:
        if c.endswith(".Scene.json"):
            return c
    return cands[0] if cands else None


# ──────────────────────────────────────────────────────────────── 检查用例 ──
class Failure(Exception):
    pass


def check_good_scene(name, gltf, exe, work, tolerance, bounds_tol, config, dname, timeout):
    # 先校验源文件（可读 + glTF 2.0），再转换：避免把"不支持的输入"变成一次几分钟的挂起
    raw, doc, buffers = load_raw_nodes(gltf)

    outdir = os.path.join(work, "out_" + dname)
    rc, log = run_convert(exe, gltf, outdir, config, timeout)
    if rc != 0:
        raise Failure("转换应成功但 rc=%d\n%s" % (rc, log[-1500:]))

    jpath = find_scene_json(outdir, os.path.splitext(os.path.basename(gltf))[0])
    if not jpath:
        raise Failure("找不到导出 JSON（%s）" % outdir)

    exp_nodes, exp_json = load_export_nodes(jpath)
    by_index = {e["index"]: e for e in exp_nodes}

    worst_a = worst_b = worst_q = 0.0
    loser = ""

    for i, rn in enumerate(raw):
        e = by_index.get(i)
        if e is None:
            raise Failure("节点 %d (%s) 未出现在导出节点表中" % (i, rn["name"]))

        expect = conj_zup(rn["M"])
        actual = e.get("trsM") or e["localM"]

        a = m_maxdiff(expect, actual)                      # [A]
        if a > worst_a:
            worst_a, loser = a, rn["name"]
        if a > tolerance:
            raise Failure("[A] 节点 %d (%s) 局部变换误差 %.3e > %.1e（旧实现就是在这里歪掉）"
                          % (i, rn["name"], a, tolerance))

        b = m_maxdiff(e["localM"], actual)                 # [B]
        worst_b = max(worst_b, b)
        if b > tolerance:
            raise Failure("[B] 节点 %d (%s) 的 matrixTable 与 TRS 展开不一致：%.3e"
                          % (i, rn["name"], b))

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
    worst_c = 0.0
    checked = skipped = 0
    for i, rn in enumerate(raw):
        e = by_index.get(i)
        if e is None or e.get("boundsIndex") is None or e.get("worldM") is None \
                or e.get("has_children"):
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
        worst_c = max(worst_c, d)
        checked += 1
        if d > bounds_tol:
            raise Failure("[C] 节点 %d (%s) 的世界 AABB 与 worldM·(R·v) 不符：%.3e"
                          % (e["index"], e["name"], d))

    return {"nodes": len(raw), "leaf_bounds": checked, "bounds_skipped": skipped,
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
    print("  容差     : 节点 %.1e / AABB %.1e / |q| %.1e" %
          (args.tolerance, args.bounds_tolerance, QUATERNION_TOLERANCE))
    print("=" * 78)

    report, failed = {}, []
    for name, path, kind, expect, dname in cases:
        try:
            if kind == "good":
                r = check_good_scene(name, path, exe, work, args.tolerance,
                                     args.bounds_tolerance, args.config, dname, args.timeout)
                print("  [PASS] %-28s 节点=%-3d 顶点级AABB=%-3d(跳过%d) max[A]=%.2e max[B]=%.2e max[C]=%.2e max||q|-1|=%.2e"
                      % (name, r["nodes"], r["leaf_bounds"], r.get("bounds_skipped", 0),
                         r["worst_A"], r["worst_B"], r["worst_C"], r["worst_q"]))
            else:
                r = check_bad_scene(name, path, exe, work, expect, args.config, dname, args.timeout)
                print("  [PASS] %-28s fail-fast 生效（rc=%d，错误信息点名 %s）" % (name, r["rc"], expect))
            report[name] = {"result": "pass", **r}
        except Failure as f:
            failed.append(name)
            report[name] = {"result": "fail", "reason": str(f)}
            print("  [FAIL] %-28s %s" % (name, f))

    print("-" * 78)
    if failed:
        print("CHECK RESULT: FAIL (%d/%d) -> %s" % (len(failed), len(cases), ", ".join(failed)))
    else:
        print("CHECK RESULT: PASS (%d/%d)  判据 M' = R·M_raw·R⁻¹" % (len(cases), len(cases)))

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
