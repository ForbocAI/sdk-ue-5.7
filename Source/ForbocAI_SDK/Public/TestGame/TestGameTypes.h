#pragma once
/**
 * Test-game type contracts — mirrors TS test-game/src/types.ts
 * User Story: As a maintainer, I need this section note so related declarations and logic stay easy to locate.
 */

#include "CoreMinimal.h"
#include "Core/rtk.hpp"

namespace TestGame {

/**
 * Spatial
 * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
 */

struct FPosition {
  int32 X;
  int32 Y;
  FPosition() : X(0), Y(0) {}
  FPosition(int32 InX, int32 InY) : X(InX), Y(InY) {}
  bool operator==(const FPosition &Other) const {
    return X == Other.X && Y == Other.Y;
  }
};

/**
 * Entity types
 * User Story: As a maintainer, I need this section note so related declarations and logic stay easy to locate.
 */

struct FGameNPC {
  FString Id;
  FString Name;
  FString Faction;
  int32 Hp;
  int32 Suspicion;
  TArray<FString> Inventory;
  TArray<FString> KnownSecrets;
  FPosition Position;

  FGameNPC() : Hp(100), Suspicion(0) {}
};

struct FPlayerState {
  FString Name;
  int32 Hp;
  bool bHidden;
  FPosition Position;
  TArray<FString> Inventory;

  FPlayerState()
      : Name(TEXT("Scout")), Hp(100), bHidden(true),
        Position(1, 1) {
    Inventory.Add(TEXT("coin-pouch"));
  }
};

/**
 * Command groups (17 required for 100% coverage)
 * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
 */

enum class ECommandGroup : uint8 {
  Status,
  NpcLifecycle,
  NpcProcessChat,
  MemoryList,
  MemoryRecall,
  MemoryStore,
  MemoryClear,
  MemoryExport,
  BridgeRules,
  BridgeValidate,
  BridgePreset,
  SoulExport,
  SoulImport,
  SoulList,
  SoulChat,
  GhostLifecycle,
  CortexInit
};

static const TArray<ECommandGroup> &RequiredGroups() {
  static const TArray<ECommandGroup> Groups = {
      ECommandGroup::Status,          ECommandGroup::NpcLifecycle,
      ECommandGroup::NpcProcessChat,  ECommandGroup::MemoryList,
      ECommandGroup::MemoryRecall,    ECommandGroup::MemoryStore,
      ECommandGroup::MemoryClear,     ECommandGroup::MemoryExport,
      ECommandGroup::BridgeRules,     ECommandGroup::BridgeValidate,
      ECommandGroup::BridgePreset,    ECommandGroup::SoulExport,
      ECommandGroup::SoulImport,      ECommandGroup::SoulList,
      ECommandGroup::SoulChat,        ECommandGroup::GhostLifecycle,
      ECommandGroup::CortexInit};
  return Groups;
}

/**
 * Command + scenario types
 * User Story: As a maintainer, I need this section note so related declarations and logic stay easy to locate.
 */

struct FCommandSpec {
  ECommandGroup Group;
  FString Command;
  TArray<FString> ExpectedRoutes;
};

enum class EEventType : uint8 { Stealth, Social, Escape, Persistence };

struct FScenarioStep {
  FString Id;
  FString Title;
  FString Description;
  EEventType EventType;
  TArray<FCommandSpec> Commands;
};

/**
 * Transcript + result types
 * User Story: As a maintainer, I need this section note so related declarations and logic stay easy to locate.
 */

enum class ETranscriptStatus : uint8 { Ok, Error };

struct FTranscriptEntry {
  FString Id;
  FString ScenarioId;
  ECommandGroup CommandGroup;
  FString Command;
  TArray<FString> ExpectedRoutes;
  ETranscriptStatus Status;
  FString Output;
  FString Timestamp;
};

struct FGameRunResult {
  bool bComplete;
  TArray<ECommandGroup> MissingGroups;
  TArray<FTranscriptEntry> Transcript;
  FString Summary;
};

/**
 * Mechanics types
 * User Story: As a maintainer, I need this section note so related declarations and logic stay easy to locate.
 */

struct FGridState {
  int32 Width;
  int32 Height;
  TArray<FPosition> Blocked;

