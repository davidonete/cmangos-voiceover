#ifndef CMANGOS_MODULE_CLASSLESS_H
#define CMANGOS_MODULE_CLASSLESS_H

#include "Module.h"
#include "ClasslessModuleConfig.h"

namespace cmangos_module
{
    class ClasslessModule;

    class ClasslessPlayerMgr
    {
    public:
        explicit ClasslessPlayerMgr(Player* inPlayer, ClasslessModule* inModule);
        ~ClasslessPlayerMgr() {}

        void EnableAddon() { addonEnabled = true; }
        bool IsAddonEnabled() const { return addonEnabled; }

    private:
        Player* player;
        ClasslessModule* module;
        bool addonEnabled;
    };

    class ClasslessModule : public Module
    {
    public:
        ClasslessModule();
        const ClasslessModuleConfig* GetConfig() const override;

        // Player Hooks
        void OnCharacterCreated(Player* player) override;
        void OnPreLoadFromDB(Player* player) override;
        void OnLogOut(Player* player) override;

        // Player Action Hooks
        void OnGetPlayerClassLevelInfo(Player* player, PlayerClassLevelInfo& info) override;
        void OnRegenerate(Player* player, uint8 power, uint32 diff, float& addedValue) override;

        // Unit Hooks
        void OnSetPower(Unit* unit, uint8 power, uint32& value) override;

        std::vector<ModuleChatCommand>* GetCommandTable() override;
        const char* GetChatCommandPrefix() const override { return "classless"; }
        bool HandleEnableAddon(WorldSession* session, const std::string& args);

    private:
        ClasslessPlayerMgr* GetClasslessPlayerMgr(Player* player);
        const ClasslessPlayerMgr* GetClasslessPlayerMgr(const Player* player) const;

        bool IsAddonEnabled(const Player* player) const;

        void SendAddonMessage(const Player* player, const char* message) const;
        void PSendAddonMessage(const Player* player, const char* format, ...) const;

    private:
        std::unordered_map<uint32, ClasslessPlayerMgr> playerMgrs;
    };
}
#endif
