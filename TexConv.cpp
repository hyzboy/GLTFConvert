#include <filesystem>
#include <iostream>
#include <mutex>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#ifdef _WIN32
    #include <windows.h>
    #include <process.h>
#endif

namespace texconv
{
    namespace {
        std::once_flag g_once;
        bool g_available=false;
        std::filesystem::path g_texconvPath; // full path to TexConv.exe if available

        bool IsIniSpace(const char ch)
        {
            return ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n';
        }

        char *TrimIni(char *text)
        {
            while(*text && IsIniSpace(*text))
                ++text;

            char *end = text + std::strlen(text);
            while(end > text && IsIniSpace(end[-1]))
                --end;
            *end = 0;
            return text;
        }

        bool ReadTexConvDirFromIni(const std::filesystem::path &configPath,
                                   std::filesystem::path &texconvDir)
        {
#ifdef _WIN32
            FILE *file = nullptr;
            if(_wfopen_s(&file, configPath.c_str(), L"rb") != 0 || !file)
                return false;
#else
            FILE *file = std::fopen(configPath.string().c_str(), "rb");
            if(!file)
                return false;
#endif

            bool sawSection = false;
            bool inGLTFConvertSection = false;
            char line[4096];
            while(std::fgets(line, sizeof(line), file))
            {
                char *text = TrimIni(line);
                if(!*text || *text == ';' || *text == '#')
                    continue;

                if(*text == '[')
                {
                    char *sectionEnd = std::strchr(text + 1, ']');
                    if(!sectionEnd)
                        continue;

                    *sectionEnd = 0;
                    const char *section = TrimIni(text + 1);
                    sawSection = true;
                    inGLTFConvertSection = std::strcmp(section, "GLTFConvert") == 0;
                    continue;
                }

                char *separator = std::strchr(text, '=');
                if(!separator || (sawSection && !inGLTFConvertSection))
                    continue;

                *separator = 0;
                char *key = TrimIni(text);
                char *value = TrimIni(separator + 1);
                if(std::strcmp(key, "TexConvDir") != 0 || !*value)
                    continue;

                const size_t valueLength = std::strlen(value);
                if(valueLength >= 2 &&
                   ((value[0] == '"' && value[valueLength - 1] == '"') ||
                    (value[0] == '\'' && value[valueLength - 1] == '\'')))
                {
                    value[valueLength - 1] = 0;
                    ++value;
                }

                texconvDir = std::filesystem::path(value);
                if(texconvDir.is_relative())
                    texconvDir = configPath.parent_path() / texconvDir;
                std::fclose(file);
                return true;
            }

            std::fclose(file);
            return false;
        }

        std::filesystem::path GetExecutableDirectory()
        {
#ifdef _WIN32
            wchar_t buffer[MAX_PATH];
            DWORD len = GetModuleFileNameW(nullptr, buffer, MAX_PATH);
            if(len==0 || len==MAX_PATH)
                return std::filesystem::current_path();
            std::filesystem::path exePath(buffer);
            return exePath.parent_path();
#else
            return std::filesystem::current_path();
#endif
        }

        void Detect()
        {
            g_available=false;
            g_texconvPath.clear();

            std::filesystem::path exeDir = GetExecutableDirectory();
            std::filesystem::path cwd    = std::filesystem::current_path();

            std::vector<std::filesystem::path> configFiles;
            configFiles.push_back(exeDir / "GLTFConvert.ini");
            if(cwd != exeDir)
                configFiles.push_back(cwd / "GLTFConvert.ini");
#ifdef GLTF_CONFIG_FILE
            configFiles.emplace_back(GLTF_CONFIG_FILE);
#endif

            std::filesystem::path configuredTexConvDir;
            for(const auto &configFile : configFiles)
            {
                if(ReadTexConvDirFromIni(configFile, configuredTexConvDir))
                    break;
            }

            std::vector<std::filesystem::path> searchDirs;
            if(!configuredTexConvDir.empty())
                searchDirs.push_back(configuredTexConvDir);
            searchDirs.push_back(exeDir);
            if(cwd != exeDir) searchDirs.push_back(cwd);

#ifdef _WIN32
            const std::wstring fileName = L"TexConv.exe";
#else
            const std::string fileName = "TexConv";
#endif
            for(const auto &dir : searchDirs)
            {
#ifdef _WIN32
                std::filesystem::path candidate = dir / fileName;
#else
                std::filesystem::path candidate = dir / fileName;
#endif
                if(std::filesystem::exists(candidate))
                {
                    g_available=true;
                    g_texconvPath=candidate;
                    std::cout << "[TexConv] Found at: " << g_texconvPath.string() << " (searched " << dir.string() << ")\n";
                    return;
                }
            }
            std::cout << "[TexConv] Not found. Checked: " << exeDir.string();
            if(cwd!=exeDir) std::cout << ", " << cwd.string();
            std::cout << "\n";
        }
    }

    // Initialize detection, returns true if TexConv executable exists alongside current program.
    bool Initialize(std::filesystem::path *outPath)
    {
        std::call_once(g_once, Detect);
        if(outPath)
            *outPath = g_texconvPath;
        return g_available;
    }

    bool IsAvailable()
    {
        std::call_once(g_once, Detect);
        return g_available;
    }

    const std::filesystem::path & Path()
    {
        std::call_once(g_once, Detect);
        return g_texconvPath;
    }

    bool Convert(const std::filesystem::path &src, const std::filesystem::path &dst)
    {
        if(!IsAvailable()) return false;
        auto texconvPath = Path();

        // Prepare output base (no extension). TexConv will produce base + .Tex2D (assumed)
        std::filesystem::path baseOut = dst;
        baseOut.replace_extension();
        std::filesystem::path produced = baseOut; produced += ".Tex2D";

        std::error_code ec;
        std::filesystem::create_directories(dst.parent_path(), ec);

#ifdef _WIN32
        // Build arguments for spawn (no quoting needed)
        std::wstring exe = texconvPath.wstring();
        std::wstring argMip = L"/mip";
        std::wstring argOut = L"/out:" + baseOut.wstring();
        std::wstring argSrc = src.wstring();
        const wchar_t *argv[] = { exe.c_str(), argMip.c_str(), argOut.c_str(), argSrc.c_str(), nullptr };
        int ret = _wspawnv(_P_WAIT, exe.c_str(), argv);
        if(ret==-1)
        {
            std::cerr << "[TexConv] _wspawnv failed, fallback to system.\n";
            // Fallback single string with quotes
            std::wstring cmd = L"\"" + exe + L"\" " + argMip + L" " + argOut + L" \"" + argSrc + L"\"";
            ret = _wsystem(cmd.c_str());
        }
#else
        std::string cmd = std::string("\"") + texconvPath.string() + "\" /mip /out:\"" + baseOut.string() + "\" \"" + src.string() + "\"";
        int ret = std::system(cmd.c_str());
#endif
        if(ret==0)
        {
            if(std::filesystem::exists(produced))
            {
                std::cout << "[TexConv] Converted: " << src << " -> " << produced.string() << "\n";
                return true;
            }
            else
            {
                std::cerr << "[TexConv] Expected produced file missing: " << produced.string() << "\n";
            }
        }
        else
        {
            std::cerr << "[TexConv] Process returned code " << ret << "\n";
        }
        return false;
    }
}
