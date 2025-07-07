#include "VoiceoverModule.h"

#include "Globals/ObjectAccessor.h"
#include "Globals/ObjectMgr.h"

#include "Entities/Player.h"
#include "Entities/GossipDef.h"

#include "World/World.h"

#ifdef ENABLE_PLAYERBOTS
#include "playerbot/PlayerbotAI.h"
#endif

namespace cmangos_module
{
    VoiceoverPlayerMgr::VoiceoverPlayerMgr(Player* inPlayer, VoiceoverModule* inModule)
    : player(inPlayer)
    , module(inModule)
    , addonEnabled(false)
    {
        
    }

    VoiceoverModule::VoiceoverModule()
    : Module("Voiceover", new VoiceoverModuleConfig())
    {

    }

    const VoiceoverModuleConfig* VoiceoverModule::GetConfig() const
    {
        return (VoiceoverModuleConfig*)Module::GetConfig();
    }

    void VoiceoverModule::OnCharacterCreated(Player* player)
    {
        if (GetConfig()->enabled)
        {
            if (player)
            {
#ifdef ENABLE_PLAYERBOTS
                // Don't allow bot characters
                if (!player->isRealPlayer())
                    return;
#endif
                // Create the player manager
                const uint32 playerId = player->GetObjectGuid().GetCounter();
                playerMgrs.insert(std::make_pair(playerId, VoiceoverPlayerMgr(player, this)));
            }
        }
    }

    void VoiceoverModule::OnPreLoadFromDB(Player* player)
    {
        if (GetConfig()->enabled)
        {
            if (player)
            {
#ifdef ENABLE_PLAYERBOTS
                // Don't allow bot characters
                if (!player->isRealPlayer())
                    return;
#endif
                // Create the player manager
                const uint32 playerId = player->GetObjectGuid().GetCounter();
                playerMgrs.insert(std::make_pair(playerId, VoiceoverPlayerMgr(player, this)));
            }
        }
    }

    void VoiceoverModule::OnLogOut(Player* player)
    {
        if (GetConfig()->enabled)
        {
            if (player)
            {
                // Delete the player achievement manager
                const uint32 playerId = player->GetObjectGuid().GetCounter();
                playerMgrs.erase(playerId);
            }
        }
    }

    std::string GetObjectTypeStrFromGuid(const ObjectGuid& guid)
    {
        switch (guid.GetHigh())
        {
            case HighGuid::HIGHGUID_PLAYER: return "Player";
            case HighGuid::HIGHGUID_ITEM: return "Item";
            case HighGuid::HIGHGUID_UNIT:
            case HighGuid::HIGHGUID_PET: return "Creature";
            case HighGuid::HIGHGUID_GAMEOBJECT: return "GameObject";
            default: return "";
        }
    }

    struct QuestInfo
    {
        bool valid = false;
        uint32 questID = 0;
        uint32 questgiverID = 0;
        std::string questTitle = "";
        std::string questGiverName = "";
        std::string questGiverGUID = "";
    };

    std::string GetQuestTitleByLocale(const Quest* quest, int localeIndex)
    {
        std::string questTitle;
        if (quest)
        {
            questTitle = quest->GetTitle();
            if (localeIndex >= 0)
            {
                if (const QuestLocale* questLocale = sObjectMgr.GetQuestLocale(quest->GetQuestId()))
                {
                    if (questLocale->Title.size() > (size_t)localeIndex && !questLocale->Title[localeIndex].empty())
                    {
                        questTitle = questLocale->Title[localeIndex];
                    }
                }
            }
        }

        return questTitle;
    }

