#include "ClasslessModule.h"

#include "Entities/Player.h"
#include "World/World.h"

#ifdef ENABLE_PLAYERBOTS
#include "playerbot/PlayerbotAI.h"
#endif

namespace cmangos_module
{
    ClasslessPlayerMgr::ClasslessPlayerMgr(Player* inPlayer, ClasslessModule* inModule)
    : player(inPlayer)
    , module(inModule)
    , addonEnabled(false)
    {
        
    }

    ClasslessModule::ClasslessModule()
    : Module("Classless", new ClasslessModuleConfig())
    {

    }

    const ClasslessModuleConfig* ClasslessModule::GetConfig() const
    {
        return (ClasslessModuleConfig*)Module::GetConfig();
    }

    void ClasslessModule::OnCharacterCreated(Player* player)
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
                playerMgrs.insert(std::make_pair(playerId, ClasslessPlayerMgr(player, this)));
            }
        }
    }

    void ClasslessModule::OnPreLoadFromDB(Player* player)
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
                playerMgrs.insert(std::make_pair(playerId, ClasslessPlayerMgr(player, this)));
            }
        }
    }

    void ClasslessModule::OnLogOut(Player* player)
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

    void ClasslessModule::OnGetPlayerClassLevelInfo(Player* player, PlayerClassLevelInfo& info)
    {
        if (GetConfig()->enabled && player)
        {
#ifdef ENABLE_PLAYERBOTS
            // Don't allow bot characters
            if (!player->isRealPlayer())
                return;
#endif

            // Add a minimum base mana for all classes
            info.basemana = 100;
        }
    }

    void CalculatePowerRegen(Player* player, float& outManaRegen, float& outManaRegenInterrupt)
    {
        outManaRegen = 0.0f;
        outManaRegenInterrupt = 0.0f;

        if (player)
        {
            // Mana regen from spirit
            float powerRegen = player->OCTRegenMPPerSpirit();

            // Simulate mana regen for warriors and rogues
            if (powerRegen <= 0.0f)
            {
                const float spirit = player->GetStat(STAT_SPIRIT);
                powerRegen = (spirit / 5 + 15) / 2.0f;
            }

            // Apply PCT bonus from SPELL_AURA_MOD_POWER_REGEN_PERCENT aura on spirit base regen
            powerRegen *= player->GetTotalAuraMultiplierByMiscValue(SPELL_AURA_MOD_POWER_REGEN_PERCENT, POWER_MANA);

            // Mana regen from SPELL_AURA_MOD_POWER_REGEN aura
            float powerRegenMP5 = player->GetTotalAuraModifierByMiscValue(SPELL_AURA_MOD_POWER_REGEN, POWER_MANA) / 5.0f;

            // Set regen rate in cast state apply only on spirit based regen
            int32 modManaRegenInterrupt = player->GetTotalAuraModifier(SPELL_AURA_MOD_MANA_REGEN_INTERRUPT);
            if (modManaRegenInterrupt > 100)
            {
                modManaRegenInterrupt = 100;
            }

            outManaRegenInterrupt = powerRegenMP5 + powerRegen * modManaRegenInterrupt / 100.0f;
            outManaRegen = powerRegenMP5 + powerRegen;
        }
    }

    void ClasslessModule::OnRegenerate(Player* player, uint8 power, uint32 diff, float& addedValue)
    {
        if (GetConfig()->enabled && player)
        {
#ifdef ENABLE_PLAYERBOTS
            // Don't allow bot characters
            if (!player->isRealPlayer())
                return false;
#endif
            
            // Simulate mana regen for rogues and warriors
            const uint8 cls = player->getClass();
            if (power == POWER_MANA && addedValue == 0.0f && (cls == CLASS_WARRIOR || cls == CLASS_ROGUE))
            {
                float modManaRegen = 0.0f;
                float modManaRegenInterrupt = 0.0f;
                CalculatePowerRegen(player, modManaRegen, modManaRegenInterrupt);

                const bool recentCast = player->IsUnderLastManaUseEffect();
                const float ManaIncreaseRate = sWorld.getConfig(CONFIG_FLOAT_RATE_POWER_MANA);
                if (recentCast)
                {
                    addedValue = modManaRegenInterrupt * ManaIncreaseRate * uint32(float(diff) / 1000);
                }
                else
                {
                    addedValue = modManaRegen * ManaIncreaseRate * uint32(float(diff) / 1000);
                }
            }
        }
    }

    void ClasslessModule::OnSetPower(Unit* unit, uint8 power, uint32& value)
    {
        if (GetConfig()->enabled && unit && unit->GetTypeId() == TYPEID_PLAYER)
        {
            Player* player = static_cast<Player*>(unit);

#ifdef ENABLE_PLAYERBOTS
            // Don't allow bot characters
            if (!player->isRealPlayer())
                return false;
#endif

            // Send updated power to client
            const uint8 cls = player->getClass();
            const bool rageClass = cls == CLASS_WARRIOR;
            const bool energyClass = cls == CLASS_ROGUE;
            const bool manaClass = !rageClass && !energyClass;
            if ((!manaClass && power == POWER_MANA) || (!rageClass && power == POWER_RAGE) || (!energyClass && power == POWER_ENERGY))
            {
                uint32 curValue = value;
                uint32 maxValue = player->GetMaxPower((Powers)(power));

                if (power == POWER_RAGE)
                {
                    curValue /= 10;
                    maxValue /= 10;
                }

                PSendAddonMessage(player, "PowerUpdate#%u;%u;%u", power, curValue, maxValue);
            }
        }
    }

    void ClasslessModule::SendAddonMessage(const Player* player, const char* message) const
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

    void ClasslessModule::PSendAddonMessage(const Player* player, const char* format, ...) const
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

    std::vector<ModuleChatCommand>* ClasslessModule::GetCommandTable()
    {
        static std::vector<ModuleChatCommand> commandTable =
        {
            { "enableAddon", std::bind(&ClasslessModule::HandleEnableAddon, this, std::placeholders::_1, std::placeholders::_2), SEC_PLAYER }
        };

        return &commandTable;
    }

    bool ClasslessModule::HandleEnableAddon(WorldSession* session, const std::string& args)
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
                if (ClasslessPlayerMgr* playerMgr = GetClasslessPlayerMgr(player))
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

    ClasslessPlayerMgr* ClasslessModule::GetClasslessPlayerMgr(Player* player)
    {
        if (GetConfig()->enabled)
        {
            ClasslessPlayerMgr* playerMgr = nullptr;
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

    const ClasslessPlayerMgr* ClasslessModule::GetClasslessPlayerMgr(const Player* player) const
    {
        if (GetConfig()->enabled)
        {
            const ClasslessPlayerMgr* playerMgr = nullptr;
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

    bool ClasslessModule::IsAddonEnabled(const Player* player) const
    {
        if (player)
        {
            if (const ClasslessPlayerMgr* playerMgr = GetClasslessPlayerMgr(player))
            {
                return playerMgr->IsAddonEnabled();
            }
        }

        return false;
    }
}