// Offline native-format model assembly and narrowly scoped tile rebinding.
#include "TileAssembler.h"
#include "ModelInstance.h"
#include "VMapDefinitions.h"
#include "WmoTileCoverage.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cerrno>
#include <fstream>
#include <iostream>
#include <vector>
#include <filesystem>

struct Record { VMAP::ModelSpawn spawn; uint32 treeIndex; std::vector<char> bytes; };

static bool ReadTile(char const* path, std::vector<Record>& records)
{
    FILE* input = std::fopen(path, "rb");
    if (!input) return false;
    char magic[8]; uint32 count = 0;
    bool ok = std::fread(magic, 1, 8, input) == 8 && !std::memcmp(magic, VMAP::VMAP_MAGIC, 8)
        && std::fread(&count, 4, 1, input) == 1 && count <= 100000;
    for (uint32 i = 0; ok && i < count; ++i)
    {
        Record rec;
        long begin = std::ftell(input);
        ok = VMAP::ModelSpawn::readFromFile(input, rec.spawn) && std::fread(&rec.treeIndex, 4, 1, input) == 1;
        long finish = std::ftell(input);
        ok = ok && begin >= 0 && finish >= begin && finish - begin <= 4096;
        if (!ok) break;
        rec.bytes.resize(finish - begin);
        ok = std::fseek(input, begin, SEEK_SET) == 0
            && std::fread(rec.bytes.data(), 1, rec.bytes.size(), input) == rec.bytes.size();
        records.push_back(std::move(rec));
    }
    ok = ok && std::fgetc(input) == EOF && !std::ferror(input);
    std::fclose(input);
    return ok;
}

// Repair one missing reference within a single existing map bundle. Keep the
// original native record (including its BIH index) and all target bytes intact.
static int IncludeReference(char const* source, char const* target, char const* output, char const* id)
{
    std::string wantedText = id;
    if (wantedText.empty() || wantedText.find_first_not_of("0123456789") != std::string::npos) return 2;
    errno = 0; char* end = nullptr;
    unsigned long wanted = std::strtoul(id, &end, 10);
    if (errno == ERANGE || !end || *end || wanted > 0xffffffffUL) return 2;
    unsigned map[2], x[2], y[2];
    char const* paths[] = {source, target};
    for (unsigned i = 0; i < 2; ++i)
    {
        std::string name = std::filesystem::path(paths[i]).filename().string();
        int consumed = 0;
        if (std::sscanf(name.c_str(), "%3u_%2u_%2u.vmtile%n", &map[i], &x[i], &y[i], &consumed) != 3
            || consumed != static_cast<int>(name.size()) || x[i] > 63 || y[i] > 63) return 2;
    }
    std::error_code error;
    if (map[0] != map[1] || !std::filesystem::equivalent(
            std::filesystem::absolute(source).parent_path(), std::filesystem::absolute(target).parent_path(), error)
        || error || std::filesystem::exists(output)) return 7;
    std::vector<Record> sources, targets;
    if (!ReadTile(source, sources) || !ReadTile(target, targets)) return 5;
    Record const* selected = nullptr;
    for (auto const& record : sources)
        if (record.spawn.ID == wanted) { if (selected) return 6; selected = &record; }
    if (!selected || (selected->spawn.flags & (VMAP::MOD_M2 | VMAP::MOD_WORLDSPAWN))
        || !(selected->spawn.flags & VMAP::MOD_HAS_BOUND)) return 6;
    auto const& bounds = selected->spawn.getBounds();
    auto tiles = VMAP::WmoTileCoverage(bounds.low().x, bounds.low().y, bounds.high().x, bounds.high().y);
    if (std::find(tiles.begin(), tiles.end(), std::make_pair(x[1], y[1])) == tiles.end()) return 6;
    for (auto const& record : targets)
        if (record.spawn.ID == wanted || record.treeIndex == selected->treeIndex) return 6;
    FILE* file = std::fopen(output, "wb");
    if (!file) return 8;
    uint32 count = static_cast<uint32>(targets.size() + 1);
    bool ok = std::fwrite(VMAP::VMAP_MAGIC, 1, 8, file) == 8 && std::fwrite(&count, 4, 1, file) == 1;
    for (auto const& record : targets)
        ok = ok && std::fwrite(record.bytes.data(), 1, record.bytes.size(), file) == record.bytes.size();
    ok = ok && std::fwrite(selected->bytes.data(), 1, selected->bytes.size(), file) == selected->bytes.size();
    if (std::fclose(file)) ok = false;
    if (!ok) { std::remove(output); return 8; }
    return 0;
}

