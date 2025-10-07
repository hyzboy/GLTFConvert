#include "SceneExportData.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>

namespace exporters
{
    using nlohmann::json;

    static json SerializeMat4(const glm::mat4 &m)
    {
        json arr = json::array();
        for (int c = 0; c < 4; ++c)
            for (int r = 0; r < 4; ++r)
                arr.push_back(m[c][r]);
        return arr;
    }

    static json SerializeBounds(const BoundingVolumes &bv)
    {
        json jb;
        if (!bv.emptyAABB())
        {
            jb["aabb"] = { bv.aabb.min.x, bv.aabb.min.y, bv.aabb.min.z, bv.aabb.max.x, bv.aabb.max.y, bv.aabb.max.z };
        }
        if (!bv.emptyOBB())
        {
            jb["obb"] = {
                bv.obb.center.x, bv.obb.center.y, bv.obb.center.z,
                bv.obb.axisX.x, bv.obb.axisX.y, bv.obb.axisX.z,
                bv.obb.axisY.x, bv.obb.axisY.y, bv.obb.axisY.z,
                bv.obb.axisZ.x, bv.obb.axisZ.y, bv.obb.axisZ.z,
                bv.obb.halfSize.x, bv.obb.halfSize.y, bv.obb.halfSize.z
            };
        }
        if (!bv.emptySphere())
        {
            jb["sphere"] = { bv.sphere.center.x, bv.sphere.center.y, bv.sphere.center.z, bv.sphere.radius };
        }
        return jb;
    }

    bool WriteSceneJson(const SceneExportData &data, const std::filesystem::path &filePath)
    {
        json j;

        if (!data.nameTable.empty())
            j["nameTable"] = data.nameTable;
        if (data.sceneNameIndex >= 0)
            j["nameIndex"] = data.sceneNameIndex;
        if (!data.sceneBounds.emptyAABB() || !data.sceneBounds.emptyOBB() || !data.sceneBounds.emptySphere())
            j["sceneBounds"] = SerializeBounds(data.sceneBounds);

        j["nodes"] = json::array();
        for (const auto &n : data.nodes)
        {
            json jn;
            jn["index"] = n.originalIndex;
            if (n.nameIndex >= 0)
                jn["nameIndex"] = n.nameIndex;
            jn["localMatrix"] = SerializeMat4(n.localMatrix);
            jn["worldMatrix"] = SerializeMat4(n.worldMatrix);
            if (!n.worldBounds.emptyAABB() || !n.worldBounds.emptyOBB() || !n.worldBounds.emptySphere())
                jn["bounds"] = SerializeBounds(n.worldBounds);
            if (!n.subMeshes.empty())
                jn["subMeshes"] = n.subMeshes;
            if (!n.children.empty())
                jn["children"] = n.children;
            j["nodes"].push_back(std::move(jn));
        }

        j["subMeshes"] = json::array();
        for (const auto &s : data.subMeshes)
        {
            json js;
            js["index"] = s.originalIndex;
            js["geometry"] = s.geometryIndex;
            if (!s.geometryFile.empty())
                js["file"] = s.geometryFile;
            if (s.materialIndex.has_value())
                js["material"] = s.materialIndex.value();
            j["subMeshes"].push_back(std::move(js));
        }

        if (!data.materials.empty())
        {
            j["materials"] = json::array();
            for (const auto &m : data.materials)
            {
                json jm;
                jm["index"] = m.originalIndex;
                if (m.nameIndex >= 0)
                    jm["nameIndex"] = m.nameIndex;
                j["materials"].push_back(std::move(jm));
            }
        }

        if (!data.geometries.empty())
        {
            j["geometries"] = json::array();
            for (const auto &g : data.geometries)
            {
                json gj;
                gj["index"] = g.originalIndex;
                gj["file"] = g.file;
                j["geometries"].push_back(std::move(gj));
            }
        }

        std::ofstream ofs(filePath, std::ios::binary);
        if (!ofs)
        {
            std::cerr << "[Export] Cannot open scene json file: " << filePath << "\n";
            return false;
        }

        ofs << j.dump(4);
        return true;
    }
}
