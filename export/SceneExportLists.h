#pragma once
#include <filesystem>
#include <vector>
#include <string>

namespace exporters
{
    // 写入索引文本列表: 每行  index<TAB>name  \n
    // 失败时输出错误日志（不抛异常）。空列表直接返回。
    void WriteIndexedListFile(const std::filesystem::path &path,
                              const std::vector<std::string> &entries,
                              const char *label);
}
