#include <algorithm>
#include <cstdint>
#include <iostream>
using uint32 = std::uint32_t;
enum { CREATURE_TYPE_CRITTER = 8 };
struct Template { uint32 health_min, health_max, type; };
struct World { bool april = false; bool IsAprilFools() const { return april; } } sWorld;
struct CreatureHarness {
    float rate;
    uint32 createHealth = 0, maxHealth = 0, currentHealth = 0;
    float _GetHealthMod(uint32) { return rate; }
    void SetCreateHealth(uint32 n) { createHealth = n; }
    void SetMaxHealth(uint32 n) { maxHealth = n; }
    void SetHealth(uint32 n) { currentHealth = n; }
    // Minimal stand-in for the native setter's percentage-to-integer boundary.
    void SetHealthPercent(float p) { currentHealth = uint32(maxHealth * p / 100.0f); }
    void select(const Template* cinfo, float rellevel, float percentHealth) {
        uint32 rank = 0;
#include "native-health.inc"
    }
};
int main() {
    struct Case { uint32 lo, hi, type; float rate, level, percent; bool april; uint32 maximum, current; };
    const Case cases[] = {
        {1,1,0,0.5f,0,100,false,1,1},
        {10,20,0,0.001f,0.5f,100,false,1,1},
        {0,0,0,0.5f,0,100,false,0,0},
        {10,20,0,1,0.5f,100,false,15,15},
        {20,10,0,1,0.5f,100,false,15,15},
        {10,20,0,2,1,100,false,40,40},
        {10,20,0,1,0,50,false,10,5},
        {10,20,0,1,0,0,false,10,0},
        {1,1,0,0,0,100,false,1,1},
        {1,1,CREATURE_TYPE_CRITTER,0.5f,0,100,true,100000000,100000000},
        {0,0,CREATURE_TYPE_CRITTER,0.5f,0,100,true,100000000,100000000},
        {1,1,0,0.5f,0,100,true,1,1}
    };
    for (unsigned i = 0; i < sizeof(cases)/sizeof(cases[0]); ++i) {
        const auto& c = cases[i];
        Template t{c.lo,c.hi,c.type}; CreatureHarness creature{c.rate}; sWorld.april=c.april;
        creature.select(&t,c.level,c.percent);
        if (creature.createHealth != c.maximum || creature.maxHealth != c.maximum || creature.currentHealth != c.current) {
            std::cerr << "Native health case " << i << " failed\n"; return 1;
        }
    }
}