    QuestInfo GetQuestInfo(const Player* player, const Quest* quest, const ObjectGuid* questGiverGuid = nullptr)
    {
        QuestInfo result;
        if (player && quest)
        {
            result.questID = quest->GetQuestId();

            std::string objectTypeStr = "Creature";
            if (questGiverGuid && !questGiverGuid->IsEmpty())
            {
                result.questgiverID = questGiverGuid->GetEntry();
                objectTypeStr = GetObjectTypeStrFromGuid(*questGiverGuid);
            }
            else
            {
                for (auto [entry, questID] : sObjectMgr.GetCreatureQuestRelationsMap())
                {
                    if (result.questID == questID)
                    {
                        result.questgiverID = entry;
                        break;
                    }
                }
            }

            const int localeIndex = player->GetSession()->GetSessionDbLocaleIndex();
            result.questTitle = GetQuestTitleByLocale(quest, localeIndex);

            if (localeIndex >= 0)
            {
                const char* questGiverName = nullptr;
                sObjectMgr.GetCreatureLocaleStrings(result.questgiverID, localeIndex, &questGiverName);
                result.questGiverName = questGiverName ? questGiverName : "";
            }

            if (result.questGiverName.empty())
            {
                if (const CreatureInfo* creatureInfo = sObjectMgr.GetCreatureTemplate(result.questgiverID))
                {
                    result.questGiverName = creatureInfo->Name;
                }
            }

            std::ostringstream guidStr;
            guidStr << objectTypeStr << "-0-0-0-0-" << result.questgiverID << "-0";
            result.questGiverGUID = guidStr.str();
            result.valid = true;
        }

        return result;
    }

    void VoiceoverModule::OnAcceptQuest(Player* player, uint32 questId, const ObjectGuid* questGiver)
    {
        if (GetConfig()->enabled && player)
        {
#ifdef ENABLE_PLAYERBOTS
            // Don't allow bot characters
            if (!player->isRealPlayer())
                return false;
#endif

            if (IsAddonEnabled(player))
            {
                if (const Quest* quest = sObjectMgr.GetQuestTemplate(questId))
                {
                    const uint8 wasAdded = 1;
                    const QuestInfo questInfo = GetQuestInfo(player, quest, questGiver);

                    PSendAddonMessage(player, "QuestLog#%u;%u;%s;%s;%s",
                        wasAdded,
                        questInfo.questID,
                        questInfo.questGiverGUID.c_str(),
                        questInfo.questTitle.c_str(),
                        questInfo.questGiverName.c_str());
                }
            }
        }
    }

    void VoiceoverModule::OnAbandonQuest(Player* player, uint32 questId)
    {
        if (GetConfig()->enabled && player)
        {
#ifdef ENABLE_PLAYERBOTS
            // Don't allow bot characters
            if (!player->isRealPlayer())
                return false;
#endif

            if (IsAddonEnabled(player))
            {
                if (const Quest* quest = sObjectMgr.GetQuestTemplate(questId))
                {
                    const uint8 wasAdded = 0;
                    const QuestInfo questInfo = GetQuestInfo(player, quest);

                    PSendAddonMessage(player, "QuestLog#%u;%u;%s;%s;%s",
                        wasAdded,
                        questInfo.questID,
                        questInfo.questGiverGUID.c_str(),
                        questInfo.questTitle.c_str(),
                        questInfo.questGiverName.c_str());
                }
            }
        }
    }

    void VoiceoverModule::SendAddonMessage(const Player* player, const char* message) const
    {
        if (IsAddonEnabled(player))
        {
            WorldPacket data;

            std::ostringstream out;
            out << GetChatCommandPrefix() << "\t" << message;

            char* buf = mangos_strdup(out.str().c_str());
            char* pos = buf;

            while (char* line = ChatHandler::LineFromMessage(pos))
            {
#if EXPANSION == 0
                ChatHandler::BuildChatPacket(data, CHAT_MSG_ADDON, line, LANG_ADDON);
#else
                ChatHandler::BuildChatPacket(data, CHAT_MSG_WHISPER, line, LANG_ADDON);
#endif
                player->GetSession()->SendPacket(data);
            }

            delete[] buf;
        }
    }

    void VoiceoverModule::PSendAddonMessage(const Player* player, const char* format, ...) const
    {
        if (player)
        {
            va_list ap;
            char str[2048];
            va_start(ap, format);
            vsnprintf(str, 2048, format, ap);
            va_end(ap);

            SendAddonMessage(player, str);
        }
    }

    std::vector<ModuleChatCommand>* VoiceoverModule::GetCommandTable()
    {
        static std::vector<ModuleChatCommand> commandTable =
        {
            { "enableAddon", std::bind(&VoiceoverModule::HandleEnableAddon, this, std::placeholders::_1, std::placeholders::_2), SEC_PLAYER },
            { "questLog", std::bind(&VoiceoverModule::HandleQuestLogRequest, this, std::placeholders::_1, std::placeholders::_2), SEC_PLAYER },
            { "soundEvent", std::bind(&VoiceoverModule::HandleSoundEventRequest, this, std::placeholders::_1, std::placeholders::_2), SEC_PLAYER }
        };

        return &commandTable;
    }

