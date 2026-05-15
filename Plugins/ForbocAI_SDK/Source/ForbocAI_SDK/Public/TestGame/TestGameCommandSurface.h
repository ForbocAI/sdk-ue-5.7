#pragma once
/**
 * TestGameCommandSurface — First-class UE test-game command boundary.
 *
 * This module provides the canonical command entrypoint for the UE test game,
 * intentionally parallel to the Node and browser harness entrypoints.
 *
 * Architecture:
 *   test game -> CommandSurface -> CLIOps::DispatchCommand -> Ops::* -> API
 *
 * This replaces the previous pattern where TestGameLib.h::ExecuteForbocAICommand
 * was a shadow CLI that re-routed commands directly to Ops::* functions,
 * bypassing the canonical CLIOps command surface.
 *
 * The important point is NOT "UE must look like a Unix shell."
 * The important point IS "the UE harness validates a real host command boundary
 * instead of bypassing it with private helper calls."
 *
 * User Story: As the UE test-game harness, I need a first-class command
 * boundary so UE proves real parity at the execution-boundary level,
 * not just at the scenario-data level.
 */

#include "CLI/CLIModule.h"
#include "CLI/CliHandlers.h"
#include "CLI/CliOperations.h"
#include "Core/functional_core.hpp"
#include "CoreMinimal.h"
#include "RuntimeStore.h"
#include "TestGame/TestGameContract.h"
#include "TestGame/TestGameTypes.h"

namespace TestGame {
namespace CommandSurface {

// ── Command result (mirrors TestGameLib FCommandResult for compat) ──

struct FCommandOutput {
  ETranscriptStatus Status;
  FString Output;
  FString RoutedThrough; // The CLIOps command key used
};

// ── Alias state (NPC and Ghost id resolution) ──

struct FAliasState {
  TMap<FString, FString> NpcAliases;
  TMap<FString, FString> GhostAliases;
  FString LastGhostSessionId;

