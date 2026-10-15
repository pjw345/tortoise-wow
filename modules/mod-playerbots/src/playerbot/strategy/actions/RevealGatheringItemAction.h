#pragma once

#include "playerbot/strategy/Action.h"
#include "MovementActions.h"
#include "playerbot/strategy/values/LastMovementValue.h"

namespace ai
{
    bool CanRevealGatheringItem(PlayerbotAI* ai, GameObject* go);
    class RevealGatheringItemAction : public Action {
    public:
        RevealGatheringItemAction(PlayerbotAI* ai) : Action(ai, "reveal gathering item") {}

        virtual bool Execute(Event& event) override;
        virtual bool isUsefulWhenStunned() override { return true; }
    };

}