    bool VoiceoverModule::HandleEnableAddon(WorldSession* session, const std::string& args)
    {
        if (GetConfig()->enabled && session)
        {
            Player* player = session->GetPlayer();
            if (player)
            {
#ifdef ENABLE_PLAYERBOTS
                // Don't allow bot characters
                if (!player->isRealPlayer())
                    return false;
#endif

                bool addonEnabled = false;
                if (VoiceoverPlayerMgr* playerMgr = GetVoiceoverPlayerMgr(player))
                {
                    addonEnabled = playerMgr->IsAddonEnabled();
                    if (!addonEnabled)
                    {
                        playerMgr->EnableAddon();
                        addonEnabled = true;
                    }
                }

                if (addonEnabled)
                {
                    SendAddonMessage(player, "AddonEnabled");
                }

                return true;
            }
        }

        return false;
    }

    bool VoiceoverModule::HandleQuestLogRequest(WorldSession* session, const std::string& args)
    {
        if (GetConfig()->enabled && session)
        {
            Player* player = session->GetPlayer();
            if (player)
            {
#ifdef ENABLE_PLAYERBOTS
                // Don't allow bot characters
                if (!player->isRealPlayer())
                    return false;
#endif

                if (IsAddonEnabled(player))
                {
                    for (uint8 slot = 0; slot < MAX_QUEST_LOG_SIZE; ++slot)
                    {
                        if (const Quest* quest = sObjectMgr.GetQuestTemplate(player->GetQuestSlotQuestId(slot)))
                        {
                            const uint8 wasAdded = 1;
                            const QuestInfo questInfo = GetQuestInfo(player, quest);

                            PSendAddonMessage(player, "QuestLog#%u;%u;%s;%s;%s",
                                wasAdded,
                                questInfo.questID,
                                questInfo.questGiverGUID.c_str(),
                                questInfo.questTitle.c_str(),
                                questInfo.questGiverName.c_str());
                        }
                    }

                    return true;
                }
            }
        }

        return false;
    }

    std::string toLower(std::string str) 
    {
        std::transform(str.begin(), str.end(), str.begin(),
        [](unsigned char c) { return std::tolower(c); });
        return str;
    }

