#include "SceneExportLists.h"
#include <fstream>
#include <iostream>

namespace exporters
{
    void WriteIndexedListFile(const std::filesystem::path &path,
                              const std::vector<std::string> &entries,
                              const char *label)
    {
        if (entries.empty()) return;
        std::ofstream ofs(path, std::ios::binary);
        if (!ofs)
        {
            std::cerr << "[Export] failed to write " << label << " list file: " << path << "\n";
            return;
        }
        for (size_t i = 0; i < entries.size(); ++i)
        {
            ofs << i << '\t' << entries[i] << '\n';
        }
    }
}
