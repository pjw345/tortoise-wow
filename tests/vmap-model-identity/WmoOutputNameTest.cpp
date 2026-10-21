#include "WmoOutputName.h"
#include <cassert>
#include <iostream>
int main() {
 const std::string base="World\\wmo\\Azeroth\\Buildings\\Magetower\\magetower.wmo";
 const std::string custom="World\\custom\\Dalaran\\magetower\\magetower.wmo";
 assert(WmoOutputName(base)=="wmo_e5a88432c8aa603d_Magetower.wmo");
 assert(WmoOutputName(base)!=WmoOutputName(custom));
 assert(WmoOutputName(base)==WmoOutputName("WORLD/WMO/AZEROTH/BUILDINGS/MAGETOWER/MAGETOWER.WMO"));
 assert(WmoOutputName(custom)==WmoOutputName("world/custom/dalaran/magetower/magetower.wmo"));
 assert(WmoOutputName(base).find_first_of("/\\")==std::string::npos);
 assert(WmoOutputName("World\\a\\x.wmo")!=WmoOutputName("World\\b\\x.wmo"));
 assert(WmoOutputName("World\\Some Place\\A File.wmo").find(' ')==std::string::npos);
 assert(WmoOutputName(base).find("Magetower.wmo")!=std::string::npos);
 assert(WmoOutputName("World\\Transport_Zeppelin.wmo").find("Transport_Zeppelin.wmo")!=std::string::npos);
 std::cout<<"WMO output identity checks passed\n";
}
