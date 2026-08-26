// Author: Michal Přikryl (Slynx) <github.com/SlynxCZ>

#include "plugin.h"

#include "CServerSideClient.h"
#include "netmessages.pb.h"

#include "module.hpp"

#include "eiface.h"
#include "iserver.h"
#include "playerslot.h"
#include "interfaces/interfaces.h"

#include <cstdio>
#include <cstring>
#include <random>

#define VERSION_STRING SEMVER " @ " GITHUB_SHA
#define BUILD_TIMESTAMP __DATE__ " " __TIME__

using namespace DynLibUtils;

Plugin g_Plugin;
PLUGIN_EXPOSE(Plugin, g_Plugin);

class GameSessionConfiguration_t
{
};

SH_DECL_HOOK2(CServerSideClient, SendNetMessage, SH_NOATTRIB, 0, bool, const CNetMessage*, NetChannelBufType_t);
SH_DECL_HOOK3_void(INetworkServerService, StartupServer, SH_NOATTRIB, 0, const GameSessionConfiguration_t&, ISource2WorldSession*, const char*);

bool Plugin::Load(PluginId id, ISmmAPI* ismm, char* error, size_t maxlen, bool late)
{
	PLUGIN_SAVEVARS();
	SH_METAMOD_OVERRIDE_SAVEVARS(id);

	GET_V_IFACE_CURRENT(GetEngineFactory, g_pEngineServer, IVEngineServer2, INTERFACEVERSION_VENGINESERVER);
	GET_V_IFACE_CURRENT(GetEngineFactory, g_pNetworkServerService, INetworkServerService, NETWORKSERVERSERVICE_INTERFACE_VERSION);

	CModule libengine(g_pEngineServer);

	CMemory pVTable = libengine.GetVirtualTableByName("CServerSideClient");

	m_iSendNetMessageHookID = SH_ADD_DVPHOOK(CServerSideClient, SendNetMessage, pVTable.RCast<CServerSideClient*>(), SH_MEMBER(this, &Plugin::CServerSideClient_SendNetMessage), false);
	m_iStartupServerHookID = SH_ADD_HOOK(INetworkServerService, StartupServer, g_pNetworkServerService, SH_MEMBER(this, &Plugin::INetworkServerService_StartupServer), true);

	return true;
}

bool Plugin::Unload(char* error, size_t maxlen)
{
	SH_REMOVE_HOOK_ID(m_iSendNetMessageHookID);
	SH_REMOVE_HOOK_ID(m_iStartupServerHookID);

	return true;
}

bool Plugin::CServerSideClient_SendNetMessage(const CNetMessage* pData, NetChannelBufType_t bufType)
{
	CServerSideClient* pThis = META_IFACEPTR(CServerSideClient);

	if (pData)
	{
		INetworkMessageInternal* pNetMsg = pData->GetNetMessage();
		if (pNetMsg)
		{
			NetMessageInfo_t* pInfo = pNetMsg->GetNetMessageInfo();
			if (pInfo && pInfo->m_MessageId == SVC_Messages::svc_VoiceData)
			{
				// The reference implementation's "playerid": the client being sent to, not
				// the one talking.
				const int nRecipient = pThis->GetPlayerSlot().Get();

				CSVCMsg_VoiceData* pMsg = const_cast<CSVCMsg_VoiceData*>(static_cast<const CSVCMsg_VoiceData*>(pData->ToPB<CSVCMsg_VoiceData>()));

				// And its "msg.Entity": the speaker.
				const int nSpeaker = pMsg->entity();

				pMsg->set_xuid(SeedForRecipient(nRecipient) + static_cast<uint64>(nSpeaker));

				if (!m_bLoggedFirstRewrite)
				{
					m_bLoggedFirstRewrite = true;
					META_LOG(this, "Rewriting svc_VoiceData xuid, first packet was entity %d -> slot %d\n", nSpeaker, nRecipient);
				}
			}
		}
	}

	RETURN_META_VALUE(MRES_IGNORED, true);
}

void Plugin::INetworkServerService_StartupServer(const GameSessionConfiguration_t& config, ISource2WorldSession* pWorldSession, const char* pszMapName)
{
	std::memset(m_PlayerSeeds, 0, sizeof(m_PlayerSeeds));
	m_bLoggedFirstRewrite = false;

	RETURN_META(MRES_IGNORED);
}

uint64 Plugin::SeedForRecipient(int nSlot)
{
	if (nSlot < 0 || nSlot >= ABSOLUTE_PLAYER_LIMIT)
		return 0;

	if (m_PlayerSeeds[nSlot] == 0)
	{
		if (m_nSeedCursor == 0)
		{
			std::random_device rd;
			std::mt19937_64 gen(rd());

			m_nSeedCursor = gen() | 1;
		}

		m_nSeedCursor += 66;
		m_PlayerSeeds[nSlot] = m_nSeedCursor;
	}

	return m_PlayerSeeds[nSlot];
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
