#include "MaterialExporter.h"

#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>

#include "pure/ModelCore.h"
#include "pure/Material.h"

using nlohmann::json;

namespace exporters
{
    static json BuildMaterialsJson(const pure::Model &sm)
    {
        json arr=json::array();
        for(const auto &m:sm.materials)
        {
            json mj=json::object();
            if(!m.name.empty())
                mj["name"]=m.name;
            arr.push_back(std::move(mj));
        }
        return arr;
    }

    bool ExportMaterials(const pure::Model &model,const std::filesystem::path &dir)
    {
        json root=json::object();
        root["materials"]=BuildMaterialsJson(model);

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