    bool VoiceoverModule::HandleSoundEventRequest(WorldSession* session, const std::string& args)
    {
        if (GetConfig()->enabled && session)
        {
            Player* player = session->GetPlayer();
            if (player)
            {
#ifdef ENABLE_PLAYERBOTS
                // Don't allow bot characters
                if (!player->isRealPlayer())
                    return false;
#endif

                if (IsAddonEnabled(player))
                {
                    std::vector<std::string> arguments = helper::SplitString(args, ";");
                    if (arguments.size() == 3)
                    {
                        const SoundEvent eventType = helper::IsValidNumberString(arguments[0]) ? static_cast<SoundEvent>(stoi(arguments[0])) : SoundEvent::INVALID;
                        uint32 id = helper::IsValidNumberString(arguments[1]) ? stoi(arguments[1]) : 0;
                        const std::string eventTitle = toLower(arguments[2]);

                        if (eventType != SoundEvent::INVALID)
                        {
                            const bool isQuestEvent = eventType == SoundEvent::QUEST_ACCEPT || eventType == SoundEvent::QUEST_PROGRESS || eventType == SoundEvent::QUEST_COMPLETE;
                            const ObjectGuid& targetGuid = player->GetSelectionGuid();
                            if (id == 0 && !targetGuid.IsEmpty())
                            {
                                // Try to guess the id based on the event type and the current target of the player
                                if (isQuestEvent && !eventTitle.empty())
                                {
                                    const int localeIndex = player->GetSession()->GetSessionDbLocaleIndex();
                                    if (PlayerMenu* playerMenu = player->GetPlayerMenu())
                                    {
                                        QuestMenu& questMenu = playerMenu->GetQuestMenu();
                                        
                                        for (uint32 i = 0; i < questMenu.MenuItemCount(); ++i)
                                        {
                                            const QuestMenuItem& menuItem = questMenu.GetItem(i);
                                            if ((eventType == SoundEvent::QUEST_ACCEPT && menuItem.m_qIcon == DIALOG_STATUS_AVAILABLE) ||
                                                (eventType == SoundEvent::QUEST_PROGRESS && menuItem.m_qIcon == DIALOG_STATUS_INCOMPLETE) ||
                                                (eventType == SoundEvent::QUEST_COMPLETE && menuItem.m_qIcon == DIALOG_STATUS_REWARD_REP))
                                            {
                                                const uint32 questID = menuItem.m_qId;
                                                if (const Quest* quest = sObjectMgr.GetQuestTemplate(questID))
                                                {
                                                    const std::string questTitle = toLower(GetQuestTitleByLocale(quest, localeIndex));
                                                    if (questTitle == eventTitle)
                                                    {
                                                        id = questID;
                                                        break;
                                                    }
                                                }
                                            }
                                        }
                                    }

                                    if (id == 0)
                                    {
                                        const uint32 targetEntry = targetGuid.GetEntry();
                                        QuestRelationsMapBounds bounds = sObjectMgr.GetCreatureQuestRelationsMapBounds(targetEntry);
                                        for (QuestRelationsMap::const_iterator itr = bounds.first; itr != bounds.second; ++itr)
                                        {
                                            const uint32 questID = itr->second;
                                            if (const Quest* quest = sObjectMgr.GetQuestTemplate(questID))
                                            {
                                                const std::string questTitle = toLower(GetQuestTitleByLocale(quest, localeIndex));
                                                if (questTitle == eventTitle)
                                                {
                                                    id = questID;
                                                    break;
                                                }
                                            }
                                        }
                                    }
                                }
                            }

                            if (id != 0)
                            {
                                if (isQuestEvent)
                                {
                                    if (const Quest* quest = sObjectMgr.GetQuestTemplate(id))
                                    {
                                        const QuestInfo questInfo = GetQuestInfo(player, quest, &targetGuid);

                                        PSendAddonMessage(player, "SoundEvent#%u;%u;%s;%s;%s",
                                            eventType,
                                            questInfo.questID,
                                            questInfo.questGiverGUID.c_str(),
                                            questInfo.questTitle.c_str(),
                                            questInfo.questGiverName.c_str());
                                    }
                                }
                            }
                        }
                    }

                    return true;
                }
            }
        }

        return false;
    }

    VoiceoverPlayerMgr* VoiceoverModule::GetVoiceoverPlayerMgr(Player* player)
    {
        if (GetConfig()->enabled)
        {
            VoiceoverPlayerMgr* playerMgr = nullptr;
            if (player)
            {
#ifdef ENABLE_PLAYERBOTS
                // Don't allow bot characters
                if (!player->isRealPlayer())
                    return false;
#endif

                const uint32 playerId = player->GetObjectGuid().GetCounter();
                auto playerMgrIt = playerMgrs.find(playerId);
                if (playerMgrIt != playerMgrs.end())
                {
                    playerMgr = &playerMgrIt->second;
                }

                MANGOS_ASSERT(playerMgr);
                return playerMgr;
            }
        }

        return nullptr;
    }

    const VoiceoverPlayerMgr* VoiceoverModule::GetVoiceoverPlayerMgr(const Player* player) const
    {
        if (GetConfig()->enabled)
        {
            const VoiceoverPlayerMgr* playerMgr = nullptr;
            if (player)
            {
#ifdef ENABLE_PLAYERBOTS
                // Don't allow bot characters
                if (!player->isRealPlayer())
                    return false;
#endif

                const uint32 playerId = player->GetObjectGuid().GetCounter();
                auto playerMgrIt = playerMgrs.find(playerId);
                if (playerMgrIt != playerMgrs.end())
                {
                    playerMgr = &playerMgrIt->second;
                }

                MANGOS_ASSERT(playerMgr);
                return playerMgr;
            }
        }

        return nullptr;
    }

    bool VoiceoverModule::IsAddonEnabled(const Player* player) const
    {
        if (player)
        {
            if (const VoiceoverPlayerMgr* playerMgr = GetVoiceoverPlayerMgr(player))
            {
                return playerMgr->IsAddonEnabled();
            }
        }

        return false;
    }
}