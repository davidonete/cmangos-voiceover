#pragma once
#include "ModuleConfig.h"

namespace cmangos_module
{
    class VoiceoverModuleConfig : public ModuleConfig
    {
    public:
        VoiceoverModuleConfig();
        bool OnLoad() override;

    public:
        bool enabled;
    };
}