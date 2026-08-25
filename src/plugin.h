#ifndef _INCLUDE_WORKSHOP_VOICE_FIX_PLUGIN_SLYNX_H_
#define _INCLUDE_WORKSHOP_VOICE_FIX_PLUGIN_SLYNX_H_
#ifdef _WIN32
#pragma once
#endif

#include "inetchannel.h"
#include "ISmmPlugin.h"

// Redirects SH_GLOB_SHPTR/SH_GLOB_PLUGPTR onto a private, plugin-owned
// SourceHook engine (vendor/sourcehook) instead of metamod's shared
// g_SHPtr/g_PLID -- must come after ISmmPlugin.h (which is what defines the
// defaults this overrides) and before any SH_DECL_HOOK*/SH_ADD_*HOOK usage.
#include "sourcehook/sourcehook_metamod_override.h"

#include "const.h"
#include "tier1/convar.h"
#include "networksystem/inetworkserializer.h"

#include <chrono>
#include <cstdint>

class CNetMessage;
class GameSessionConfiguration_t;
class ISource2WorldSession;

class Plugin final : public ISmmPlugin
{
public:
	bool Load(PluginId id, ISmmAPI* ismm, char* error, size_t maxlen, bool late) override;
	bool Unload(char* error, size_t maxlen) override;

private: // Hooks
	bool CServerSideClient_SendNetMessage(const CNetMessage* pData, NetChannelBufType_t bufType);
	void INetworkServerService_StartupServer(const GameSessionConfiguration_t& config, ISource2WorldSession* pWorldSession, const char* pszMapName);

	int m_iSendNetMessageHookID = 0;
	int m_iStartupServerHookID = 0;

private:
	uint64 SeedForRecipient(int nSlot);

	// Keyed by the slot of the client the voice packet is being sent *to*, and
	// the speaker's entity index is what gets added on top. Seeds are handed
	// out 66 apart -- wider than any entity index -- so two recipients' xuid
	// windows can never overlap.
	uint64 m_PlayerSeeds[ABSOLUTE_PLAYER_LIMIT] = {};
	uint64 m_nSeedCursor = 0;
	bool m_bLoggedFirstRewrite = false;

private:
	const char* GetAuthor() override;
	const char* GetName() override;
	const char* GetDescription() override;
	const char* GetURL() override;
	const char* GetLicense() override;
	const char* GetVersion() override;
	const char* GetDate() override;
	const char* GetLogTag() override;
};

extern Plugin g_Plugin;

PLUGIN_GLOBALVARS();

#endif // _INCLUDE_WORKSHOP_VOICE_FIX_PLUGIN_SLYNX_H_
