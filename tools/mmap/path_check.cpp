// Read-only offline checker for native mmap/Detour path connectivity.
#include "DetourNavMesh.h"
#include "DetourNavMeshQuery.h"
#include "DetourAlloc.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <fstream>
#include <iostream>
#include <vector>
#include <filesystem>
#include <algorithm>

int main(int argc, char** argv)
{
    if (argc != 9) { std::cerr << "usage: mmap_path_check map.mmap tile.mmtile-or-directory x1 y1 z1 x2 y2 z2\n"; return 2; }
    dtNavMeshParams params{};
    std::ifstream map(argv[1], std::ios::binary);
    if (!map.read(reinterpret_cast<char*>(&params), sizeof(params))) return 3;
    auto* mesh = dtAllocNavMesh();
    if (!mesh || dtStatusFailed(mesh->init(&params))) return 4;
    std::vector<std::filesystem::path> tiles;
    std::filesystem::path tilePath(argv[2]);
    if (std::filesystem::is_directory(tilePath)) {
        const std::string mapId=std::filesystem::path(argv[1]).stem().string();
        for(auto const& entry:std::filesystem::directory_iterator(tilePath)) {
            auto name=entry.path().filename().string();
            if(entry.is_regular_file() && entry.path().extension()==".mmtile" && name.compare(0,mapId.size(),mapId)==0) tiles.push_back(entry.path());
        }
        std::sort(tiles.begin(),tiles.end());
    } else tiles.push_back(tilePath);
    if(tiles.empty())return 5;
    for(auto const& file:tiles) {
        unsigned header[5]{};
        std::ifstream tile(file, std::ios::binary);
        if (!tile.read(reinterpret_cast<char*>(header), sizeof(header)) || header[0] != 0x4d4d4150 || header[1] != DT_NAVMESH_VERSION || header[2] != 6 || header[3] > 64*1024*1024) return 5;
        auto* data = static_cast<unsigned char*>(dtAlloc(header[3], DT_ALLOC_PERM));
        if (!data || !tile.read(reinterpret_cast<char*>(data), header[3])) return 6;
        if (dtStatusFailed(mesh->addTile(data, header[3], DT_TILE_FREE_DATA, 0, nullptr))) return 7;
    }
    auto* query = dtAllocNavMeshQuery();
    if (!query || dtStatusFailed(query->init(mesh, 32768))) return 8;
    float start[] = {std::strtof(argv[4],nullptr), std::strtof(argv[5],nullptr), std::strtof(argv[3],nullptr)};
    float end[] = {std::strtof(argv[7],nullptr), std::strtof(argv[8],nullptr), std::strtof(argv[6],nullptr)};
    for(int i=3;i<9;++i) {
        char* end=nullptr;float n=std::strtof(argv[i],&end);
        if(!end || end==argv[i] || *end || !std::isfinite(n))return 2;
    }
    const float extents[] = {2.f, 3.f, 2.f};
    dtQueryFilter filter; filter.setIncludeFlags(0x11); filter.setExcludeFlags(0);
    dtPolyRef a=0,b=0; float pa[3]{},pb[3]{};
    auto sa=query->findNearestPoly(start,extents,&filter,&a,pa);
    auto sb=query->findNearestPoly(end,extents,&filter,&b,pb);
    if (dtStatusFailed(sa)||dtStatusFailed(sb)||!a||!b) { std::cout << "{\"complete\":false,\"reason\":\"endpoint_not_on_mesh\",\"start_ref\":" << a << ",\"end_ref\":" << b << "}\n"; return 1; }
    dtPolyRef corridor[4096]{}; int count=0;
    auto status=query->findPath(a,b,pa,pb,&filter,corridor,&count,4096);
    bool complete=dtStatusSucceed(status)&&count>0&&corridor[count-1]==b&&!dtStatusDetail(status,DT_PARTIAL_RESULT)&&!dtStatusDetail(status,DT_BUFFER_TOO_SMALL)&&!dtStatusDetail(status,DT_OUT_OF_NODES);
    float straight[4096*3]{}; unsigned char flags[4096]{}; dtPolyRef refs[4096]{};int n=0;
    if (count) query->findStraightPath(pa,pb,corridor,count,straight,flags,refs,&n,4096);
    std::cout << "{\"complete\":" << (complete?"true":"false") << ",\"status\":" << status << ",\"corridor_count\":" << count << ",\"start_projected\":[" << pa[2] << ',' << pa[0] << ',' << pa[1] << "],\"end_projected\":[" << pb[2] << ',' << pb[0] << ',' << pb[1] << "],\"points\":[";
    for(int i=0;i<n;++i) {if(i)std::cout<<',';std::cout<<'['<<straight[i*3+2]<<','<<straight[i*3]<<','<<straight[i*3+1]<<']';}
    std::cout << "]}\n";
    dtFreeNavMeshQuery(query);dtFreeNavMesh(mesh);return complete?0:1;
}
