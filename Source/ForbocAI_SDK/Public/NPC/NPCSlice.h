#pragma once
/**
 * SYSTEM_OVERRIDE denied unless npc state is coherent end to end
 * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
 */

#include "Core/rtk.hpp"
#include "CoreMinimal.h"
#include "NPC/NPCSliceActions.h"
#include "NPC/NPCTypes.h"
#include "Types.h"

namespace NPCSlice {

using namespace rtk;
using namespace func;

/**
 * Builds the NPC slice reducer and extra cases.
 * User Story: As NPC runtime setup, I need one slice factory so NPC lifecycle
 * actions update state through a single reducer contract.
 */
inline Slice<FNPCSliceState> CreateNPCSlice() {
  return buildSlice(sliceBuilder<FNPCSliceState>(TEXT("npc"), FNPCSliceState()) |
                    addExtraCase(
          Actions::SetNPCInfoActionCreator(),
          [](const FNPCSliceState &State,
             const Action<FNPCInternalState> &Action) -> FNPCSliceState {
            FNPCSliceState Next = State;
            FNPCInternalState NewNPC = Action.PayloadValue;
            NewNPC.StateLog.Empty();
            NewNPC.StateLog.Add(MakeStateLogEntry(NewNPC.State, NewNPC.State));
            Next.Entities = GetNPCAdapter().upsertOne(Next.Entities, NewNPC);
            Next.ActiveNpcId = NewNPC.Id;
            return Next;
          }) |
                    addExtraCase(Actions::SetActiveNPCActionCreator(),
                    [](const FNPCSliceState &State,
                       const Action<FString> &Action) -> FNPCSliceState {
                      FNPCSliceState Next = State;
                      Next.ActiveNpcId = Action.PayloadValue;
                      return Next;
                    }) |
                    addExtraCase(
          Actions::SetNPCStateActionCreator(),
          [](const FNPCSliceState &State,
             const Action<FSetNPCStatePayload> &Action) -> FNPCSliceState {
            FNPCSliceState Next = State;
            const FSetNPCStatePayload &Payload = Action.PayloadValue;
            Next.Entities = GetNPCAdapter().updateOne(
                Next.Entities, Payload.Id,
                [Payload](const FNPCInternalState &Existing) {
                  FNPCInternalState Updated = Existing;
                  Updated.State = Payload.State;
                  Updated.StateLog.Add(
                      MakeStateLogEntry(Payload.State, Payload.State));
                  return Updated;
                });
            return Next;
          }) |
                    addExtraCase(
          Actions::UpdateNPCStateActionCreator(),
          [](const FNPCSliceState &State,
             const Action<FUpdateNPCStatePayload> &Action) -> FNPCSliceState {
            FNPCSliceState Next = State;
            const FUpdateNPCStatePayload &Payload = Action.PayloadValue;
            Next.Entities = GetNPCAdapter().updateOne(
                Next.Entities, Payload.Id,
                [Payload](const FNPCInternalState &Existing) {
                  FNPCInternalState Updated = Existing;
                  Updated.State =
                      MergeStateDelta(Existing.State, Payload.Delta);
                  Updated.StateLog.Add(
                      MakeStateLogEntry(Payload.Delta, Updated.State));
                  return Updated;
                });
            return Next;
          }) |
                    addExtraCase(
          Actions::AddToHistoryActionCreator(),
          [](const FNPCSliceState &State,
             const Action<FAddToHistoryPayload> &Action) -> FNPCSliceState {
            FNPCSliceState Next = State;
            const FAddToHistoryPayload &Payload = Action.PayloadValue;
            Next.Entities = GetNPCAdapter().updateOne(
                Next.Entities, Payload.Id,
                [Payload](const FNPCInternalState &Existing) {
                  FNPCInternalState Updated = Existing;
                  FNPCHistoryEntry Entry;
                  Entry.Role = Payload.Role;
                  Entry.Content = Payload.Content;
                  Updated.History.Add(Entry);
                  return Updated;
                });
            return Next;
          }) |
                    addExtraCase(
          Actions::SetHistoryActionCreator(),
          [](const FNPCSliceState &State,
             const Action<FSetHistoryPayload> &Action) -> FNPCSliceState {
            FNPCSliceState Next = State;
            const FSetHistoryPayload &Payload = Action.PayloadValue;
            Next.Entities = GetNPCAdapter().updateOne(
                Next.Entities, Payload.Id,
                [Payload](const FNPCInternalState &Existing) {
                  FNPCInternalState Updated = Existing;
                  Updated.History = Payload.History;
                  return Updated;
                });
            return Next;
          }) |
                    addExtraCase(
          Actions::SetLastActionActionCreator(),
          [](const FNPCSliceState &State,
             const Action<FSetLastActionPayload> &Action) -> FNPCSliceState {
            FNPCSliceState Next = State;
            const FSetLastActionPayload &Payload = Action.PayloadValue;
            Next.Entities = GetNPCAdapter().updateOne(
                Next.Entities, Payload.Id,
                [Payload](const FNPCInternalState &Existing) {
                  FNPCInternalState Updated = Existing;
                  return (Payload.bHasAction
                              ? (Updated.LastAction = Payload.Action,
                                 Updated.bHasLastAction = true, void())
                              : (Updated.LastAction = FAgentAction{},
                                 Updated.bHasLastAction = false, void()),
                          Updated);
                });
            return Next;
          }) |
                    addExtraCase(
          Actions::BlockActionActionCreator(),
          [](const FNPCSliceState &State,
             const Action<FBlockActionPayload> &Action) -> FNPCSliceState {
            FNPCSliceState Next = State;
            const FBlockActionPayload &Payload = Action.PayloadValue;
            Next.Entities = GetNPCAdapter().updateOne(
                Next.Entities, Payload.Id,
                [Payload](const FNPCInternalState &Existing) {
                  FNPCInternalState Updated = Existing;
                  Updated.bIsBlocked = true;
                  Updated.BlockReason = Payload.Reason;
                  return Updated;
                });
            return Next;
          }) |
                    addExtraCase(Actions::ClearBlockActionCreator(),
                    [](const FNPCSliceState &State,
                       const Action<FString> &Action) -> FNPCSliceState {
                      FNPCSliceState Next = State;
                      Next.Entities = GetNPCAdapter().updateOne(
                          Next.Entities, Action.PayloadValue,
                          [](const FNPCInternalState &Existing) {
                            FNPCInternalState Updated = Existing;
                            Updated.bIsBlocked = false;
                            Updated.BlockReason.Empty();
                            return Updated;
                          });
                      return Next;
                    }) |
                    addExtraCase(Actions::RemoveNPCActionCreator(),
                    [](const FNPCSliceState &State,
                       const Action<FString> &Action) -> FNPCSliceState {
                      FNPCSliceState Next = State;
                      return (Next.Entities = GetNPCAdapter().removeOne(
                                  Next.Entities, Action.PayloadValue),
                              Next.ActiveNpcId == Action.PayloadValue
                                  ? (Next.ActiveNpcId.Empty(), void())
                                  : void(),
                              Next);
                    }));
}

/**
 * Selects a single NPC by id.
 * User Story: As NPC lookups, I need direct access to one NPC so reducers,
 * thunks, and UI code can target the correct entity.
 */
inline func::Maybe<FNPCInternalState> SelectNPCById(const FNPCSliceState &State,
                                                    const FString &Id) {
  return GetNPCAdapter().getSelectors().selectById(State.Entities, Id);
}

/**
 * Selects every NPC currently held in slice state.
 * User Story: As NPC inspection flows, I need the full entity list so tools
 * and gameplay systems can review current NPC state.
 */
inline TArray<FNPCInternalState> SelectAllNPCs(const FNPCSliceState &State) {
  return GetNPCAdapter().getSelectors().selectAll(State.Entities);
}

/**
 * User Story: As NPC runtime state access, I need a selector for the active NPC
 * id so UI and orchestration logic can resolve the current actor consistently.
 * (From TS)
 */
inline FString SelectActiveNpcId(const FNPCSliceState &State) {
  return State.ActiveNpcId;
}

/**
 * Selects the currently active NPC when one is set.
 * User Story: As NPC orchestration, I need the active NPC resolved so the
 * current actor can be processed without manual id lookups.
 */
inline func::Maybe<FNPCInternalState>
SelectActiveNPC(const FNPCSliceState &State) {
  return State.ActiveNpcId.IsEmpty()
             ? func::nothing<FNPCInternalState>()
             : SelectNPCById(State, State.ActiveNpcId);
}

} // namespace NPCSlice
