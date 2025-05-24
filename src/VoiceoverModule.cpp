#include "Voiceover.h"

#include "Entities/Player.h"
#include "World/World.h"

#ifdef ENABLE_PLAYERBOTS
#include "playerbot/PlayerbotAI.h"
#endif

namespace cmangos_module
{
    VoiceOverPlayerMgr::VoiceOverPlayerMgr(Player* inPlayer, Voiceover* inModule)
    : player(inPlayer)
    , module(inModule)
    , addonEnabled(false)
    {
        
    }

    Voiceover::Voiceover()
    : Module("VoiceOver", new VoiceoverConfig())
    {

    }

    const VoiceoverConfig* Voiceover::GetConfig() const
    {
        return (VoiceoverConfig*)Module::GetConfig();
    }

    void Voiceover::OnCharacterCreated(Player* player)
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
                playerMgrs.insert(std::make_pair(playerId, VoiceOverPlayerMgr(player, this)));
            }
        }
    }

    void Voiceover::OnPreLoadFromDB(Player* player)
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
                playerMgrs.insert(std::make_pair(playerId, VoiceOverPlayerMgr(player, this)));
            }
        }
    }

    void Voiceover::OnLogOut(Player* player)
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

    void Voiceover::SendAddonMessage(const Player* player, const char* message) const
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

    void Voiceover::PSendAddonMessage(const Player* player, const char* format, ...) const
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

    std::vector<ModuleChatCommand>* Voiceover::GetCommandTable()
    {
        static std::vector<ModuleChatCommand> commandTable =
        {
            { "enableAddon", std::bind(&Voiceover::HandleEnableAddon, this, std::placeholders::_1, std::placeholders::_2), SEC_PLAYER }
        };

        return &commandTable;
    }

    bool Voiceover::HandleEnableAddon(WorldSession* session, const std::string& args)
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
                if (VoiceOverPlayerMgr* playerMgr = GetVoiceOverPlayerMgr(player))
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

                    // Send the initial power values to initialize the client power bars
                    uint32 powerValue = player->GetPower(POWER_MANA);
                    OnSetPower(player, POWER_MANA, powerValue);

                    powerValue = player->GetPower(POWER_RAGE);
                    OnSetPower(player, POWER_RAGE, powerValue);

                    powerValue = player->GetPower(POWER_ENERGY);
                    OnSetPower(player, POWER_ENERGY, powerValue);
                }

                return true;
            }
        }

        return false;
    }

    VoiceOverPlayerMgr* Voiceover::GetVoiceOverPlayerMgr(Player* player)
    {
        if (GetConfig()->enabled)
        {
            VoiceOverPlayerMgr* playerMgr = nullptr;
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

    const VoiceOverPlayerMgr* Voiceover::GetVoiceOverPlayerMgr(const Player* player) const
    {
        if (GetConfig()->enabled)
        {
            const VoiceOverPlayerMgr* playerMgr = nullptr;
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

    bool Voiceover::IsAddonEnabled(const Player* player) const
    {
        if (player)
        {
            if (const VoiceOverPlayerMgr* playerMgr = GetVoiceOverPlayerMgr(player))
            {
                return playerMgr->IsAddonEnabled();
            }
        }

        return false;
    }
}