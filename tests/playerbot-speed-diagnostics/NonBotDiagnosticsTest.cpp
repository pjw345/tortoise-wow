#include <cstdint>
using uint32 = uint32_t;
using int32 = int32_t;
#include "SpeedDeclarations.inc"
#include "SpeedStubs.inc"
int main()
{
    BotActionLog_LogSpeed(nullptr, "TEST", 1, 1, 1, false, 1, "non-bot build");
    BotActionLog_LogSpeedAura(nullptr, 123, 1, -50, false, "non-bot build");
}
