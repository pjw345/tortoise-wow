#pragma once

#include "Common.h"

class Unit;

// Implemented by the bot module, or PlayerbotStubs.cpp for non-bot builds.
// Read-only, opt-in diagnostics; never changes movement or consumes an ACK.
void BotActionLog_LogSpeed(Unit* unit, const char* event, int moveType,
    float oldRate, float requestedRate, bool forced, float ratio, const char* reason);
void BotActionLog_LogSpeedAura(Unit* unit, uint32 spellId, uint32 auraType,
    int32 amount, bool apply, const char* reason);
