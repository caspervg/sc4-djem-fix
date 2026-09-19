#pragma once

#include <cRZMessage2COMDirector.h>
#include <filesystem>

#include "DjemFix.h"
#include "utils/Settings.h"

class cIGZMessage2;
class cIGZCOM;

class SC4DjemFixDirector final : public cRZMessage2COMDirector
{
public:
    SC4DjemFixDirector();
    ~SC4DjemFixDirector() override;

    [[nodiscard]] uint32_t GetDirectorID() const override;
    bool OnStart(cIGZCOM* pCOM) override;
    bool PreFrameWorkInit() override;
    bool PreAppInit() override;
    bool PostAppInit() override;
    bool PreAppShutdown() override;
    bool PostAppShutdown() override;
    bool PostSystemServiceShutdown() override;
    bool AbortiveQuit() override;
    bool OnInstall() override;
    bool DoMessage(cIGZMessage2* pMsg) override;

private:
	static std::filesystem::path GetDllDirectory_();
	static std::filesystem::path GetLogDirectory_();
	void InitializeLogger_();
	void Shutdown_() noexcept;

	Settings settings_{};
	Djem::Fix djemFix_{};
	bool frameworkHookInstalled_ = false;
};
