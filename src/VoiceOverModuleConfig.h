#pragma once
#include "ModuleConfig.h"

namespace cmangos_module
{
    class ClasslessModuleConfig : public ModuleConfig
    {
    public:
        ClasslessModuleConfig();
        bool OnLoad() override;

    public:
        bool enabled;
    };
}