  // API-contract alias rules
  FString NpcCreateAliasRule;       // "substitute_generated_npc_id"
  FString BridgeValidateCommandRule; // "expand_preset_macro"
};

// ── Command key mapping (forbocai text -> CLIOps key) ──

namespace detail {

/**
 * Maps a tokenized command to its CLIOps command key.
 *
 * e.g. ["forbocai", "npc", "create", "doomguard"] -> "npc_create"
 *      ["forbocai", "bridge", "validate", "doomguard-jump"] -> "bridge_validate"
 *
 * This is the ONLY place where test-game command text is translated
 * to CLIOps keys. No other module should perform this mapping.
 */
inline FString MapToCommandKey(const TArray<FString> &Tokens) {
  return Tokens.Num() < 2
             ? FString(TEXT("unknown"))
             : Tokens.Num() < 3
                   ? Tokens[1] // e.g. "forbocai status" -> "status"
                   : FString::Printf(TEXT("%s_%s"),
                                     *Tokens[1], *Tokens[2]);
}

/**
 * Extract positional arguments from the tokenized command.
 * Skips the first 3 tokens (forbocai <domain> <subcommand>).
 * Handles --text flag for chat/soul commands.
 *
 * Recursive — FP-compliant.
 */
inline TArray<FString> ExtractArgsRecursive(const TArray<FString> &Tokens,
                                             int32 Idx,
                                             TArray<FString> Acc) {
  return Idx >= Tokens.Num()
             ? Acc
             : (Tokens[Idx] == TEXT("--text")
                    ? ExtractArgsRecursive(Tokens, Idx + 1, Acc) // skip flag
                    : (Acc.Add(Tokens[Idx]),
                       ExtractArgsRecursive(Tokens, Idx + 1, Acc)));
}

inline TArray<FString> ExtractArgs(const TArray<FString> &Tokens) {
  // For 2-token commands (e.g. "forbocai status"), no args
  return Tokens.Num() <= 3
             ? TArray<FString>()
             : ExtractArgsRecursive(Tokens, 3, {});
}

/**
 * Resolve NPC alias based on contract alias rules.
 *
 * When NpcCreateAliasRule == "substitute_generated_npc_id":
 *   After `npc create doomguard`, the alias "doomguard" maps to the
 *   API-generated NPC ID for all subsequent commands.
 */
inline FString ResolveNpcAlias(const FAliasState &Aliases,
                                const FString &Candidate) {
  const FString *Found = Aliases.NpcAliases.Find(Candidate);
  return Found ? *Found : Candidate;
}

/**
 * Tokenize a command string, respecting quoted strings.
 * Reuses the same tokenizer from TestGameLib.
 */
inline TArray<FString> Tokenize(const FString &Command) {
  TArray<FString> Tokens;
  FString Current;
  bool bInQuotes = false;

  // Recursive character-by-character tokenizer
  struct Tokenizer {
    static void Process(const FString &Cmd, int32 Idx, bool bQuoted,
                        FString &Cur, TArray<FString> &Out) {
      return Idx >= Cmd.Len()
                 ? (!Cur.IsEmpty() ? (Out.Add(Cur), void()) : void())
                 : Cmd[Idx] == TEXT('"')
                       ? (Process(Cmd, Idx + 1, !bQuoted, Cur, Out))
                       : (!bQuoted && FChar::IsWhitespace(Cmd[Idx]))
                             ? (!Cur.IsEmpty()
                                    ? (Out.Add(Cur), Cur.Reset(),
                                       Process(Cmd, Idx + 1, bQuoted, Cur, Out))
                                    : Process(Cmd, Idx + 1, bQuoted, Cur, Out))
                             : (Cur.AppendChar(Cmd[Idx]),
                                Process(Cmd, Idx + 1, bQuoted, Cur, Out));
    }
  };

  Tokenizer::Process(Command, 0, false, Current, Tokens);
  return Tokens;
}

} // namespace detail

// ── Public Command Surface API ──

/**
 * Execute a test-game command through the canonical CLIOps command surface.
 *
 * This is the ONLY entrypoint for test-game command execution.
 * It tokenizes the command, maps to a CLIOps key, resolves aliases,
 * and delegates to CLIOps::DispatchCommand.
 *
 * User Story: As the first-class UE command boundary, I need one
 * execution function so all test-game commands flow through the
 * same canonical surface that Node and browser use.
 */
inline FCommandOutput Execute(const FString &CommandText,
                              FAliasState &Aliases) {
  const TArray<FString> Tokens = detail::Tokenize(CommandText);

  if (Tokens.Num() < 1 || Tokens[0] != TEXT("forbocai")) {
    return FCommandOutput{ETranscriptStatus::Error,
                          TEXT("Not a forbocai command"), TEXT("")};
  }

  const FString CommandKey = detail::MapToCommandKey(Tokens);
  TArray<FString> Args = detail::ExtractArgs(Tokens);

  if (Args.Num() > 0 &&
      (CommandKey.Contains(TEXT("npc_")) ||
       CommandKey.Contains(TEXT("memory_")) ||
       CommandKey.Contains(TEXT("soul_")))) {
    Args[0] = detail::ResolveNpcAlias(Aliases, Args[0]);
  }

  const func::TestResult<void> Result =
      CLIOps::DispatchCommand(CommandKey, Args);
  const FString ResultMessage = Result.message.empty()
                                    ? FString()
                                    : FString(UTF8_TO_TCHAR(Result.message.c_str()));

  if (CommandKey == TEXT("npc_create") && Result.bSuccess && Args.Num() > 0 &&
      Aliases.NpcCreateAliasRule == TEXT("substitute_generated_npc_id")) {
    Aliases.NpcAliases.Add(Args[0], ResultMessage);
  }

  return FCommandOutput{
      Result.bSuccess ? ETranscriptStatus::Ok : ETranscriptStatus::Error,
      ResultMessage,
      CommandKey};
}

/**
 * Execute a FCommandSpec (from scenario steps) through the command surface.
 * Records a transcript entry for coverage tracking.
 *
 * User Story: As the autoplay harness, I need scenario-spec execution
 * so each command step produces a tracked transcript entry.
 */
inline FTranscriptEntry ExecuteSpec(const FCommandSpec &Spec,
                                    const FString &ScenarioId,
                                    FAliasState &Aliases) {
  const FCommandOutput Output = Execute(Spec.Command, Aliases);

  FTranscriptEntry Entry;
  Entry.Id = FGuid::NewGuid().ToString();
  Entry.ScenarioId = ScenarioId;
  Entry.CommandGroup = Spec.Group;
  Entry.Command = Spec.Command;
  Entry.ExpectedRoutes = Spec.ExpectedRoutes;
  Entry.Status = Output.Status;
  Entry.Output = Output.Output;
  Entry.Timestamp = FDateTime::UtcNow().ToString();

  return Entry;
}

/**
 * Initialize alias state from the API contract's alias rules.
 *
 * User Story: As cross-host parity, I need alias initialization from
 * the contract so UE uses the same NPC alias behavior as Node/browser.
 */
inline FAliasState CreateAliasState(
    const Contract::FContractResponse &ContractResponse) {
  FAliasState State;
  ContractResponse.bValid
      ? (State.NpcCreateAliasRule =
             ContractResponse.AliasRules.NpcCreateAlias,
         State.BridgeValidateCommandRule =
             ContractResponse.AliasRules.BridgeValidateCommand,
         void())
      : (State.NpcCreateAliasRule = TEXT("substitute_generated_npc_id"),
         State.BridgeValidateCommandRule = TEXT("expand_preset_macro"),
         void());
  return State;
}

/**
 * Run a full scenario through the command surface.
 * Returns all transcript entries for coverage analysis.
 *
 * Recursive — FP-compliant.
 */
inline TArray<FTranscriptEntry> RunScenarioRecursive(
    const FScenarioStep &Scenario,
    FAliasState &Aliases,
    int32 CmdIdx,
    TArray<FTranscriptEntry> Acc) {
  return CmdIdx >= Scenario.Commands.Num()
             ? Acc
             : (Acc.Add(ExecuteSpec(Scenario.Commands[CmdIdx],
                                    Scenario.Id, Aliases)),
                RunScenarioRecursive(Scenario, Aliases, CmdIdx + 1, Acc));
}

/**
 * Run all scenarios through the command surface.
 *
 * User Story: As the UE autoplay harness, I need full scenario execution
 * so all 4 scenarios run through the canonical command boundary and
 * produce a complete transcript for coverage verification.
 */
inline TArray<FTranscriptEntry> RunAllScenarios(
    const TArray<FScenarioStep> &Steps,
    FAliasState &Aliases,
    int32 StepIdx = 0,
    TArray<FTranscriptEntry> Acc = {}) {
  return StepIdx >= Steps.Num()
             ? Acc
             : [&]() {
    UE_LOG(LogTemp, Display,
           TEXT("CommandSurface: Running scenario [%d/%d] '%s'"),
           StepIdx + 1, Steps.Num(), *Steps[StepIdx].Title);

    TArray<FTranscriptEntry> StepEntries =
        RunScenarioRecursive(Steps[StepIdx], Aliases, 0, {});

    // Append step entries to accumulated transcript
    const auto AppendRecursive =
        [&Acc, &StepEntries](int32 Idx, const auto &Self) -> void {
      return Idx >= StepEntries.Num()
                 ? void()
                 : (Acc.Add(StepEntries[Idx]), Self(Idx + 1, Self));
    };
    AppendRecursive(0, AppendRecursive);

    return RunAllScenarios(Steps, Aliases, StepIdx + 1, Acc);
  }();
}

} // namespace CommandSurface
} // namespace TestGame
