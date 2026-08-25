// Author: Michal Přikryl (Slynx) <github.com/SlynxCZ>

#include "plugin.h"

#include "netmessages.pb.h"

#include "eiface.h"
#include "iserver.h"
#include "playerslot.h"
#include "utlvector.h"
#include "interfaces/interfaces.h"
#include "engine/igameeventsystem.h"

#include <cstdio>
#include <cstring>
#include <random>

#define VERSION_STRING SEMVER " @ " GITHUB_SHA
#define BUILD_TIMESTAMP __DATE__ " " __TIME__

Plugin g_Plugin;
PLUGIN_EXPOSE(Plugin, g_Plugin);

IGameEventSystem* g_pGameEventSystem = nullptr;

class GameSessionConfiguration_t
{
};

SH_DECL_HOOK8_void(IGameEventSystem, PostEventAbstract, SH_NOATTRIB, 0, CSplitScreenSlot, bool, int, const uint64*, INetworkMessageInternal*, const CNetMessage*, unsigned long, NetChannelBufType_t);
SH_DECL_HOOK3_void(INetworkServerService, StartupServer, SH_NOATTRIB, 0, const GameSessionConfiguration_t&, ISource2WorldSession*, const char*);

bool Plugin::Load(PluginId id, ISmmAPI* ismm, char* error, size_t maxlen, bool late)
{
	PLUGIN_SAVEVARS();
	SH_METAMOD_OVERRIDE_SAVEVARS(id);

	GET_V_IFACE_CURRENT(GetEngineFactory, g_pGameEventSystem, IGameEventSystem, GAMEEVENTSYSTEM_INTERFACE_VERSION);
	GET_V_IFACE_CURRENT(GetEngineFactory, g_pNetworkServerService, INetworkServerService, NETWORKSERVERSERVICE_INTERFACE_VERSION);

	m_iPostEventAbstractHookID = SH_ADD_HOOK(IGameEventSystem, PostEventAbstract, g_pGameEventSystem, SH_MEMBER(this, &Plugin::IGameEventSystem_PostEventAbstract), false);
	m_iStartupServerHookID = SH_ADD_HOOK(INetworkServerService, StartupServer, g_pNetworkServerService, SH_MEMBER(this, &Plugin::INetworkServerService_StartupServer), true);

	return true;
}

bool Plugin::Unload(char* error, size_t maxlen)
{
	SH_REMOVE_HOOK_ID(m_iPostEventAbstractHookID);
	SH_REMOVE_HOOK_ID(m_iStartupServerHookID);

	return true;
}

void Plugin::IGameEventSystem_PostEventAbstract(CSplitScreenSlot nSlot, bool bLocalOnly, int nClientCount, const uint64* clients, INetworkMessageInternal* pEvent, const CNetMessage* pData, unsigned long nSize, NetChannelBufType_t bufType)
{
	if (!pEvent || !pData)
		RETURN_META(MRES_IGNORED);

	NetMessageInfo_t* pInfo = pEvent->GetNetMessageInfo();
	if (!pInfo || pInfo->m_MessageId != svc_VoiceData)
		RETURN_META(MRES_IGNORED);

	CSVCMsg_VoiceData* pMsg = const_cast<CSVCMsg_VoiceData*>(static_cast<const CSVCMsg_VoiceData*>(pData->ToPB<CSVCMsg_VoiceData>()));

	const int nSpeaker = pMsg->entity();

	pMsg->set_xuid(SeedForSpeaker(nSpeaker) + static_cast<uint64>(nSpeaker));

	if (!m_bLoggedFirstRewrite)
	{
		m_bLoggedFirstRewrite = true;
		META_CONPRINTF("[WorkshopVoiceFix] Rewriting svc_VoiceData xuid, first packet was from client %d\n", nSpeaker);
	}

	RETURN_META(MRES_IGNORED);
}

void Plugin::INetworkServerService_StartupServer(const GameSessionConfiguration_t& config, ISource2WorldSession* pWorldSession, const char* pszMapName)
{
	std::memset(m_PlayerSeeds, 0, sizeof(m_PlayerSeeds));
	m_bLoggedFirstRewrite = false;

	RETURN_META(MRES_IGNORED);
}

uint64 Plugin::SeedForSpeaker(int nEntity)
{
	if (nEntity < 0 || nEntity >= kVoiceEntityLimit)
		return 0;

	if (m_PlayerSeeds[nEntity] == 0)
	{
		if (m_nSeedCursor == 0)
		{
			std::random_device rd;
			std::mt19937_64 gen(rd());

			m_nSeedCursor = gen() | 1;
		}

		m_nSeedCursor += 66;
		m_PlayerSeeds[nEntity] = m_nSeedCursor;
	}

	return m_PlayerSeeds[nEntity];
}

///////////////////////////////////////
const char* Plugin::GetLicense()
{
	return "GPLv3";
}

const char* Plugin::GetVersion()
{
	return VERSION_STRING;
}

const char* Plugin::GetDate()
{
	return BUILD_TIMESTAMP;
}

const char* Plugin::GetLogTag()
{
	return "WorkshopVoiceFix";
}

const char* Plugin::GetAuthor()
{
	return "Slynx (˙·٠● S l y n x ●٠·˙)";
}

const char* Plugin::GetDescription()
{
	return "Gives every speaker a unique xuid on outgoing voice data";
}

const char* Plugin::GetName()
{
	return "Workshop voice fix";
}

const char* Plugin::GetURL()
{
	return "https://slynxdev.cz";
}
