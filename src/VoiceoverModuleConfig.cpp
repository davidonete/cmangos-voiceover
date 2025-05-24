#include "VoiceoverModuleConfig.h"

namespace cmangos_module
{
    VoiceoverModuleConfig::VoiceoverModuleConfig()
    : ModuleConfig("voiceover.conf")
    , enabled(false)
    {

    }

    bool VoiceoverModuleConfig::OnLoad()
    {
        enabled = config.GetBoolDefault("Voiceover.Enable", false);
        return true;
    }
}