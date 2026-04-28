#pragma once
/**
 * Test-game lib modules — mirrors TS lib/commandRunner.ts, render.ts, coverage.ts
 * Command execution, ASCII grid rendering, runtime connectivity checks
 * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
 */

#include "CLI/CliOperations.h"
#include "Core/AsyncHttp.h"
#include "CoreMinimal.h"
#include "RuntimeStore.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "TestGame/TestGameStore.h"
#include "TestGame/TestGameTypes.h"
#include "Core/functional_core.hpp"
#include <exception>

namespace TestGame {

/**
 * Command Runner — mirrors TS lib/commandRunner.ts
 * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
 */

struct FCommandResult {
  ETranscriptStatus Status;
  FString Output;
};

struct FCommandExecutionContext {
  rtk::EnhancedStore<FStoreState> Store;
  TMap<FString, FString> NpcAliases;
  TMap<FString, FString> GhostAliases;
  FString LastGhostSessionId;

  FCommandExecutionContext() : Store(createStore()) {}
};

using FCommandExecutor =
    TFunction<FCommandResult(FCommandExecutionContext &, const FCommandSpec &)>;

namespace detail {
inline FCommandResult SuccessResult(const FString &Output) {
  return FCommandResult{ETranscriptStatus::Ok,
                        Output.IsEmpty() ? FString(TEXT("Command completed."))
                                         : Output};
}

inline FCommandResult ErrorResult(const FString &Output) {
  return FCommandResult{ETranscriptStatus::Error,
                        Output.IsEmpty() ? FString(TEXT("Command failed."))
                                         : Output};
}

inline FString JoinTokens(const TArray<FString> &Tokens, int32 StartIndex) {
  FString Joined;
  for (int32 Index = StartIndex; Index < Tokens.Num(); ++Index) {
    Joined += Index > StartIndex ? FString(TEXT(" ")) : FString();
    Joined += Tokens[Index];
  }
  return Joined;
}

inline TArray<FString> TokenizeCommand(const FString &Command) {
  TArray<FString> Tokens;
  FString Current;
  bool bInQuotes = false;

  for (int32 Index = 0; Index < Command.Len(); ++Index) {
    const TCHAR Char = Command[Index];
    if (Char == TEXT('"')) {
      bInQuotes = !bInQuotes;
      continue;
    }

    if (!bInQuotes && FChar::IsWhitespace(Char)) {
      if (!Current.IsEmpty()) {
        Tokens.Add(Current);
        Current.Reset();
      }
      continue;
    }

    Current.AppendChar(Char);
  }

  if (!Current.IsEmpty()) {
    Tokens.Add(Current);
  }

  return Tokens;
}

inline FString EscapeCommandJsonString(const FString &Value) {
  FString Escaped;
  for (int32 Index = 0; Index < Value.Len(); ++Index) {
    const TCHAR Char = Value[Index];
    switch (Char) {
    case TEXT('\\'):
      Escaped += TEXT("\\\\");
      break;
    case TEXT('"'):
      Escaped += TEXT("\\\"");
      break;
    case TEXT('\n'):
      Escaped += TEXT("\\n");
      break;
    case TEXT('\r'):
      Escaped += TEXT("\\r");
      break;
    case TEXT('\t'):
      Escaped += TEXT("\\t");
      break;
    default:
      Escaped.AppendChar(Char);
      break;
    }
  }
  return Escaped;
}

inline FString BuildProcessOutput(const FAgentResponse &Response) {
  const FString ActionType = Response.Action.Type.IsEmpty()
                                 ? FString(TEXT("NONE"))
                                 : Response.Action.Type;
  return FString::Printf(
      TEXT("{\"dialogue\":\"%s\",\"action\":{\"type\":\"%s\",\"target\":\"%s\",")
      TEXT("\"reason\":\"%s\"}}"),
      *EscapeCommandJsonString(Response.Dialogue),
      *EscapeCommandJsonString(ActionType),
      *EscapeCommandJsonString(Response.Action.Target),
      *EscapeCommandJsonString(Response.Action.Reason));
}

inline FString BuildBridgeValidationOutput(const FValidationResult &Validation) {
  const FString ActionType =
      Validation.bValid ? FString(TEXT("ALLOW")) : FString(TEXT("BLOCKED"));
  return FString::Printf(TEXT("{\"valid\":%s,\"reason\":\"%s\",")
                         TEXT("\"action\":{\"type\":\"%s\"}}"),
                         Validation.bValid ? TEXT("true") : TEXT("false"),
                         *EscapeCommandJsonString(Validation.Reason),
                         *EscapeCommandJsonString(ActionType));
}

inline FString ResolveNpcId(const FCommandExecutionContext &Context,
                            const FString &Candidate) {
  const FString *Alias = Context.NpcAliases.Find(Candidate);
  return Alias != nullptr ? *Alias : Candidate;
}

inline void RememberNpcAlias(FCommandExecutionContext &Context,
                             const FString &Alias, const FString &ResolvedId) {
  !Alias.IsEmpty() ? (void)Context.NpcAliases.Add(Alias, ResolvedId) : (void)0;
}

inline FString ResolveGhostSessionId(const FCommandExecutionContext &Context,
                                     const FString &Candidate) {
  const FString *Alias = Context.GhostAliases.Find(Candidate);
  return Alias != nullptr ? *Alias
         : (!Context.LastGhostSessionId.IsEmpty() &&
            Candidate.StartsWith(TEXT("ghost-")))
             ? Context.LastGhostSessionId
             : Candidate;
}

inline void RememberGhostSession(FCommandExecutionContext &Context,
                                 const FString &Alias,
                                 const FString &ResolvedId) {
  Context.LastGhostSessionId = ResolvedId;
  !Alias.IsEmpty() ? (void)Context.GhostAliases.Add(Alias, ResolvedId)
                   : (void)0;
}

inline func::Maybe<FNPCInternalState>
FindNpc(FCommandExecutionContext &Context, const FString &NpcId) {
  return NPCSlice::SelectNPCById(Context.Store.getState().NPCs, NpcId);
}

inline FCommandResult RunExternalCommand(const FString &CommandLine) {
  FString StdOut;
  int32 ReturnCode = -1;

  FString Executable;
  FString Args;
  (!CommandLine.Split(TEXT(" "), &Executable, &Args))
      ? (void)(Executable = CommandLine)
      : (void)0;

  void *ReadPipe = nullptr;
  void *WritePipe = nullptr;
  FPlatformProcess::CreatePipe(ReadPipe, WritePipe);

  FProcHandle Proc = FPlatformProcess::CreateProc(
      *Executable, *Args, false, true, true, nullptr, 0, nullptr, WritePipe,
      ReadPipe);

  struct ProcPoller {
    static void Poll(FProcHandle &Proc, void *ReadPipe, FString &StdOut,
                     double StartTime) {
      return !FPlatformProcess::IsProcRunning(Proc)
                 ? (void)0
                 : (FPlatformTime::Seconds() - StartTime > 10.0)
                       ? (FPlatformProcess::TerminateProc(Proc, true), void())
                       : (StdOut += FPlatformProcess::ReadPipe(ReadPipe),
                          FPlatformProcess::Sleep(0.01f),
                          Poll(Proc, ReadPipe, StdOut, StartTime));
    }
  };

  Proc.IsValid()
      ? [&]() {
          const double StartTime = FPlatformTime::Seconds();
          ProcPoller::Poll(Proc, ReadPipe, StdOut, StartTime);
          StdOut += FPlatformProcess::ReadPipe(ReadPipe);
          FPlatformProcess::GetProcReturnCode(Proc, &ReturnCode);
          FPlatformProcess::CloseProc(Proc);
        }()
      : (void)0;

  FPlatformProcess::ClosePipe(ReadPipe, WritePipe);

  return ReturnCode == 0 ? SuccessResult(StdOut.TrimEnd())
                         : ErrorResult(StdOut.TrimEnd());
}

inline FCommandResult ExecuteForbocAICommand(FCommandExecutionContext &Context,
                                             const TArray<FString> &Tokens) {
  try {
    if (Tokens.Num() < 2 || Tokens[0] != TEXT("forbocai")) {
      return ErrorResult(TEXT("Unsupported test-game command."));
    }

    const FString Domain = Tokens[1];

    if (Domain == TEXT("status")) {
      const FApiStatusResponse Status = Ops::CheckApiStatus(Context.Store);
      return SuccessResult(
          FString::Printf(TEXT("API: %s (v%s)"), *Status.Status, *Status.Version));
    }

    if (Domain == TEXT("npc")) {
      if (Tokens.Num() < 3) {
        return ErrorResult(TEXT("Usage: forbocai npc <subcommand>"));
      }

      const FString Subcommand = Tokens[2];
      if (Subcommand == TEXT("create")) {
        if (Tokens.Num() < 4) {
          return ErrorResult(TEXT("Usage: forbocai npc create <persona>"));
        }

        const FString Alias = Tokens[3];
        const FNPCInternalState Npc = Ops::CreateNpc(Context.Store, Alias);
        RememberNpcAlias(Context, Alias, Npc.Id);
        return SuccessResult(
            FString::Printf(TEXT("Created NPC: %s (%s)"), *Npc.Id, *Alias));
      }

      if (Subcommand == TEXT("state")) {
        if (Tokens.Num() < 4) {
          return ErrorResult(TEXT("Usage: forbocai npc state <npcId>"));
        }

        const FString NpcId = ResolveNpcId(Context, Tokens[3]);
        const auto MaybeNpc = FindNpc(Context, NpcId);
        return !MaybeNpc.hasValue
                   ? ErrorResult(
                         FString::Printf(TEXT("NPC not found: %s"), *NpcId))
                   : SuccessResult(FString::Printf(
                         TEXT("NPC ID: %s\nPersona: %s\nState: %s"),
                         *MaybeNpc.value.Id, *MaybeNpc.value.Persona,
                         *MaybeNpc.value.State.JsonData));
      }

      if (Subcommand == TEXT("update")) {
        if (Tokens.Num() < 6) {
          return ErrorResult(
              TEXT("Usage: forbocai npc update <npcId> <field> <value>"));
        }

        const FString NpcId = ResolveNpcId(Context, Tokens[3]);
        const FString Field = Tokens[4];
        const FString Value = JoinTokens(Tokens, 5);
        FAgentState Delta;
        Delta.JsonData =
            FString::Printf(TEXT("{\"%s\":\"%s\"}"),
                            *EscapeCommandJsonString(Field),
                            *EscapeCommandJsonString(Value));

        const auto Updated = Ops::UpdateNpc(Context.Store, NpcId, Delta);
        return !Updated.hasValue
                   ? ErrorResult(
                         FString::Printf(TEXT("NPC not found: %s"), *NpcId))
                   : SuccessResult(FString::Printf(TEXT("NPC %s updated: %s"),
                                                   *Updated.value.Id,
                                                   *Updated.value.State.JsonData));
      }

      if (Subcommand == TEXT("process") || Subcommand == TEXT("chat")) {
        const bool bHasTextFlag =
            Tokens.Num() > 4 && Tokens[4] == TEXT("--text");
        const int32 TextIndex = Subcommand == TEXT("chat")
                                    ? (bHasTextFlag ? 5 : 4)
                                    : 4;
        if (Tokens.Num() <= TextIndex) {
          return ErrorResult(Subcommand == TEXT("chat")
                                 ? TEXT("Usage: forbocai npc chat <npcId> --text <message>")
                                 : TEXT("Usage: forbocai npc process <npcId> <message>"));
        }

        const FString NpcId = ResolveNpcId(Context, Tokens[3]);
        const FString Text = JoinTokens(Tokens, TextIndex);
        const FAgentResponse Response = Ops::ProcessNpc(Context.Store, NpcId, Text);
        return SuccessResult(BuildProcessOutput(Response));
      }

      if (Subcommand == TEXT("active")) {
        const auto Active = Ops::GetActiveNpc(Context.Store);
        return !Active.hasValue
                   ? ErrorResult(TEXT("No active NPC."))
                   : SuccessResult(FString::Printf(TEXT("Active NPC: %s (%s)"),
                                                   *Active.value.Id,
                                                   *Active.value.Persona));
      }
    }

    if (Domain == TEXT("memory")) {
      if (Tokens.Num() < 3) {
        return ErrorResult(TEXT("Usage: forbocai memory <subcommand>"));
      }

      const FString Subcommand = Tokens[2];
      if (Tokens.Num() < 4) {
        return ErrorResult(TEXT("Usage: forbocai memory <subcommand> <npcId>"));
      }

      const FString NpcId = ResolveNpcId(Context, Tokens[3]);
      if (Subcommand == TEXT("list")) {
        const TArray<FMemoryItem> Items = Ops::MemoryList(Context.Store, NpcId);
        return SuccessResult(
            FString::Printf(TEXT("Found %d memories"), Items.Num()));
      }

      if (Subcommand == TEXT("recall")) {
        if (Tokens.Num() < 5) {
          return ErrorResult(
              TEXT("Usage: forbocai memory recall <npcId> <query>"));
        }

        const TArray<FMemoryItem> Items =
            Ops::MemoryRecall(Context.Store, NpcId, JoinTokens(Tokens, 4));
        return SuccessResult(
            FString::Printf(TEXT("Recalled %d memories"), Items.Num()));
      }

      if (Subcommand == TEXT("store")) {
        if (Tokens.Num() < 5) {
          return ErrorResult(
              TEXT("Usage: forbocai memory store <npcId> <text>"));
        }

        Ops::MemoryStore(Context.Store, NpcId, JoinTokens(Tokens, 4));
        return SuccessResult(TEXT("Memory stored."));
      }

      if (Subcommand == TEXT("clear")) {
        Ops::MemoryClear(Context.Store, NpcId);
        return SuccessResult(TEXT("Memory cleared."));
      }

      if (Subcommand == TEXT("export")) {
        const TArray<FMemoryItem> Items = Ops::MemoryList(Context.Store, NpcId);
        return SuccessResult(
            FString::Printf(TEXT("Exported %d memories"), Items.Num()));
      }
    }

    if (Domain == TEXT("bridge")) {
      if (Tokens.Num() < 3) {
        return ErrorResult(TEXT("Usage: forbocai bridge <subcommand>"));
      }

      const FString Subcommand = Tokens[2];
      if (Subcommand == TEXT("rules")) {
        const TArray<FBridgeRule> Rules = Ops::BridgeRules(Context.Store);
        return SuccessResult(
            FString::Printf(TEXT("Found %d bridge rules"), Rules.Num()));
      }

      if (Subcommand == TEXT("preset")) {
        if (Tokens.Num() < 4) {
          return ErrorResult(TEXT("Usage: forbocai bridge preset <name>"));
        }

        const FDirectiveRuleSet Preset =
            Ops::BridgePreset(Context.Store, Tokens[3]);
        return SuccessResult(
            FString::Printf(TEXT("Loaded preset: %s"), *Preset.Id));
      }

      if (Subcommand == TEXT("validate")) {
        if (Tokens.Num() < 4) {
          return ErrorResult(TEXT("Usage: forbocai bridge validate <action>"));
        }

        FAgentAction Action;
        Action.Type = Tokens[3];
        Action.PayloadJson =
            FString::Printf(TEXT("{\"type\":\"%s\"}"),
                            *EscapeCommandJsonString(Tokens[3]));
        const FBridgeValidationContext BridgeContext;
        const FValidationResult Validation =
            Ops::ValidateBridge(Context.Store, Action, BridgeContext);
        return SuccessResult(BuildBridgeValidationOutput(Validation));
      }
    }

    if (Domain == TEXT("soul")) {
      if (Tokens.Num() < 3) {
        return ErrorResult(TEXT("Usage: forbocai soul <subcommand>"));
      }

      const FString Subcommand = Tokens[2];
      if (Subcommand == TEXT("export")) {
        if (Tokens.Num() < 4) {
          return ErrorResult(TEXT("Usage: forbocai soul export <npcId>"));
        }

        const FString NpcId = ResolveNpcId(Context, Tokens[3]);
        const FSoulExportResult Exported = Ops::ExportSoul(Context.Store, NpcId);
        return SuccessResult(
            FString::Printf(TEXT("Soul exported: %s"), *Exported.TxId));
      }

      if (Subcommand == TEXT("import")) {
        if (Tokens.Num() < 4) {
          return ErrorResult(TEXT("Usage: forbocai soul import <txId>"));
        }

        const FSoul Soul = Ops::ImportSoul(Context.Store, Tokens[3]);
        return SuccessResult(
            FString::Printf(TEXT("Soul imported: %s"), *Soul.Id));
      }

      if (Subcommand == TEXT("list")) {
        const int32 Limit = Tokens.Num() > 3 ? FCString::Atoi(*Tokens[3]) : 50;
        const TArray<FSoulListItem> Souls = Ops::ListSouls(Context.Store, Limit);
        return SuccessResult(FString::Printf(TEXT("Found %d souls"), Souls.Num()));
      }

      if (Subcommand == TEXT("chat")) {
        const bool bHasTextFlag =
            Tokens.Num() > 4 && Tokens[4] == TEXT("--text");
        const int32 TextIndex = bHasTextFlag ? 5 : 4;
        if (Tokens.Num() <= TextIndex) {
          return ErrorResult(
              TEXT("Usage: forbocai soul chat <npcId> --text <message>"));
        }

        const FString NpcId = ResolveNpcId(Context, Tokens[3]);
        const FString Text = JoinTokens(Tokens, TextIndex);
        const FAgentResponse Response = Ops::ProcessNpc(Context.Store, NpcId, Text);
        return SuccessResult(BuildProcessOutput(Response));
      }
    }

    if (Domain == TEXT("ghost")) {
      if (Tokens.Num() < 3) {
        return ErrorResult(TEXT("Usage: forbocai ghost <subcommand>"));
      }

      const FString Subcommand = Tokens[2];
      if (Subcommand == TEXT("run")) {
        const FString Suite = Tokens.Num() > 3 ? Tokens[3] : TEXT("default");
        const int32 Duration = Tokens.Num() > 4 ? FCString::Atoi(*Tokens[4]) : 300;
        const FGhostRunResponse Response = Ops::GhostRun(Context.Store, Suite, Duration);
        RememberGhostSession(Context, TEXT("ghost-001"), Response.SessionId);
        return SuccessResult(FString::Printf(TEXT("Ghost session started: %s"),
                                             *Response.SessionId));
      }

      if (Subcommand == TEXT("history")) {
        const int32 Limit = Tokens.Num() > 3 ? FCString::Atoi(*Tokens[3]) : 10;
        const TArray<FGhostHistoryEntry> History =
            Ops::GhostHistory(Context.Store, Limit);
        return SuccessResult(
            FString::Printf(TEXT("Ghost history: %d entries"), History.Num()));
      }

      if (Tokens.Num() < 4) {
        return ErrorResult(TEXT("Usage: forbocai ghost <subcommand> <sessionId>"));
      }

      const FString SessionId = ResolveGhostSessionId(Context, Tokens[3]);
      if (Subcommand == TEXT("status")) {
        const FGhostStatusResponse Response =
            Ops::GhostStatus(Context.Store, SessionId);
        return SuccessResult(
            FString::Printf(TEXT("Ghost status: %s"), *Response.GhostStatus));
      }

      if (Subcommand == TEXT("results")) {
        const FGhostResultsResponse Response =
            Ops::GhostResults(Context.Store, SessionId);
        return SuccessResult(FString::Printf(
            TEXT("Ghost results: %d/%d passed"), Response.ResultsPassed,
            Response.ResultsTotalTests));
      }

      if (Subcommand == TEXT("stop")) {
        const FGhostStopResponse Response = Ops::GhostStop(Context.Store, SessionId);
        return SuccessResult(
            FString::Printf(TEXT("Ghost stopped: %s"), *Response.StopStatus));
      }
    }

    if (Domain == TEXT("cortex")) {
      if (Tokens.Num() < 3 || Tokens[2] != TEXT("init")) {
        return ErrorResult(TEXT("Usage: forbocai cortex init [--remote] [model]"));
      }

      const bool bRemote = Tokens.Num() > 3 && Tokens[3] == TEXT("--remote");
      if (bRemote) {
        const FString Model = Tokens.Num() > 4 ? Tokens[4] : TEXT("api-integrated");
        const FCortexStatus Status = Ops::InitRemoteCortex(Context.Store, Model);
        return SuccessResult(
            FString::Printf(TEXT("Remote cortex initialized: %s"),
                            *Status.Id));
      }

      const FString Model = Tokens.Num() > 3 ? Tokens[3] : TEXT("smollm2-135m");
      const FCortexStatus Status = Ops::InitCortex(Context.Store, Model);
      return SuccessResult(
          FString::Printf(TEXT("Cortex initialized: %s"), *Status.Id));
    }

    return ErrorResult(
        FString::Printf(TEXT("Unsupported test-game command: %s"),
                        *JoinTokens(Tokens, 0)));
  } catch (const std::exception &Error) {
    return ErrorResult(UTF8_TO_TCHAR(Error.what()));
  }
}
} // namespace detail

/**
 * Executes a scenario command through the in-process UE runtime when possible
 * so autoplay preserves command state across steps. Unknown commands fall back
 * to process execution.
 */
inline FCommandResult RunCommand(FCommandExecutionContext &Context,
                                 const FCommandSpec &Command) {
  const TArray<FString> Tokens = detail::TokenizeCommand(Command.Command);
  return Tokens.Num() > 0 && Tokens[0] == TEXT("forbocai")
             ? detail::ExecuteForbocAICommand(Context, Tokens)
             : detail::RunExternalCommand(Command.Command);
}

/**
 * Resolves a single grid cell character for the current game state.
 * User Story: As ASCII rendering, I need one cell resolver so the grid view
 * can show blocked tiles, the player, and NPCs consistently.
 */

/**
 * Recursive helper — scans blocked positions for a match.
 * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
 */
namespace detail {
inline bool IsBlocked(const TArray<FPosition> &Blocked, const FPosition &Pos,
                      int32 Index) {
  return Index >= Blocked.Num()
             ? false
             : (Blocked[Index] == Pos ? true
                                      : IsBlocked(Blocked, Pos, Index + 1));
}

/**
 * Recursive helper — scans NPCs for a position match and returns the cell char.
 * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
 */
inline TCHAR NpcCellAt(const TArray<FGameNPC> &Npcs, const FPosition &Pos,
                       int32 Index) {
  return Index >= Npcs.Num()
             ? TEXT('.')
             : (Npcs[Index].Position == Pos
                    ? (Npcs[Index].Id == TEXT("miller")
                           ? TEXT('M')
                           : (Npcs[Index].Id == TEXT("doomguard") ? TEXT('D')
                                                                  : TEXT('N')))
                    : NpcCellAt(Npcs, Pos, Index + 1));
}
} // namespace detail

inline TCHAR CellAt(const FPosition &Pos, const FTestGameState &State) {
  /**
   * Check blocked
   * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
   */
  return detail::IsBlocked(State.Grid.Blocked, Pos, 0)
             ? TEXT('#')
             /**
              * Check player
              * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
              */
             : (State.Player.Position == Pos
                    ? TEXT('P')
                    /**
                     * Check NPCs
                     * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
                     */
                    : detail::NpcCellAt(
                          GetNPCAdapter().getSelectors().selectAll(
                              State.NPCs.Entities),
                          Pos, 0));
}

/**
 * Renders the whole tile grid as ASCII rows.
 * User Story: As terminal rendering, I need the full grid rendered so scenario
 * output can show the current world state in text form.
 */
namespace detail {
inline FString RenderRow(const FTestGameState &State, int32 X, int32 Width,
                         const FString &Acc) {
  return X >= Width
             ? Acc
             : RenderRow(State, X + 1, Width,
                         Acc + (X > 0 ? FString(TEXT(" ")) : FString()) +
                             CellAt(FPosition(X, (int32)0), State));
}

inline FString RenderRowAt(const FTestGameState &State, int32 Y, int32 X,
                            int32 Width, const FString &Acc) {
  return X >= Width
             ? Acc
             : RenderRowAt(State, Y, X + 1, Width,
                            Acc + (X > 0 ? FString(TEXT(" ")) : FString()) +
                                CellAt(FPosition(X, Y), State));
}

inline FString RenderRows(const FTestGameState &State, int32 Y, int32 Height,
                           const FString &Acc) {
  return Y >= Height
             ? Acc
             : RenderRows(State, Y + 1, Height,
                           Acc + (Y > 0 ? FString(TEXT("\n")) : FString()) +
                               RenderRowAt(State, Y, 0, State.Grid.Width,
                                           FString()));
}
} // namespace detail

inline FString RenderGrid(const FTestGameState &State) {
  return detail::RenderRows(State, 0, State.Grid.Height, FString());
}

/**
 * Renders a compact legend string.
 * User Story: As terminal rendering, I need a legend string so players can
 * interpret the ASCII symbols shown in the grid output.
 */
inline FString RenderLegend() {
  return TEXT("Legend :: P=Scout  D=Doomguard  M=Miller  #=Blocked  .=Open");
}

using FRuntimeConnectivityProbe = TFunction<bool(const FString &)>;

/**
 * Checks runtime connectivity to a given status URL.
 * Returns true if the endpoint responds with JSON containing a status field
 * within 1.5s.
 * User Story: As runtime fallback selection, I need a connectivity probe so
 * the test game can decide which runtime URL is available.
 */
namespace detail {
inline bool HasStatusJsonField(const FString &Body) {
  TSharedPtr<FJsonObject> JsonObject;
  TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Body);
  return FJsonSerializer::Deserialize(Reader, JsonObject) &&
         JsonObject.IsValid() && JsonObject->HasTypedField<EJson::String>(
                                     TEXT("status"));
}
} // namespace detail