  FGridState() : Width(8), Height(8) {
    Blocked.Add(FPosition(4, 4));
    Blocked.Add(FPosition(4, 5));
    Blocked.Add(FPosition(6, 2));
  }
};

struct FStealthState {
  bool bDoorOpen;
  int32 AlertLevel;
  FStealthState() : bDoorOpen(false), AlertLevel(0) {}
};

struct FTradeOffer {
  FString NpcId;
  FString Item;
  int32 Price;
  FTradeOffer() : Price(0) {}
};

struct FSocialState {
  FString ActiveDialogue;
  FTradeOffer ActiveTrade;
  bool bHasActiveTrade;
  FSocialState() : bHasActiveTrade(false) {}
};

struct FBridgeRulesState {
  int32 MaxJumpForce;
  int32 MaxMoveDistance;
  FString ActivePreset;
  FBridgeRulesState()
      : MaxJumpForce(500), MaxMoveDistance(2),
        ActivePreset(TEXT("default")) {}
};

/**
 * Store-domain types
 * User Story: As a maintainer, I need this section note so related declarations and logic stay easy to locate.
 */

struct FMemoryRecord {
  FString Id;
  FString NpcId;
  FString Text;
  float Importance;
  FMemoryRecord() : Importance(0.5f) {}
};

struct FInventoryState {
  TMap<FString, TArray<FString>> ByOwner;
  FInventoryState() {
    ByOwner.Add(TEXT("player"), {TEXT("coin-pouch")});
    ByOwner.Add(TEXT("miller"), {TEXT("medkit")});
  }
};

struct FSoulTrackingState {
  TMap<FString, FString> ExportsByNpc;
  TArray<FString> ImportedSoulTxIds;
};

/**
 * Terminal-domain types
 * User Story: As a maintainer, I need this section note so related declarations and logic stay easy to locate.
 */

enum class EPlayMode : uint8 { Manual, Autoplay };

struct FUIState {
  EPlayMode Mode;
  TArray<FString> Messages;
  FUIState() : Mode(EPlayMode::Autoplay) {
    Messages.Add(TEXT("SYSTEM_OVERRIDE :: terminal HUD online"));
  }
};

struct FTranscriptState {
  TArray<FTranscriptEntry> Entries;
};

/**
 * Autoplay-domain types
 * User Story: As a maintainer, I need this section note so related declarations and logic stay easy to locate.
 */

struct FHarnessState {
  TMap<ECommandGroup, bool> Covered;
};

/**
 * Validation helpers
 * User Story: As a maintainer, I need this section note so related declarations and logic stay easy to locate.
 */

struct FJumpResult {
  bool bValid;
  FString Reason;
};

struct FMoveResult {
  int32 AllowedDistance;
  bool bCapped;
};

/**
 * Returns whether a grid position is traversable.
 * User Story: As test-game movement validation, I need passability checks so
 * AI movement can reject blocked or out-of-bounds tiles.
 */
inline bool IsPassable(const FGridState &Grid, const FPosition &Pos) {
  struct CheckBlocked {
    static bool apply(const TArray<FPosition> &Blocked,
                      const FPosition &P,
                      int32 Idx) {
      return Idx >= Blocked.Num()
          ? true
          : Blocked[Idx] == P
          ? false
          : apply(Blocked, P, Idx + 1);
    }
  };
  return (Pos.X < 0 || Pos.Y < 0 || Pos.X >= Grid.Width ||
          Pos.Y >= Grid.Height)
      ? false
      : CheckBlocked::apply(Grid.Blocked, Pos, 0);
}

/**
 * Validates whether a requested jump force is allowed.
 * User Story: As test-game bridge validation, I need jump checks so excessive
 * jump force is rejected before actions are applied.
 */
inline FJumpResult ValidateJump(const FBridgeRulesState &Rules, int32 Force) {
  FJumpResult R;
  R.bValid = Force <= Rules.MaxJumpForce;
  R.Reason = !R.bValid
      ? FString::Printf(TEXT("Force %d exceeds max %d"), Force,
                         Rules.MaxJumpForce)
      : FString();
  return R;
}

/**
 * Caps requested movement distance to the configured rules.
 * User Story: As test-game movement rules, I need move distance capped so
 * actions respect the configured maximum move allowance.
 */
inline FMoveResult CapMoveDistance(const FBridgeRulesState &Rules,
                                   int32 Requested) {
  FMoveResult R;
  R.AllowedDistance =
      Requested > Rules.MaxMoveDistance ? Rules.MaxMoveDistance : Requested;
  R.bCapped = R.AllowedDistance < Requested;
  return R;
}

} // namespace TestGame
