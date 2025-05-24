#include "ClasslessModuleConfig.h"

namespace cmangos_module
{
    ClasslessModuleConfig::ClasslessModuleConfig()
    : ModuleConfig("classless.conf")
    , enabled(false)
    {

    }

    bool ClasslessModuleConfig::OnLoad()
    {
        enabled = config.GetBoolDefault("Classless.Enable", false);
        return true;
    }
}