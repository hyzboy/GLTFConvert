#include "gltf/import/GLTFExtensions.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <unordered_map>
#include <unordered_set>

namespace gltf
{
    namespace
    {
        /// 源文件声明的扩展名 → 处置。三类：
        ///  - **enabled**：启用解析；其中"未实现效果"的进 `unhandled`（告警，不静默降级）
        ///  - **rejected**：明确拒绝并给出理由（产出会缺失/错误，宁可失败）
        ///  - 其它（数组里没列到的名字）⇒ fastgltf 根本不支持 ⇒ 拒绝
        const std::unordered_set<std::string> &EnabledNames()
        {
            static const std::unordered_set<std::string> s = {
                "KHR_texture_transform",           // UV 变换（未实现效果 → 告警）
                "KHR_materials_unlit",             // **已实现**
                "KHR_materials_ior",
                "KHR_materials_specular",
                "KHR_materials_iridescence",
                "KHR_materials_volume",
                "KHR_materials_transmission",
                "KHR_materials_clearcoat",
                "KHR_materials_emissive_strength",
                "KHR_materials_sheen",
                "KHR_materials_anisotropy",
                "KHR_materials_dispersion",
                "KHR_materials_diffuse_transmission",
                "KHR_materials_variants",          // 取默认变体（等价于不选变体）
                "KHR_lights_punctual",             // 光节点；几何不受影响
                "KHR_mesh_quantization",           // accessor 数据表示层
                "EXT_texture_webp",
                "KHR_texture_basisu",
                "MSFT_texture_dds",
                "GODOT_single_root",
            };
            return s;
        }

        /// 转换器**真正实现**了效果的扩展（其余 enabled 的都要告警）
        const std::unordered_set<std::string> &HandledNames()
        {
            static const std::unordered_set<std::string> s = {
                "KHR_materials_unlit",
                "KHR_mesh_quantization",
                "KHR_materials_variants",
                "GODOT_single_root",
            };
            return s;
        }

        /// 明确拒绝的扩展 → 理由
        const char *RejectReason(const std::string &name)
        {
            static const std::unordered_map<std::string, const char *> reasons = {
                { "KHR_draco_mesh_compression",
                  "转换器未集成 Draco 解码，几何会缺失/错误" },
                { "EXT_meshopt_compression",
                  "转换器未集成 meshopt 解码，几何会缺失/错误" },
                { "EXT_mesh_gpu_instancing",
                  "转换器未实现 GPU 实例化，实例几何会整体丢失" },
                { "KHR_accessor_float64",
                  "转换器未处理双精度 accessor" },
                { "KHR_materials_pbrSpecularGlossiness",
                  "已废弃的材质模型，转换器未实现" },
                { "MSFT_packing_normalRoughnessMetallic",
                  "纹理打包约定未实现，会得到错误的 PBR 通道" },
                { "MSFT_packing_occlusionRoughnessMetallic",
                  "纹理打包约定未实现，会得到错误的 PBR 通道" },
            };
            const auto it = reasons.find(name);
            return it == reasons.end() ? nullptr : it->second;
        }

        /// 取出 JSON 文本：`.gltf` 整份即 JSON；`.glb` 需定位 JSON chunk（type 0x4E4F534A）
        std::string_view JsonChunkOf(const std::vector<char> &raw)
        {
            if (raw.size() < 4)
                return {};
            if (std::memcmp(raw.data(), "glTF", 4) != 0)
                return { raw.data(), raw.size() };

            const char *base = raw.data();
            const char *end  = base + raw.size();
            const char *p    = base + 12;                 // GLB 头：magic(4) + version(4) + length(4)
            while (p + 8 <= end)
            {
                uint32_t len = 0, type = 0;
                std::memcpy(&len, p, 4);
                std::memcpy(&type, p + 4, 4);
                p += 8;
                if (type == 0x4E4F534A)                   // 'JSON'
                    return { p, static_cast<size_t>(std::min<ptrdiff_t>(len, end - p)) };
                p += len;
            }
            return {};
        }

        /// 在 JSON 文本里找 `key` 后面的数组，收集其中的字符串（不引第三方 JSON 库：
        /// 这里只要名字列表，且 glTF 的 JSON 是机器生成的规范文本）
        void CollectStringArray(std::string_view json, std::string_view key, std::vector<std::string> &out)
        {
            std::size_t pos = 0;
            while ((pos = json.find(key, pos)) != std::string_view::npos)
            {
                const std::size_t lb = json.find('[', pos + key.size());
                if (lb == std::string_view::npos)
                    return;
                const std::size_t rb = json.find(']', lb);
                if (rb == std::string_view::npos)
                    return;
                const std::string_view arr = json.substr(lb + 1, rb - lb - 1);
                std::size_t q = 0;
                while ((q = arr.find('"', q)) != std::string_view::npos)
                {
                    const std::size_t e = arr.find('"', q + 1);
                    if (e == std::string_view::npos)
                        break;
                    out.emplace_back(arr.substr(q + 1, e - q - 1));
                    q = e + 1;
                }
                pos = rb;
            }
        }
    }//namespace

    fastgltf::Extensions EnabledExtensions()
    {
        using fastgltf::Extensions;
        return Extensions::KHR_texture_transform
             | Extensions::KHR_materials_unlit
             | Extensions::KHR_materials_ior
             | Extensions::KHR_materials_specular
             | Extensions::KHR_materials_iridescence
             | Extensions::KHR_materials_volume
             | Extensions::KHR_materials_transmission
             | Extensions::KHR_materials_clearcoat
             | Extensions::KHR_materials_emissive_strength
             | Extensions::KHR_materials_sheen
             | Extensions::KHR_materials_anisotropy
             | Extensions::KHR_materials_dispersion
             | Extensions::KHR_materials_diffuse_transmission
             | Extensions::KHR_materials_variants
             | Extensions::KHR_lights_punctual
             | Extensions::KHR_mesh_quantization
             | Extensions::EXT_texture_webp
             | Extensions::KHR_texture_basisu
             | Extensions::MSFT_texture_dds
             | Extensions::GODOT_single_root;
    }

    std::vector<std::string> ReadRequiredExtensions(const std::filesystem::path &path)
    {
        std::ifstream ifs(path, std::ios::binary | std::ios::ate);
        if (!ifs)
            return {};

        const std::streamoff size = ifs.tellg();
        if (size <= 0)
            return {};
        ifs.seekg(0);

        std::vector<char> raw(static_cast<std::size_t>(size));
        if (!ifs.read(raw.data(), size))
            return {};

        std::vector<std::string> names;
        CollectStringArray(JsonChunkOf(raw), "\"extensionsRequired\"", names);

        std::sort(names.begin(), names.end());
        names.erase(std::unique(names.begin(), names.end()), names.end());
        return names;
    }

    ExtensionCheck CheckRequiredExtensions(const std::filesystem::path &path)
    {
        ExtensionCheck result;

        for (const auto &name : ReadRequiredExtensions(path))
        {
            if (EnabledNames().count(name))
            {
                if (!HandledNames().count(name))
                    result.unhandled.push_back(name);       // 启用解析，但效果未实现
                continue;
            }

            result.rejected.push_back(name);
            if (!result.error.empty())
                result.error += "；";
            const char *reason = RejectReason(name);
            result.error += name;
            result.error += reason ? "（" : "（fastgltf/转换器均不支持该必需扩展）";
            if (reason)
                result.error += reason;
            result.error += "）";
        }

        result.ok = result.rejected.empty();
        return result;
    }
}//namespace gltf
