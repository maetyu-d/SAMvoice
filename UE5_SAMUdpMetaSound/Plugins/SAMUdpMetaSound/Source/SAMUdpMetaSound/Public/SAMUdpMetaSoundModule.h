#pragma once

#include "Modules/ModuleManager.h"

class FSAMUdpMetaSoundModule final : public IModuleInterface
{
public:
    void StartupModule() override;
    void ShutdownModule() override;
};
