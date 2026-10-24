#pragma once

#include <cstdint>
#include <string>

// MPQ paths are case insensitive. Include the complete canonical source path
// in the output identity: custom and base assets can share a plain filename.
inline std::string CanonicalWmoSourcePath(std::string path)
{
    for (char& c : path)
    {
        if (c == '/') c = '\\';
        if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
    }
    return path;
}

inline std::string WmoOutputName(std::string const& sourcePath)
{
    std::string path = CanonicalWmoSourcePath(sourcePath);
    uint64_t hash = UINT64_C(14695981039346656037);
    for (unsigned char c : path) { hash ^= c; hash *= UINT64_C(1099511628211); }
    char const digits[] = "0123456789abcdef";
    std::string suffix(16, '0');
    for (int i = 15; i >= 0; --i) { suffix[i] = digits[hash & 15]; hash >>= 4; }
    std::string name = path.substr(path.find_last_of('\\') + 1);
    // Retain the legacy basename spelling as a suffix, so existing model-name
    // substring rules (for example los_mods) continue to match.
    auto alpha = [](char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); };
    for (std::size_t i = 0; i + 3 < name.size(); ++i)
        if ((i == 0 || !alpha(name[i-1])) && name[i] >= 'a' && name[i] <= 'z') name[i] -= 'a' - 'A';
    for (char& c : name) if (c == ' ') c = '_';
    return "wmo_" + suffix + "_" + name;
}
