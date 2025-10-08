#include "SanitizeName.h"
#include <cctype>

namespace exporters
{
    std::string SanitizeName(std::string n)
    {
        std::string out; out.reserve(n.size());
        char last='\0';
        for(char cOrig:n)
        {
            unsigned char uc=static_cast<unsigned char>(cOrig);
            char c=cOrig;
            if(uc<32) c='_';
            else if(!(std::isalnum(uc)||c=='_'||c=='-'||c=='.')) c='_';
            if(c=='_'&&last=='_') continue;
            out.push_back(c); last=c;
        }
        while(!out.empty()&&(out.front()=='.'||out.front()=='_')) out.erase(out.begin());
        while(!out.empty()&&(out.back()=='.'||out.back()=='_')) out.pop_back();
        if(out.empty()) out="unnamed";
        static const char *reserved[]={
            "CON","PRN","AUX","NUL",
            "COM1","COM2","COM3","COM4","COM5","COM6","COM7","COM8","COM9",
            "LPT1","LPT2","LPT3","LPT4","LPT5","LPT6","LPT7","LPT8","LPT9"
        };
        for(const char *r:reserved)
        {
            const std::string rs(r);
            if(out.size()!=rs.size()) continue;
            bool eq=true; for(size_t i=0;i<rs.size();++i) { char a=(char)std::toupper((unsigned char)out[i]); if(a!=rs[i]) { eq=false; break; } }
            if(eq) { out+='_'; break; }
        }
        return out;
    }
}
