#pragma once

// Handler classification — pinned by the canonical instruction sequence in
// classified/docs/design/api/architecture.md § Canonical Instruction Sequence.
// Drift between this table and the dispatcher is treated as a contract bug.
//
//   Local        — handler executes a host-side capability (recall, lookup, etc.)
//   Pass-through — handler only acknowledges API-supplied tape state and recurses
//
// | Instruction        | Classification | Reason                                                          |
// | ------------------ | -------------- | --------------------------------------------------------------- |
// | IdentifyActor      | Local          | UE resolves actor info from the runtime registry                |
// | QueryVector        | Local          | UE runs the configured memory engine recall                     |
// | Decision           | Local          | UE applies the local decision policy (2026-04-28)               |
// | Reasoning          | Pass-through   | API hosts the SLM; UE only acks and continues (2026-04-27)      |
// | Finalize           | Local          | UE persists memory, applies state delta, dispatches verdict     |

#include "Core/ThunkDetail.h"
#include "DirectiveSlice.h"
#include "Memory/MemoryThunks.h"
#include "Protocol/ProtocolRequestTypes.h"

namespace rtk {

struct FProtocolRuntime {
  std::function<ThunkAction<FMemoryItem, FStoreState>(const FMemoryItem &)>
      StoreMemory;
  std::function<ThunkAction<TArray<FMemoryItem>, FStoreState>(
      const FMemoryRecallRequest &)>
      RecallMemory;
  std::function<ThunkAction<FCortexResponse, FStoreState>(
      const FString &, const FCortexConfig &)>
      CompleteInference;

  /**
   * Returns whether local memory store and recall handlers are configured.
   * User Story: As protocol orchestration, I need to know whether memory
   * support exists before executing instructions that depend on it.
   */
  bool HasMemory() const {
    return static_cast<bool>(StoreMemory) && static_cast<bool>(RecallMemory);
  }