int main(int argc, char** argv)
{
    if (argc == 6 && std::string(argv[1]) == "--include")
        return IncludeReference(argv[2], argv[3], argv[4], argv[5]);
    if (argc == 5 && std::string(argv[1]) == "--assemble")
    {
        const std::string name=argv[4];
        if(name.size()<5 || name.substr(name.size()-4)!=".wmo" || name.find_first_of("/\\")!=std::string::npos) return 2;
        if(std::filesystem::exists(std::filesystem::path(argv[3])/(name+".vmo"))) return 7;
        VMAP::TileAssembler assembler(argv[2], argv[3]);
        return assembler.convertRawFile(name) ? 0 : 1;
    }
    bool inspect = argc == 3 && std::string(argv[1]) == "--inspect";
    bool rebind = argc == 7 && std::string(argv[1]) == "--rebind";
    if (!inspect && !rebind) { std::cerr << "--assemble raw-dir out-dir model; --inspect tile; --rebind in out spawn-id old-name new-name; --include source-tile target-tile new-output spawn-id\n"; return 2; }
    FILE* input = std::fopen(argv[2], "rb");
    if (!input) return 3;
    char magic[8]; uint32 count=0;
    if (std::fread(magic,1,8,input)!=8 || std::memcmp(magic,VMAP::VMAP_MAGIC,8) || std::fread(&count,4,1,input)!=1 || count>100000) { std::fclose(input); return 4; }
    std::vector<Record> records; unsigned matches=0;
    char* end=nullptr; unsigned long wanted=0;
    if(rebind) {
        std::string id=argv[4]; errno=0;
        if(id.empty() || id.find_first_not_of("0123456789")!=std::string::npos) {std::fclose(input);return 2;}
        wanted=std::strtoul(argv[4],&end,10);
        if(errno==ERANGE || !end || *end || wanted>0xffffffffUL) {std::fclose(input);return 2;}
    }
    for (uint32 i=0;i<count;++i)
    {
        Record rec; long begin=std::ftell(input);
        if(!VMAP::ModelSpawn::readFromFile(input,rec.spawn) || std::fread(&rec.treeIndex,4,1,input)!=1) {std::fclose(input);return 5;}
        long finish=std::ftell(input);
        if(begin<0 || finish<begin || finish-begin>4096) {std::fclose(input);return 5;}
        rec.bytes.resize(finish-begin);std::fseek(input,begin,SEEK_SET);
        if(std::fread(rec.bytes.data(),1,rec.bytes.size(),input)!=rec.bytes.size()) {std::fclose(input);return 5;}
        if(inspect) std::cout<<rec.spawn.ID<<'\t'<<rec.treeIndex<<'\t'<<rec.spawn.name<<'\t'<<rec.spawn.iPos.x<<','<<rec.spawn.iPos.y<<','<<rec.spawn.iPos.z<<'\n';
        if(rebind && rec.spawn.ID==wanted)
        {
            if(rec.spawn.name!=argv[5] || (rec.spawn.flags & VMAP::MOD_M2)) {std::fclose(input);return 6;}
            std::string name=argv[6];
            if(name.size()<5 || name.size()>499 || name.substr(name.size()-4)!=".wmo" || name.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-. ")!=std::string::npos) {std::fclose(input);return 2;}
            rec.spawn.name=name;++matches;
        }
        records.push_back(std::move(rec));
    }
    if(std::fgetc(input)!=EOF || std::ferror(input)) {std::fclose(input);return 5;}
    std::fclose(input);
    if(inspect)return 0;
    if(matches!=1 || std::filesystem::exists(argv[3]))return 7;
    FILE* output=std::fopen(argv[3],"wb");if(!output)return 8;
    bool ok=std::fwrite(magic,1,8,output)==8 && std::fwrite(&count,4,1,output)==1;
    for(auto const&rec:records)
    {
        if(rec.spawn.ID==wanted)
            ok=ok && VMAP::ModelSpawn::writeToFile(output,rec.spawn) && std::fwrite(&rec.treeIndex,4,1,output)==1;
        else
            ok=ok && std::fwrite(rec.bytes.data(),1,rec.bytes.size(),output)==rec.bytes.size();
    }
    if(std::fclose(output))ok=false;
    if(!ok) {std::remove(argv[3]);return 8;}
    return 0;
}
