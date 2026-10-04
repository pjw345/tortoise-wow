
#include "playerbot/playerbot.h"
#include "SkillAction.h"

using namespace ai;

bool SkillAction::Execute(Event& event)
{
    Player* requester = event.getOwner() ? event.getOwner() : GetMaster();
    if (!requester)
        return false;

    std::string cmd = event.getParam();

    bool unlearn = (cmd.find("unlearn ") == 0);

    std::string skillName = cmd;

    if (unlearn)
        skillName = skillName.substr(8);

    std::wstring wnamepart;

    if (!Utf8toWStr(skillName, wnamepart))
        return false;

    std::set<uint32> skillIds = ChatHelper::ExtractAllSkillIds(skillName);

    // converting string that we try to find to lower case
    wstrToLower(wnamepart);

    bool skillFound = false;
    std::map<std::string, std::string> args;

    uint32 preferredLocale = LOCALE_enUS;
    if (requester->GetSession())
    {
        uint32 sessionLocale = requester->GetSession()->GetSessionDbcLocale();
        if (sessionLocale < MAX_DBC_LOCALE)
            preferredLocale = sessionLocale;
    }

    for (uint32 id = 0; id < sSkillLineStore.GetNumRows(); ++id)
    {
        if (!bot->HasSkill(id))
            continue;

        SkillLineEntry const* skillInfo = sSkillLineStore.LookupEntry(id);
        if (!skillInfo)
            continue;

        bool nameMatches = false;
        if (!skillName.empty() && skillIds.empty())
        {
            auto localeMatches = [&](uint32 locale)
            {
                char const* localizedName = skillInfo->name[locale];
                return localizedName && localizedName[0] && Utf8FitTo(localizedName, wnamepart);
            };

            nameMatches = localeMatches(preferredLocale);
            for (uint32 locale = 0; !nameMatches && locale < MAX_DBC_LOCALE; ++locale)
            {
                if (locale != preferredLocale)
                    nameMatches = localeMatches(locale);
            }
        }

        if (skillName.empty() || skillIds.find(id) != skillIds.end() || nameMatches)
        {
            if (unlearn)
            {
                args["%skillname"] = ChatHelper::formatSkill(id);

                if (!bot->GetSkillInfo(uint16(id), ([](SkillRaceClassInfoEntry const& entry) { return (entry.flags & SKILL_FLAG_CAN_UNLEARN); })))
                {
                    ai->TellPlayerNoFacing(requester, BOT_TEXT2("Unable to unlearn %skillname", args));
                    return false;
                }

                bot->SetSkillStep(uint16(id), 0);

                ai->TellPlayerNoFacing(requester, BOT_TEXT2("Unlearned %skillname", args));

                return true;
            }

            ai->TellPlayerNoFacing(requester, ChatHelper::formatSkill(id, bot));

            skillFound = true;
        }
    }

    if (!skillFound)
    {
        if (skillName.empty())
        {
            ai->TellPlayerNoFacing(requester, BOT_TEXT2("I do not have skills.", args));
        }
        else
        {
            args["%skillname"] = skillName;
            ai->TellPlayerNoFacing(requester, BOT_TEXT2("I do not have %skillname.", args));
        }
        return false;
    }

    return true;
}
