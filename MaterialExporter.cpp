#include "MaterialExporter.h"

#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
#include <vector>

#include "pure/Material.h"

using nlohmann::json;

namespace exporters
{
    static json BuildMaterialsJson(const std::vector<pure::Material> &materials)
    {
        json arr=json::array();
        for(const auto &m:materials)
        {
            json mj=json::object();
            if(!m.name.empty())
                mj["name"]=m.name;
            arr.push_back(std::move(mj));
        }
        return arr;
    }

    bool ExportMaterials(const std::vector<pure::Material> &materials,const std::filesystem::path &dir)
    {
        json root=json::object();
        root["materials"]=BuildMaterialsJson(materials);

        auto path=dir/"Materials.json";
        std::ofstream ofs(path,std::ios::binary);
        if(!ofs)
        {
            std::cerr<<"[Export] Cannot open materials file: "<<path<<"\n";
            return false;
        }
        ofs<<root.dump(4);
        ofs.close();
        std::cout<<"[Export] Saved: "<<path<<"\n";
        return true;
    }
}