inline bool CheckRuntimeConnectivity(
    const FString &Url = TEXT("http://localhost:8080/status")) {
  try {
    const func::HttpResult<FString> Result = Ops::WaitForResult(
        func::AsyncHttp::Get<FString>(Url), 1.5);
    return Result.bSuccess && Result.ResponseCode == 200 &&
           detail::HasStatusJsonField(Result.data);
  } catch (const std::exception &) {
    return false;
  }
}

/**
 * Reads the explicit runtime URL from FORBOC_RUNTIME_URL. Returns an empty
 * string if the env var is unset.
 * User Story: As the no-fallback runtime policy, I need consumers to set the
 * runtime URL explicitly so production hosts never silently downgrade to
 * localhost or an unintended remote.
 */
inline FString ResolveRuntimeUrlFromEnv() {
  return FPlatformMisc::GetEnvironmentVariable(TEXT("FORBOC_RUNTIME_URL"));
}

/**
 * Test-harness probe-based resolver. Verification-only.
 * User Story: As the test-game harness, I need a resolver that probes
 * localhost first and a published API second so verification runs locally
 * during development and remotely in CI without per-developer config.
 *
 * Production code paths must NOT call this. Use ResolveRuntimeUrlFromEnv
 * (or set FORBOC_RUNTIME_URL explicitly) so the no-fallback policy holds.
 */
inline FString ResolveRuntimeUrl(const FRuntimeConnectivityProbe &Probe) {
  return Probe(TEXT("http://localhost:8080/status"))
             ? FString(TEXT("http://localhost:8080"))
             : (Probe(TEXT("https://api.forboc.ai/status"))
                    ? FString(TEXT("https://api.forboc.ai"))
                    : FString());
}

/**
 * Default test-harness resolver. Prefers FORBOC_RUNTIME_URL when set; falls
 * back to the probe-based path only if no env var is configured.
 * User Story: As a test runner, I need explicit env-var configuration to take
 * precedence so CI can pin the runtime without relying on probe order.
 */
inline FString ResolveRuntimeUrl() {
  const FString FromEnv = ResolveRuntimeUrlFromEnv();
  return !FromEnv.IsEmpty()
             ? FromEnv
             : ResolveRuntimeUrl(
                   [](const FString &Url) {
                     return CheckRuntimeConnectivity(Url);
                   });
}

} // namespace TestGame
