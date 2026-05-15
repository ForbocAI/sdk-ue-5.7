<br/>
<div align="center">
  <img alt="ForbocAI logo" src="https://forboc.ai/logo.png" height="50" align="center">

  <br/>

# ForbocAI SDK for Unreal Engine 5.7

Drop-in autonomous NPCs for Unreal Engine 5.7 — neuro-symbolic agents with persistent memory, identity, and ruleset-aware decision making, callable from C++ and Blueprints.

[![Documentation](https://img.shields.io/badge/docs-docs.forboc.ai-blue)](https://docs.forboc.ai)
[![Fab](https://img.shields.io/badge/Fab-ForbocAI-orange)](https://fab.com)

</div>

---

## What you get

- **Agents**: Persona-driven NPCs you can spawn, update, and delete from a single factory call.
- **Memory**: Embedding-backed recall so agents remember past interactions across sessions.
- **Bridge**: Validate agent-proposed actions against your game's ruleset before they fire.
- **Souls**: Export and re-import an agent's identity (JSON) — portable across projects and saves.
- **Speech & dialogue hooks**: Drop-in components for TTS, viseme blending, and chat UI.
- **Blueprint surface**: All public operations exposed as `BlueprintCallable` nodes.
- **CLI**: `doctor`, `npc_list`, `npc_create`, `npc_process`, `soul_export`, and friends, runnable through `UnrealEditor-Cmd` for smoke-testing in CI or after install.

NPC reasoning is hosted on the ForbocAI API; the plugin handles local capabilities (memory, identification, soul transport, command surface) and talks to the API over HTTP.

---

## Installation

### Option 1 — Fab (recommended)

1. Search for **ForbocAI** on [Fab](https://fab.com).
2. Add to library and install to your engine.
3. Enable the plugin in your project: `Edit → Plugins → ForbocAI`.

### Option 2 — Manual

1. Download a release archive from the [Releases](https://github.com/ForbocAI/sdk-ue-5.7/releases) page.
2. Copy the `ForbocAI_SDK` folder into your project's `Plugins/` directory.
3. Right-click your `.uproject` and **Generate Visual Studio project files**.
4. Build in `Development Editor`.

### Prerequisites

| Platform | Tools |
|---|---|
| Windows | UE 5.7, [VS Build Tools 2022](https://aka.ms/vs/17/release/vs_buildtools.exe) (C++ workload + Windows 11 SDK) |
| macOS | UE 5.7, Xcode 15+ |
| Linux | UE 5.7, Clang 16+ |

The plugin reaches an API endpoint at runtime. By default it tries `http://localhost:8080`, then falls back to `https://api.forboc.ai`. Override with `FAgentConfig::ApiUrl` or via the SDK config.

---

## Quick start (C++)

```cpp
#include "AgentModule.h"
#include "MemoryModule.h"

// 1. Create an agent
FAgentConfig Config;
Config.Persona = TEXT("Cyber-Merchant");
// Config.ApiUrl is optional — defaults to localhost, falls back to api.forboc.ai.

const FAgent Merchant = AgentFactory::Create(Config);

// 2. Process player input asynchronously
AgentOps::Process(
    Merchant, TEXT("What wares do you have?"), {},
    [](FAgentResponse Response) {
        UE_LOG(LogTemp, Log, TEXT("Reply: %s"), *Response.Dialogue);
    });

// 3. Update agent state — returns a NEW agent (originals stay untouched)
const FAgentState Suspicious = TypeFactory::AgentState(TEXT("Suspicious"));
const FAgent Updated = AgentOps::WithState(Merchant, Suspicious);

// 4. Memory store — add an interaction
const FMemoryStore Store = MemoryOps::CreateStore();
const FMemoryStore After = MemoryOps::Store(
    Store, TEXT("Customer asked about wares"), TEXT("interaction"), 0.8f);
```

> All public types are immutable structs. Operations return new values rather than mutating in place — assign the result back if you want to keep it.

## Quick start (Blueprint)

1. Create a new Blueprint Actor parented to `ASDKTestActor` (shipped with the demo project).
2. In **Class Defaults**, set **Persona** (and optionally **Api Url**).
3. Implement **Event On Agent Response** to consume dialogue.
4. Call **Process Input** from any input or UI event.

The full demo project lives at [`ForbocAI/demo-ue-5.7`](https://github.com/ForbocAI/demo-ue-5.7) and shows multi-bot orchestration, dialogue, speech, and ruleset-aware combat encounters.

---

## CLI smoke tests

The plugin ships a Commandlet you can run via `UnrealEditor-Cmd` to verify the install and exercise the API path without launching the editor UI.

### Windows

```powershell
& "C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" `
  "C:\Path\To\Your.uproject" `
  -run=ForbocAI -Command=doctor `
  -nosplash -nopause -unattended
```

### macOS

```bash
"/Users/Shared/Epic Games/UE_5.7/Engine/Binaries/Mac/UnrealEditor-Cmd" \
  "/Path/To/Your.uproject" \
  -run=ForbocAI -Command=doctor \
  -nosplash -nopause -unattended
```

### Linux

```bash
"/opt/UnrealEngine/Engine/Binaries/Linux/UnrealEditor-Cmd" \
  "/path/to/Your.uproject" \
  -run=ForbocAI -Command=doctor \
  -nosplash -nopause -unattended
```

The first run may take a few minutes (DDC warm-up, shader compile). Subsequent runs are ~30 s.

| Command | Purpose |
|---|---|
| `doctor` | Check API connectivity and report SDK version |
| `npc_list` | List active agents |
| `npc_create -Persona="..."` | Create a new agent |
| `npc_process -Id="..." -Input="..."` | Send input to an agent |
| `soul_export -Id="..."` | Export an agent's soul to JSON |
| `config_set -Key="..." -Value="..."` | Persist a CLI config value |
| `config_get -Key="..."` | Read a stored CLI config value |

Sample `doctor` output:

```
ForbocAI CLI (UE5) — Command: doctor
API Status: online (v0.4.0)
```

---

## Documentation & support

- Full reference, tutorials, and protocol docs: <https://docs.forboc.ai>
- Demo project: <https://github.com/ForbocAI/demo-ue-5.7>
- Issues and feature requests: <https://github.com/ForbocAI/sdk-ue-5.7/issues>

---

## License

© 2026 ForbocAI, Inc. All rights reserved. See [LICENSE](./LICENSE) for full terms.
