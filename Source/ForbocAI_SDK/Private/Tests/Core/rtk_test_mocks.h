#pragma once

#include "CoreMinimal.h"

struct FNpcMockState {
  FString Id;
  int32 Health;

  bool operator==(const FNpcMockState &Other) const {
    return Id == Other.Id && Health == Other.Health;
  }
};

struct FAppMockState {
  FNpcMockState ActiveNpc;
};
