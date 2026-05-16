#pragma once
/**
 * uplink traffic enters here; keep the contract hard-edged
 * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
 */

#include "../Core/AsyncHttp.h"
#include "../Core/JsonInterop.h"
#include "../Core/rtk.hpp"
#include "../RuntimeConfig.h"
#include "../Types.h"
#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "GenericPlatform/GenericPlatformHttp.h"
#include "JsonObjectConverter.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

struct FStoreState;

namespace APISlice {

using namespace rtk;

struct FAPIState {
  FString LastEndpoint;
  FString Status;
  FString Error;

  FAPIState() : Status(TEXT("idle")) {}
};

extern rtk::ApiSlice<FStoreState> ForbocAiApi;

} // namespace APISlice

#include "APICodecs.h"
#include "APIEndpoints.h"

namespace APISlice {

inline Slice<FAPIState> CreateAPISlice() {
  return buildSlice(sliceBuilder<FAPIState>(TEXT("forbocApi"), FAPIState()));
}

} // namespace APISlice
