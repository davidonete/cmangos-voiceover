#include "VoiceoverModule.h"

#include "Entities/Player.h"
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

    void VoiceoverModule::OnGossipQuestDetails(Player* player, const Quest* quest, const ObjectGuid& questGiverGuid)
    {
        if (GetConfig()->enabled && player && quest)
        {
#ifdef ENABLE_PLAYERBOTS
            // Don't allow bot characters
            if (!player->isRealPlayer())
                return false;
#endif

            if (IsAddonEnabled(player))
            {
                PSendAddonMessage(player, "Quest#%u;%u", quest->GetQuestId(), 1);
            }
        }
    }

    void VoiceoverModule::OnGossipQuestReward(Player* player, const Quest* quest, const ObjectGuid& questGiverGuid)
    {
        if (GetConfig()->enabled && player && quest)
        {
#ifdef ENABLE_PLAYERBOTS
            // Don't allow bot characters
            if (!player->isRealPlayer())
                return false;
#endif

            if (IsAddonEnabled(player))
            {
                PSendAddonMessage(player, "Quest#%u;%u", quest->GetQuestId(), 3);
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
            { "enableAddon", std::bind(&VoiceoverModule::HandleEnableAddon, this, std::placeholders::_1, std::placeholders::_2), SEC_PLAYER }
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