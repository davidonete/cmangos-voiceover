#ifndef CMANGOS_MODULE_VOICEOVER_H
#define CMANGOS_MODULE_VOICEOVER_H

#include "Module.h"
#include "VoiceoverModuleConfig.h"

namespace cmangos_module
{
    enum class SoundEvent : uint8
    {
        QUEST_ACCEPT = 1,
        QUEST_PROGRESS = 2,
        QUEST_COMPLETE = 3,
        QUEST_GREETING = 4,
        GOSSIP = 5,
        INVALID = 6
    };

    class VoiceoverModule;

    class VoiceoverPlayerMgr
    {
    public:
        explicit VoiceoverPlayerMgr(Player* inPlayer, VoiceoverModule* inModule);
        ~VoiceoverPlayerMgr() {}

        void EnableAddon() { addonEnabled = true; }
        bool IsAddonEnabled() const { return addonEnabled; }

    private:
        Player* player;
        VoiceoverModule* module;
        bool addonEnabled;
    };

    class VoiceoverModule : public Module
    {
    public:
        VoiceoverModule();
        const VoiceoverModuleConfig* GetConfig() const override;

        // Player Hooks
        void OnCharacterCreated(Player* player) override;
        void OnPreLoadFromDB(Player* player) override;
        void OnLogOut(Player* player) override;

        // Player Action Hooks
        void OnAcceptQuest(Player* player, uint32 questId, const ObjectGuid* questGiver);
        void OnAbandonQuest(Player* player, uint32 questId);

        std::vector<ModuleChatCommand>* GetCommandTable() override;
        const char* GetChatCommandPrefix() const override { return "voiceover"; }
        bool HandleEnableAddon(WorldSession* session, const std::string& args);
        bool HandleQuestLogRequest(WorldSession* session, const std::string& args);
        bool HandleSoundEventRequest(WorldSession* session, const std::string& args);

    private:
        VoiceoverPlayerMgr* GetVoiceoverPlayerMgr(Player* player);
        const VoiceoverPlayerMgr* GetVoiceoverPlayerMgr(const Player* player) const;

        bool IsAddonEnabled(const Player* player) const;

        void SendAddonMessage(const Player* player, const char* message) const;
        void PSendAddonMessage(const Player* player, const char* format, ...) const;

    private:
        std::unordered_map<uint32, VoiceoverPlayerMgr> playerMgrs;
    };
}
#endif
