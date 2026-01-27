#include "SceneExportJson.h"
#include "SceneExportData.h"
#include "SceneExportNames.h"
#include "math/BoundingVolumes.h"

#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>

namespace exporters
{
    static nlohmann::json SerializeMat4(const glm::mat4 &m)
    {
        nlohmann::json jm = nlohmann::json::array();
        for (int c = 0; c < 4; ++c)
            for (int r = 0; r < 4; ++r)
                jm.push_back(m[c][r]);
        return jm;
    }

    static nlohmann::json SerializeBounds(const BoundingVolumes &bv)
    {
        nlohmann::json j;
        j["aabbMin"] = { bv.aabb.min.x, bv.aabb.min.y, bv.aabb.min.z };
        j["aabbMax"] = { bv.aabb.max.x, bv.aabb.max.y, bv.aabb.max.z };
        j["sphere"]  = { bv.sphere.center.x, bv.sphere.center.y, bv.sphere.center.z, bv.sphere.radius };
        j["obbCenter"] = { bv.obb.center.x, bv.obb.center.y, bv.obb.center.z };
        j["obbAxisX"]  = { bv.obb.axisX.x, bv.obb.axisX.y, bv.obb.axisX.z };
        j["obbAxisY"]  = { bv.obb.axisY.x, bv.obb.axisY.y, bv.obb.axisY.z };
        j["obbAxisZ"]  = { bv.obb.axisZ.x, bv.obb.axisZ.y, bv.obb.axisZ.z };
        j["obbHalf"]   = { bv.obb.halfSize.x, bv.obb.halfSize.y, bv.obb.halfSize.z };
        return j;
    }

    bool WriteSceneJson(const SceneExportData &data, const std::filesystem::path &filePath)
    {
        nlohmann::json j;
        if (!data.nameTable.empty()) j["nameTable"] = data.nameTable;
        if (data.sceneNameIndex >= 0) j["sceneNameIndex"] = data.sceneNameIndex;

        if (!data.trsTable.empty())
        {
            nlohmann::json trsArr = nlohmann::json::array();
            for (const auto &t : data.trsTable)
            {
                nlohmann::json jt;
                jt["t"] = { t.translation.x, t.translation.y, t.translation.z };
                jt["r"] = { t.rotation.w, t.rotation.x, t.rotation.y, t.rotation.z };
                jt["s"] = { t.scale.x, t.scale.y, t.scale.z };
                trsArr.push_back(std::move(jt));
            }
            j["trsTable"] = std::move(trsArr);
        }
        if (!data.matrixTable.empty())
        {
            nlohmann::json mats = nlohmann::json::array();
            for (const auto &m : data.matrixTable) mats.push_back(SerializeMat4(m));
            j["matrixTable"] = std::move(mats);
        }
        if (!data.boundsTable.empty())
        {
            nlohmann::json bts = nlohmann::json::array();
            for (const auto &b : data.boundsTable) bts.push_back(SerializeBounds(b));
            j["boundsTable"] = std::move(bts);
        }
        if (data.sceneBoundsIndex >= 0) j["sceneBoundsIndex"] = data.sceneBoundsIndex;

        j["nodes"] = nlohmann::json::array();
        for (const auto &n : data.nodes)
        {
            nlohmann::json jn;
            jn["index"] = n.originalIndex;
            if (n.nameIndex >= 0) jn["nameIndex"] = n.nameIndex;
            jn["localM"] = n.localMatrixIndex;
            jn["worldM"] = n.worldMatrixIndex;
            if (n.trsIndex >= 0) jn["trs"] = n.trsIndex;
            if (n.boundsIndex >= 0) jn["boundsIndex"] = n.boundsIndex;
            if (!n.primitives.empty()) jn["primitives"] = n.primitives;
            if (!n.children.empty()) jn["children"] = n.children;
            j["nodes"].push_back(std::move(jn));
        }
        j["primitives"] = nlohmann::json::array();
        for (const auto &p : data.primitives)
        {
            nlohmann::json jp;
            jp["index"] = p.originalIndex;
            jp["geometry"] = p.geometryIndex;
            if (!p.geometryFile.empty()) jp["file"] = p.geometryFile;
            if (p.materialIndex.has_value()) jp["material"] = p.materialIndex.value();
            j["primitives"].push_back(std::move(jp));
        }
        if (!data.materials.empty())
        {
            nlohmann::json mats = nlohmann::json::array();
            for (const auto &m : data.materials)
            {
                nlohmann::json jm; jm["index"] = m.originalIndex; if (!m.file.empty()) jm["file"] = m.file; mats.push_back(std::move(jm));
            }
            j["materials"] = std::move(mats);
        }
        if (!data.geometries.empty())
        {
            nlohmann::json geos = nlohmann::json::array();
            for (const auto &g : data.geometries)
            {
                nlohmann::json gj; gj["index"] = g.originalIndex; if (!g.file.empty()) gj["file"] = g.file; geos.push_back(std::move(gj));
            }
            j["geometries"] = std::move(geos);
        }

        std::ofstream ofs(filePath, std::ios::binary);
        if (!ofs) { std::cerr << "[Export] Cannot open scene json for write: " << filePath << "\n"; return false; }
        ofs << j.dump(4);
        return true;
    }
}
