# WorkshopVoiceFix

**Metamod plugin for CS2 — voice chat fix on workshop maps.**

Scaffolded from [ConsoleSpamFix_mm](https://github.com/SlynxCZ/ConsoleSpamFix_mm):
same build layout, same vendored SourceHook.

## What it does

CS2 clients key voice playback off the `xuid` field of `svc_VoiceData`. When
that field does not identify the speaker uniquely, streams collide and players
stop hearing each other. This gives every speaker its own xuid.

One SourceHook hook on `IGameEventSystem::PostEventAbstract` -- the path every
net message the server posts to clients goes through, voice included, which is
what that call's `NetChannelBufType_t` / `BUF_VOICE` is for. Messages that are
not `svc_VoiceData` pass through untouched:

```cpp
NetMessageInfo_t *pInfo = pEvent->GetNetMessageInfo();
if (!pInfo || pInfo->m_MessageId != svc_VoiceData)
    RETURN_META(MRES_IGNORED);

CSVCMsg_VoiceData *pMsg = /* pData->ToPB<CSVCMsg_VoiceData>(), const_cast'd */;
const int nSpeaker = pMsg->client();

pMsg->set_xuid(SeedForSpeaker(nSpeaker) + (uint64)nSpeaker);
```

The message is rewritten in place in a pre-hook, so the engine serializes the
modified one.

`SeedForSpeaker` hands out one random-based seed per player slot, 66 apart. 66
is wider than the highest slot index, so `seed + slot` for one speaker can never
land inside another's window. Seeds are cleared on every map change
(`INetworkServerService::StartupServer`), since slots get reassigned to
different people across a map and clients cache the xuid association.

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

If a hook added here lands on a vtable slot that other plugins on the same
server also hook through metamod's shared engine (CounterStrikeSharp, CS2Fixes),
the two engines do not coordinate: whichever patched last owns the slot, each
treats what it found there as "the original", and `MRES_SUPERCEDE` only
suppresses its own chain. Ordering then comes down to plugin load order, and
unloading is order-sensitive too. Worth checking against whatever the voice
hooks end up being.
