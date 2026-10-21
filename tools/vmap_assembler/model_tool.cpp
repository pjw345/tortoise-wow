// Offline native-format model assembly and narrowly scoped tile rebinding.
#include "TileAssembler.h"
#include "ModelInstance.h"
#include "VMapDefinitions.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cerrno>
#include <fstream>
#include <iostream>
#include <vector>
#include <filesystem>

struct Record { VMAP::ModelSpawn spawn; uint32 treeIndex; std::vector<char> bytes; };
int main(int argc, char** argv)
{
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
    if (!inspect && !rebind) { std::cerr << "--assemble raw-dir out-dir model; --inspect tile; --rebind in out spawn-id old-name new-name\n"; return 2; }
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