  /**
   * Returns whether a local cortex completion handler is configured.
   * User Story: As protocol orchestration, I need to know whether local
   * inference is available before executing inference instructions.
   */
  bool HasCortex() const { return static_cast<bool>(CompleteInference); }
};

/**
 * Builds the default local protocol runtime backed by node memory and cortex.
 * User Story: As local protocol execution, I need a ready-made runtime so the
 * protocol loop can call local memory and inference services consistently.
 */
inline FProtocolRuntime LocalProtocolRuntime() {
  FProtocolRuntime Runtime;
  Runtime.StoreMemory = [](const FMemoryItem &Item) {
    return nodeMemoryStoreThunk(Item);
  };
  Runtime.RecallMemory = [](const FMemoryRecallRequest &Request) {
    return nodeMemoryRecallThunk(Request);
  };
  Runtime.CompleteInference = nullptr;
  return Runtime;
}

namespace detail {

/**
 * Converts a single memory item into its recalled counterpart.
 * User Story: As protocol memory flows, I need item conversion so recalled
 * memory arrays can be built without imperative loop code.
 */
inline FRecalledMemory MemoryItemToRecalled(const FMemoryItem &Item) {
  FRecalledMemory M;
  M.Text = Item.Text;
  M.Type = Item.Type;
  M.Importance = Item.Importance;
  M.Similarity = Item.Similarity;
  return M;
}

/**
 * Recursively populates a recalled-memory array from memory items.
 * User Story: As protocol memory flows, I need recursive array building so
 * memory conversion stays expression-style without imperative loops.
 */
inline TArray<FRecalledMemory>
PopulateRecalledMemoriesRecursive(const TArray<FMemoryItem> &Memories,
                                  int32 Index,
                                  TArray<FRecalledMemory> Result) {
  return Index >= Memories.Num()
             ? Result
             : (Result.Add(MemoryItemToRecalled(Memories[Index])),
                PopulateRecalledMemoriesRecursive(Memories, Index + 1,
                                                  MoveTemp(Result)));
}

inline func::AsyncResult<rtk::FEmptyPayload>
PersistMemoryInstructions(const TArray<FMemoryStoreInstruction> &Instructions,
                          int32 Index, const FProtocolRuntime &Runtime,
                          std::function<AnyAction(const AnyAction &)> Dispatch,
                          std::function<FStoreState()> GetState);

func::AsyncResult<FAgentResponse>
RunProtocolTurn(const FString &NpcId, const FString &Input,
                const FString &RunId, const FNPCProcessTape &Tape,
                const FString &LastResult, bool bHasLastResult,
                int32 Turn, const FProtocolRuntime &Runtime,
                std::function<AnyAction(const AnyAction &)> Dispatch,
                std::function<FStoreState()> GetState);

/**
 * Handles the IdentifyActor protocol instruction by serializing actor info
 * and recursing into the next protocol turn.
 * User Story: As protocol instruction dispatch, I need actor identification
 * handled as a pure expression so the instruction ternary stays flat.
 */
inline func::AsyncResult<FAgentResponse>
HandleIdentifyActor(const FNPCProcessResponse &Response,
                    const FString &NpcId, const FString &Input,
                    const FString &RunId, int32 Turn,
                    const FProtocolRuntime &Runtime,
                    std::function<AnyAction(const AnyAction &)> Dispatch,
                    std::function<FStoreState()> GetState) {
  FNPCActorInfo Actor;
  Actor.NpcId = NpcId;
  Actor.Persona = Response.Tape.Persona;
  Actor.Data = Response.Tape.NpcState;
  return RunProtocolTurn(NpcId, Input, RunId, Response.Tape,
                         SerializeIdentifyActorResult(Actor), true, Turn + 1,
                         Runtime, Dispatch, GetState);
}

/**
 * Handles the QueryVector protocol instruction by dispatching a memory recall
 * and building the recalled-memory tape for the next turn.
 * User Story: As protocol instruction dispatch, I need vector queries handled
 * as a pure expression so the instruction ternary stays flat.
 */
inline func::AsyncResult<FAgentResponse>
HandleQueryVector(const FNPCProcessResponse &Response,
                  const FNPCInstruction &Instruction,
                  const FString &NpcId, const FString &Input,
                  const FString &RunId, int32 Turn,
                  const FProtocolRuntime &Runtime,
                  std::function<AnyAction(const AnyAction &)> Dispatch,
                  std::function<FStoreState()> GetState) {
  return !Runtime.HasMemory()
             ? (Dispatch(DirectiveSlice::Actions::DirectiveRunFailed(
                    RunId,
                    TEXT("API requested memory recall, but no memory engine "
                         "is configured"))),
                RejectAsync<FAgentResponse>(
                    TEXT("API requested memory recall, but no memory engine "
                         "is configured")))
             : [&]() -> func::AsyncResult<FAgentResponse> {
    FDirectiveResponse Directive;
    Directive.MemoryRecall = TypeFactory::MemoryRecallInstruction(
        Instruction.Query, Instruction.Limit, Instruction.Threshold);
    Dispatch(
        DirectiveSlice::Actions::DirectiveReceived(RunId, Directive));

    FMemoryRecallRequest RecallRequest;
    RecallRequest.Query = Instruction.Query;
    RecallRequest.Limit = Instruction.Limit;
    RecallRequest.Threshold = Instruction.Threshold;

    return func::AsyncChain::then<TArray<FMemoryItem>, FAgentResponse>(
        Runtime.RecallMemory(RecallRequest)(Dispatch, GetState),
        [NpcId, Input, RunId, Response, Turn, Dispatch, GetState,
         Runtime](const TArray<FMemoryItem> &Memories) {
          FNPCProcessTape NextTape = Response.Tape;
          NextTape.Memories =
              PopulateRecalledMemoriesRecursive(Memories, 0, TArray<FRecalledMemory>());
          NextTape.bVectorQueried = true;
          return RunProtocolTurn(
              NpcId, Input, RunId, NextTape,
              SerializeQueryVectorResult(Memories), true, Turn + 1,
              Runtime, Dispatch, GetState);
        });
  }();
}

/**
 * Handles the Decision protocol instruction by returning a decision intent.
 * User Story: As protocol instruction dispatch, I need the Decision step
 * handled so the multi-round loop can advance past the decision phase
 * without stalling. The API issues DecisionInstruction after QueryVector;
 * the SDK must return a DecisionResult with goal and actionType so the
 * tape includes decisionIntent on subsequent /process calls.
 *
 * Implementation (2026-04-28): Derives goal and actionType from observation
 * and memories, mirroring the API Orchestrator.hs logic.
 */
inline func::AsyncResult<FAgentResponse>
HandleDecision(const FNPCProcessResponse &Response,
               const FString &NpcId, const FString &Input,
               const FString &RunId, int32 Turn,
               const FProtocolRuntime &Runtime,
               std::function<AnyAction(const AnyAction &)> Dispatch,
               std::function<FStoreState()> GetState) {
  FNPCProcessTape NextTape = Response.Tape;
  
  const FString ObsLower = Response.Tape.Observation.ToLower();
  const TArray<FString> ActionVerbs = {TEXT("attack"), TEXT("move"), TEXT("take"), TEXT("give"), TEXT("use"), TEXT("open"), TEXT("close"), TEXT("pick")};
  
  /**
   * Recursive predicate to detect action verbs without imperative loops.
   * User Story: As a maintainer, I need this note so the surrounding code intent
   * stays clear during maintenance and debugging.
   */
  const std::function<bool(int32)> ContainsActionVerb = [&](int32 Index) -> bool {
    return Index >= ActionVerbs.Num()
               ? false
               : (ObsLower.Contains(ActionVerbs[Index])
                      ? true
                      : ContainsActionVerb(Index + 1));
  };
  
  NextTape.DecisionIntent.ActionType = ContainsActionVerb(0) ? TEXT("INTERACT") : TEXT("SPEAK");

  const TArray<FString> TargetTokens = {TEXT("to"), TEXT("at"), TEXT("on"), TEXT("with")};

  /**
   * Recursive target extractor — walks the preposition list without an
   * imperative loop. Returns the trimmed substring after the first matching
   * " <token> " separator, or empty when no token matches.
   */
  const std::function<FString(const FString &, int32)> ExtractTargetRecursive =
      [&](const FString &Obs, int32 Index) -> FString {
    return Index >= TargetTokens.Num()
               ? FString(TEXT(""))
               : [&]() -> FString {
                   const FString &Token = TargetTokens[Index];
                   const int32 Pos = Obs.Find(TEXT(" ") + Token + TEXT(" "));
                   return Pos != INDEX_NONE
                              ? Obs.RightChop(Pos + Token.Len() + 2)
                                    .TrimStartAndEnd()
                              : ExtractTargetRecursive(Obs, Index + 1);
                 }();
  };

  NextTape.DecisionIntent.Target =
      NextTape.DecisionIntent.ActionType == TEXT("INTERACT")
          ? ExtractTargetRecursive(ObsLower, 0)
          : FString(TEXT(""));

  NextTape.DecisionIntent.Goal =
      Response.Tape.Memories.Num() > 0
          ? FString::Printf(
                TEXT("Respond to: %s (with %d recalled memories)"),
                *Response.Tape.Observation, Response.Tape.Memories.Num())
          : FString::Printf(TEXT("Respond to: %s"),
                            *Response.Tape.Observation);

  NextTape.bDecisionCompleted = true;

  return RunProtocolTurn(
      NpcId, Input, RunId, NextTape,
      SerializeDecisionResult(NextTape.DecisionIntent.Goal, NextTape.DecisionIntent.ActionType, NextTape.DecisionIntent.Target),
      true, Turn + 1, Runtime, Dispatch, GetState);
}

/**
 * Handles the Reasoning protocol instruction.
 * User Story: As protocol instruction dispatch, I need the Reasoning step
 * acknowledged so the multi-round loop can advance past it without stalling.
 *
 * Architectural note (2026-04-28): SLM inference is now API-hosted. The API
 * runs the model itself and populates Tape.ReasoningText / Tape.ResponseText
 * before returning. The SDK's only remaining responsibility for a Reasoning
 * step is to mark the tape completed and continue. Local cortex execution is
 * deliberately not invoked here.
 */
inline func::AsyncResult<FAgentResponse>
HandleReasoning(const FNPCProcessResponse &Response,
                const FString &NpcId, const FString &Input,
                const FString &RunId, int32 Turn,
                const FProtocolRuntime &Runtime,
                std::function<AnyAction(const AnyAction &)> Dispatch,
                std::function<FStoreState()> GetState) {
  FNPCProcessTape NextTape = Response.Tape;
  NextTape.bReasoningCompleted = true;

  return RunProtocolTurn(
      NpcId, Input, RunId, NextTape,
      SerializeReasoningResult(NextTape.ReasoningOutput.ReasoningText, NextTape.ReasoningOutput.ResponseText),
      true, Turn + 1, Runtime, Dispatch, GetState);
}

/**
 * Handles the Finalize protocol instruction by validating the verdict,
 * persisting memory, and applying state transforms.
 * User Story: As protocol instruction dispatch, I need finalization handled
 * as a pure expression so the instruction ternary stays flat.
 */
inline func::AsyncResult<FAgentResponse>
HandleFinalize(const FNPCInstruction &Instruction,
               const FString &NpcId, const FString &Input,
               const FString &RunId,
               const FProtocolRuntime &Runtime,
               std::function<AnyAction(const AnyAction &)> Dispatch,
               std::function<FStoreState()> GetState) {
  FVerdictResponse Verdict;
  Verdict.bValid = Instruction.bValid;
  Verdict.Signature = Instruction.Signature;
  Verdict.MemoryStore = Instruction.MemoryStore;
  Verdict.StateDelta = Instruction.StateTransform;
  Verdict.Dialogue = Instruction.Dialogue;
  Verdict.bHasAction = Instruction.bHasAction;
  Verdict.Action = Instruction.Action;
  Dispatch(DirectiveSlice::Actions::VerdictValidated(RunId, Verdict));

  return !Instruction.bValid
             ? (Dispatch(NPCSlice::Actions::BlockAction(
                    NpcId, Instruction.Dialogue.IsEmpty()
                               ? FString(TEXT("Validation failed"))
                               : Instruction.Dialogue)),
                ResolveAsync(BuildAgentResponse(Instruction)))
             : func::AsyncChain::then<rtk::FEmptyPayload, FAgentResponse>(
                   PersistMemoryInstructions(Instruction.MemoryStore, 0,
                                             Runtime, Dispatch, GetState),
                   [NpcId, Input, Instruction, Dispatch,
                    GetState](const rtk::FEmptyPayload &) {
                     HasStatePayload(Instruction.StateTransform)
                         ? (Dispatch(NPCSlice::Actions::UpdateNPCState(
                                NpcId, Instruction.StateTransform)),
                            void())
                         : void();

                     Dispatch(NPCSlice::Actions::SetLastAction(
                         NpcId, Instruction.Action, Instruction.bHasAction));

                     Dispatch(NPCSlice::Actions::AddToHistory(
                         NpcId, TEXT("user"), Input));
                     Dispatch(NPCSlice::Actions::AddToHistory(
                         NpcId, TEXT("assistant"), Instruction.Dialogue));

                     return ResolveAsync(BuildAgentResponse(Instruction));
                   });
}

inline func::AsyncResult<FAgentResponse>
RunProtocolTurn(const FString &NpcId, const FString &Input,
                const FString &RunId, const FNPCProcessTape &Tape,
                const FString &LastResult, bool bHasLastResult,
                int32 Turn, const FProtocolRuntime &Runtime,
                std::function<AnyAction(const AnyAction &)> Dispatch,
                std::function<FStoreState()> GetState) {
  return Turn >= 12
             ? (Dispatch(DirectiveSlice::Actions::DirectiveRunFailed(
                    RunId, TEXT("Max turns exceeded"))),
                RejectAsync<FAgentResponse>(
                    TEXT("Protocol loop exceeded max turns")))
             : [&]() -> func::AsyncResult<FAgentResponse> {
    FNPCProcessRequest Request;
    Request.Tape = Tape;
    Request.LastResult = LastResult;
    Request.bHasLastResult = bHasLastResult;

    return func::AsyncChain::then<FNPCProcessResponse, FAgentResponse>(
               APISlice::Endpoints::postNpcProcess(NpcId, Request)(Dispatch,
                                                                   GetState),
               [NpcId, Input, RunId, Tape, Turn, Runtime, Dispatch,
                GetState](const FNPCProcessResponse &Response)
                   -> func::AsyncResult<FAgentResponse> {
                 const FNPCInstruction &Instruction = Response.Instruction;

                 return Instruction.Type ==
                                ENPCInstructionType::IdentifyActor
                            ? HandleIdentifyActor(Response, NpcId, Input,
                                                  RunId, Turn, Runtime,
                                                  Dispatch, GetState)
                        : Instruction.Type ==
                                  ENPCInstructionType::QueryVector
                            ? HandleQueryVector(Response, Instruction, NpcId,
                                                Input, RunId, Turn, Runtime,
                                                Dispatch, GetState)
                        : Instruction.Type ==
                                  ENPCInstructionType::Decision
                            ? HandleDecision(Response, NpcId, Input, RunId,
                                             Turn, Runtime, Dispatch, GetState)
                        : Instruction.Type ==
                                  ENPCInstructionType::Reasoning
                            ? HandleReasoning(Response, NpcId, Input, RunId,
                                              Turn, Runtime, Dispatch, GetState)
                        : Instruction.Type == ENPCInstructionType::Finalize
                            ? HandleFinalize(Instruction, NpcId, Input, RunId,
                                             Runtime, Dispatch, GetState)
                            : (Dispatch(
                                   DirectiveSlice::Actions::DirectiveRunFailed(
                                       RunId,
                                       FString::Printf(
                                           TEXT("Unsupported protocol "
                                                "instruction type: %d"),
                                           static_cast<int32>(
                                               Instruction.Type)))),
                               RejectAsync<FAgentResponse>(FString::Printf(
                                   TEXT("Unsupported protocol instruction "
                                        "type: %d"),
                                   static_cast<int32>(Instruction.Type))));
               })
        .catch_([RunId, Dispatch](std::string Error) {
          Dispatch(DirectiveSlice::Actions::DirectiveRunFailed(
              RunId, FString(UTF8_TO_TCHAR(Error.c_str()))));
        });
  }();
}

inline func::AsyncResult<rtk::FEmptyPayload>
PersistMemoryInstructions(const TArray<FMemoryStoreInstruction> &Instructions,
                          int32 Index, const FProtocolRuntime &Runtime,
                          std::function<AnyAction(const AnyAction &)> Dispatch,
                          std::function<FStoreState()> GetState) {
  return Index >= Instructions.Num()
             ? ResolveAsync(rtk::FEmptyPayload{})
         : !Runtime.StoreMemory
             ? RejectAsync<rtk::FEmptyPayload>(
                   TEXT("API returned memoryStore instructions, but no memory "
                        "engine is configured"))
             : func::AsyncChain::then<FMemoryItem, rtk::FEmptyPayload>(
                   Runtime.StoreMemory(MakeMemoryItem(Instructions[Index]))(
                       Dispatch, GetState),
                   [Instructions, Index, Runtime, Dispatch,
                    GetState](const FMemoryItem &Stored) {
                     return PersistMemoryInstructions(Instructions, Index + 1,
                                                      Runtime, Dispatch,
                                                      GetState);
                   });
}

} // namespace detail

/**
 * Protocol thunks
 * User Story: As a maintainer, I need this section note so related declarations and logic stay easy to locate.
 */

inline ThunkAction<FAgentResponse, FStoreState>
processNPC(const FString &NpcId, const FString &Input = TEXT(""),
           const FString &ContextJson = TEXT("{}"),
           const FString &Persona = TEXT(""),
           const FAgentState &InitialState = FAgentState(),
           const FProtocolRuntime &Runtime = FProtocolRuntime()) {
  return [NpcId, Input, ContextJson, Persona, InitialState, Runtime](
             std::function<AnyAction(const AnyAction &)> Dispatch,
             std::function<FStoreState()> GetState)
             -> func::AsyncResult<FAgentResponse> {
    const auto ExistingNpc = NPCSlice::SelectNPCById(GetState().NPCs, NpcId);
    const bool bHasExplicitState =
        !InitialState.JsonData.IsEmpty() && InitialState.JsonData != TEXT("{}");

    const FString ResolvedPersona =
        ExistingNpc.hasValue && Persona.IsEmpty()
            ? ExistingNpc.value.Persona
            : Persona;

    const FAgentState CurrentState =
        ExistingNpc.hasValue && !bHasExplicitState
            ? ExistingNpc.value.State
            : InitialState;

    return [&]() -> func::AsyncResult<FAgentResponse> {
      !ExistingNpc.hasValue
          ? [&]() {
              FNPCInternalState Info;
              Info.Id = NpcId;
              Info.Persona = ResolvedPersona;
              Info.State = InitialState;
              Dispatch(NPCSlice::Actions::SetNPCInfo(Info));
            }()
          : (NPCSlice::SelectActiveNpcId(GetState().NPCs) != NpcId
                 ? (Dispatch(NPCSlice::Actions::SetActiveNPC(NpcId)), void())
                 : void());

      const FString RunId = FString::Printf(
          TEXT("%s:%lld"), *NpcId, FDateTime::UtcNow().ToUnixTimestamp());
      Dispatch(
          DirectiveSlice::Actions::DirectiveRunStarted(RunId, NpcId, Input));

      FNPCProcessTape Tape = TypeFactory::ProcessTape(Input, ContextJson,
                                                      CurrentState,
                                                      ResolvedPersona);
      Tape.Memories.Empty();
      Tape.bVectorQueried = false;

      return detail::RunProtocolTurn(NpcId, Input, RunId, Tape, TEXT(""), false,
                                     0, Runtime, Dispatch, GetState);
    }();
  };
}

} // namespace rtk
