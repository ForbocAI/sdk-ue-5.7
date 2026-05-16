#pragma once
/**
 * personas and souls; the essence of the machine
 * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
 */

#include "Core/rtk.hpp"
#include "CoreMinimal.h"
#include "Types.h"

namespace SoulSlice {

using namespace rtk;

struct FSoulSliceState {
  FString ExportStatus;
  FString ImportStatus;
  FSoulExportResult LastExport;
  bool bHasLastExport;
  FSoul LastImport;
  bool bHasLastImport;
  TArray<FSoulListItem> AvailableSouls;
  FString Error;

  FSoulSliceState()
      : ExportStatus(TEXT("idle")), ImportStatus(TEXT("idle")),
        bHasLastExport(false), bHasLastImport(false) {}
};

namespace Actions {

/**
 * Returns the cached action creator for remote soul export start events.
 * User Story: As soul export orchestration, I need a stable pending action
 * creator so reducers and thunks reuse one export-start contract.
 */
inline const EmptyActionCreator &RemoteExportSoulPendingActionCreator() {
  static const EmptyActionCreator ActionCreator =
      createAction(TEXT("soul/remoteExportPending"));
  return ActionCreator;
}

/**
 * Returns the cached action creator for successful soul exports.
 * User Story: As soul export completion flows, I need a reusable success
 * action creator so export results are stored consistently.
 */
inline const ActionCreator<FSoulExportResult> &
RemoteExportSoulSuccessActionCreator() {
  static const ActionCreator<FSoulExportResult> ActionCreator =
      createAction<FSoulExportResult>(TEXT("soul/remoteExportSuccess"));
  return ActionCreator;
}

/**
 * Returns the cached action creator for failed soul exports.
 * User Story: As soul export error handling, I need a reusable failure action
 * creator so errors propagate through one contract.
 */
inline const ActionCreator<FString> &RemoteExportSoulFailedActionCreator() {
  static const ActionCreator<FString> ActionCreator =
      createAction<FString>(TEXT("soul/remoteExportFailed"));
  return ActionCreator;
}

/**
 * Returns the cached action creator for soul import start events.
 * User Story: As soul import orchestration, I need one pending action creator
 * so reducers can track import startup consistently.
 */
inline const EmptyActionCreator &ImportSoulPendingActionCreator() {
  static const EmptyActionCreator ActionCreator =
      createAction(TEXT("soul/importPending"));
  return ActionCreator;
}

/**
 * Returns the cached action creator for successful soul imports.
 * User Story: As soul import flows, I need a reusable success action creator
 * so imported payloads are stored the same way everywhere.
 */
inline const ActionCreator<FSoul> &ImportSoulSuccessActionCreator() {
  static const ActionCreator<FSoul> ActionCreator =
      createAction<FSoul>(TEXT("soul/importSuccess"));
  return ActionCreator;
}

/**
 * Returns the cached action creator for failed soul imports.
 * User Story: As soul import error handling, I need a reusable failure action
 * creator so import errors stay consistent across callers.
 */
inline const ActionCreator<FString> &ImportSoulFailedActionCreator() {
  static const ActionCreator<FString> ActionCreator =
      createAction<FString>(TEXT("soul/importFailed"));
  return ActionCreator;
}

/**
 * Returns the cached action creator for replacing the available soul list.
 * User Story: As soul catalog views, I need a stable action creator so remote
 * listings can populate the slice with one contract.
 */
inline const ActionCreator<TArray<FSoulListItem>> &SetSoulListActionCreator() {
  static const ActionCreator<TArray<FSoulListItem>> ActionCreator =
      createAction<TArray<FSoulListItem>>(TEXT("soul/setSoulList"));
  return ActionCreator;
}

/**
 * Returns the cached action creator for resetting soul state.
 * User Story: As cleanup flows, I need a dedicated clear action creator so the
 * soul slice can return to defaults predictably.
 */
inline const EmptyActionCreator &ClearSoulStateActionCreator() {
  static const EmptyActionCreator ActionCreator =
      createAction(TEXT("soul/clearSoulState"));
  return ActionCreator;
}

/**
 * Creates an action that marks a remote soul export as pending.
 * User Story: As export status tracking, I need pending state recorded so the
 * UI can show that an export is in flight.
 */
inline AnyAction RemoteExportSoulPending() {
  return RemoteExportSoulPendingActionCreator()();
}

/**
 * Creates an action that stores the latest remote soul export result.
 * User Story: As export result consumers, I need the final export metadata
 * saved so downstream flows can use the produced soul reference.
 */
inline AnyAction RemoteExportSoulSuccess(const FSoulExportResult &Result) {
  return RemoteExportSoulSuccessActionCreator()(Result);
}

/**
 * Creates an action that stores a remote soul export failure.
 * User Story: As export error handling, I need failures captured so callers
 * can explain why an export did not complete.
 */
inline AnyAction RemoteExportSoulFailed(const FString &Error) {
  return RemoteExportSoulFailedActionCreator()(Error);
}

/**
 * Creates an action that marks a soul import as pending.
 * User Story: As import status tracking, I need pending state stored so the UI
 * can reflect that a soul import is underway.
 */
inline AnyAction ImportSoulPending() {
  return ImportSoulPendingActionCreator()();
}

/**
 * Creates an action that stores the latest imported soul payload.
 * User Story: As import result consumers, I need the imported soul preserved
 * so later reducers and views can reuse it.
 */
inline AnyAction ImportSoulSuccess(const FSoul &Soul) {
  return ImportSoulSuccessActionCreator()(Soul);
}

/**
 * Creates an action that stores a soul import failure.
 * User Story: As import error handling, I need the failure recorded so callers
 * can surface useful feedback to the user.
 */
inline AnyAction ImportSoulFailed(const FString &Error) {
  return ImportSoulFailedActionCreator()(Error);
}

/**
 * Creates an action that replaces the available soul list.
 * User Story: As soul listing flows, I need the latest list stored so browsing
 * and selection use current remote data.
 */
inline AnyAction SetSoulList(const TArray<FSoulListItem> &SoulList) {
  return SetSoulListActionCreator()(SoulList);
}

/**
 * Creates an action that resets the soul slice to its initial state.
 * User Story: As cleanup flows, I need soul state cleared so later exports and
 * imports start from a known baseline.
 */
inline AnyAction ClearSoulState() { return ClearSoulStateActionCreator()(); }

} // namespace Actions

/**
 * Builds the soul slice reducer and initial state.
 * User Story: As soul runtime setup, I need one slice factory so export and
 * import actions are wired into the store consistently.
 */
inline Slice<FSoulSliceState> CreateSoulSlice() {
  return buildSlice(
      sliceBuilder<FSoulSliceState>(TEXT("soul"), FSoulSliceState()) |
      addExtraCase(
          Actions::RemoteExportSoulPendingActionCreator(),
          [](const FSoulSliceState &State,
             const Action<rtk::FEmptyPayload> &Action) -> FSoulSliceState {
            FSoulSliceState Next = State;
            Next.ExportStatus = TEXT("exporting");
            Next.Error.Empty();
            return Next;
          })
      | addExtraCase(
          Actions::RemoteExportSoulSuccessActionCreator(),
          [](const FSoulSliceState &State,
             const Action<FSoulExportResult> &Action) -> FSoulSliceState {
            FSoulSliceState Next = State;
            Next.ExportStatus = TEXT("success");
            Next.LastExport = Action.PayloadValue;
            Next.bHasLastExport = true;
            return Next;
          })
      | addExtraCase(Actions::RemoteExportSoulFailedActionCreator(),
                    [](const FSoulSliceState &State,
                       const Action<FString> &Action) -> FSoulSliceState {
                      FSoulSliceState Next = State;
                      Next.ExportStatus = TEXT("failed");
                      Next.Error = Action.PayloadValue;
                      return Next;
                    }) |
      addExtraCase(
          Actions::ImportSoulPendingActionCreator(),
          [](const FSoulSliceState &State,
             const Action<rtk::FEmptyPayload> &Action) -> FSoulSliceState {
            FSoulSliceState Next = State;
            Next.ImportStatus = TEXT("importing");
            Next.Error.Empty();
            return Next;
          })
      | addExtraCase(Actions::ImportSoulSuccessActionCreator(),
                    [](const FSoulSliceState &State,
                       const Action<FSoul> &Action) -> FSoulSliceState {
                      FSoulSliceState Next = State;
                      Next.ImportStatus = TEXT("success");
                      Next.LastImport = Action.PayloadValue;
                      Next.bHasLastImport = true;
                      return Next;
                    }) |
      addExtraCase(Actions::ImportSoulFailedActionCreator(),
                    [](const FSoulSliceState &State,
                       const Action<FString> &Action) -> FSoulSliceState {
                      FSoulSliceState Next = State;
                      Next.ImportStatus = TEXT("failed");
                      Next.Error = Action.PayloadValue;
                      return Next;
                    }) |
      addExtraCase(
          Actions::SetSoulListActionCreator(),
          [](const FSoulSliceState &State,
             const Action<TArray<FSoulListItem>> &Action) -> FSoulSliceState {
            FSoulSliceState Next = State;
            Next.AvailableSouls = Action.PayloadValue;
            return Next;
          }) |
      addExtraCase(
          Actions::ClearSoulStateActionCreator(),
          [](const FSoulSliceState &State,
             const Action<rtk::FEmptyPayload> &Action) -> FSoulSliceState {
            return FSoulSliceState();
          }));
}

} // namespace SoulSlice
