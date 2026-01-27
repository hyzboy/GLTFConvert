#include "SceneExportNames.h"

namespace exporters
{
    int32_t GetOrAddName(std::unordered_map<std::string, int32_t> &map,
                         std::vector<std::string> &table,
                         const std::string &name)
    {
        if (name.empty())
            return -1;

        auto it = map.find(name);
        if (it != map.end())
            return it->second;

        int32_t idx = static_cast<int32_t>(table.size());
        table.push_back(name);
        map.emplace(name, idx);
        return idx;
    }
}
