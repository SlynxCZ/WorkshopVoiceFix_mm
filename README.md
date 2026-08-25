# WorkshopVoiceFix

**Metamod plugin for CS2 — voice chat fix on workshop maps.**

Scaffolded from [ConsoleSpamFix_mm](https://github.com/SlynxCZ/ConsoleSpamFix_mm):
same build layout, same vendored SourceHook/DynLibUtils.

## What it does

CS2 clients key voice playback off the `xuid` field of `svc_VoiceData`. When that
field does not identify the speaker uniquely, streams collide and players stop
hearing each other. This gives every speaker a distinct xuid, per listener.

One SourceHook hook on `CServerSideClient::SendNetMessage`. That call runs **once
per recipient**, which is the point: the hook knows both who is talking (the
message) and who is being sent to (the client it was called on). Messages that
are not `svc_VoiceData` pass through untouched:

```cpp
NetMessageInfo_t *pInfo = pData->GetNetMessage()->GetNetMessageInfo();
if (!pInfo || pInfo->m_MessageId != SVC_Messages::svc_VoiceData)
    RETURN_META_VALUE(MRES_IGNORED, true);

CServerSideClient *pRecipient = META_IFACEPTR(CServerSideClient);
const int nRecipient = pRecipient->GetPlayerSlot().Get();

CSVCMsg_VoiceData *pMsg = /* pData->ToPB<CSVCMsg_VoiceData>(), const_cast'd */;
const int nSpeaker = pMsg->entity();

pMsg->set_xuid(SeedForRecipient(nRecipient) + (uint64)nSpeaker);
```

The message is rewritten in place in a pre-hook, so the engine serializes the
modified one into that client's channel.

`SeedForRecipient` hands out one random-based seed per **listener** slot, 66
apart. 66 is wider than the highest entity index, so `seed + entity` for one
listener can never land inside another's window. Seeds are cleared on every map
change (`INetworkServerService::StartupServer`).

Keying on the listener rather than the speaker is what makes reuse harmless: if a
slot is taken over by someone new mid-map, they inherit a seed, but they are a
fresh client with nothing cached, so the mapping they are handed is as good as a
new one.

### Installing the hook

`CServerSideClient` is an engine class with no interface to fetch and no instance
to hook at load time. The vtable is resolved out of the engine module by name and
hooked directly:

```cpp
CModule libengine(g_pEngineServer);
CMemory pCServerSideClientVTable = libengine.GetVirtualTableByName("CServerSideClient");

m_iSendNetMessageHookID = SH_ADD_DVPHOOK(CServerSideClient, SendNetMessage,
    pCServerSideClientVTable.RCast<CServerSideClient *>(),
    SH_MEMBER(this, &Plugin::CServerSideClient_SendNetMessage), false);
```

`SH_ADD_DVPHOOK` (`Hook_DVP`) takes its pointer as the vtable itself rather than
as an object, and leaves the hook's interface pointer null, so a single install
covers every client -- no waiting for the first connection, no per-instance
bookkeeping. `META_IFACEPTR` inside the handler still yields the actual `this`.

Both of those are version-dependent and neither fails loudly on its own, so
`Load()` bails with a message rather than hooking into nothing:

- the `CServerSideClient` RTTI name has to still be in the engine module, and
- `src/sdk/CServerSideClient.h` is a hand-reconstructed layout, so the vtable
  index SourceHook derives for `SendNetMessage` is only right while the virtual
  order above it matches the real class.

### Protobuf version

`CSVCMsg_VoiceData` changed upstream in 2026: field 2 `client` became
`client_deprecated` and field 8 `entity` was added. This uses `entity`, so it
needs a `$CSGO_PROTO` checkout newer than that. An older one fails to compile on
`entity()` rather than misbehaving at runtime, which is the good outcome.

## Building

Requires `HL2SDKCS2`, `MMSOURCE_DEV` and `CSGO_PROTO` in the environment, and
the submodules checked out (`git submodule update --init --recursive`).

CMake (local dev):

```
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo \
      -DCMAKE_C_COMPILER=clang-18 -DCMAKE_CXX_COMPILER=clang++-18
cmake --build build
```

Output lands in `build/addons/`, laid out ready to copy into the server's
`game/csgo/` directory.

AMBuild (what CI and the docker image run):

```
docker compose -f docker/docker-compose.yml up --build --abort-on-container-exit
```

Packaged output lands in `build/package/cs2`.

CI (`.github/workflows/main.yml`) builds Linux in the steamrt sniper container
and Windows with MSVC + AMBuild, and on a tag pushes
`WorkshopVoiceFix_mm_<tag>-{linux,windows}.tar.gz` to the release.

## SourceHook

Like the rest of the family, this plugin carries its **own private SourceHook**
(`vendor/sourcehook`, wired up via `sourcehook_metamod_override.h`) rather than
using metamod's shared instance.

That is mostly free here. The one slot this plugin patches is
`CServerSideClient::SendNetMessage`, which is not one of the usual targets --
CounterStrikeSharp and CS2Fixes go through `IGameEventSystem::PostEventAbstract`
for outgoing messages, a different vtable in a different class.

If that ever changes and something else patches the same slot through metamod's
shared engine, the two engines do not coordinate: whichever patched last owns the
slot, each treats what it found there as "the original", and `MRES_SUPERCEDE`
only suppresses its own chain. Ordering then comes down to plugin load order, and
unloading is order-sensitive too. The fix in that case is to put both sides on
one engine, not to change the hook.
