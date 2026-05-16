#pragma once

// clang-format off
#include "CoreMinimal.h"
#include "MemoryTypes.generated.h"
// clang-format on

/**
 * Memory Item — Immutable data.
 * User Story: As an SDK integrator, I need this type or module note so I can understand the role of the surrounding API surface quickly.
 */
USTRUCT(BlueprintType)
struct FMemoryItem {
  GENERATED_BODY()

  UPROPERTY(BlueprintReadOnly, Category = "Memory")
  FString Id;

  UPROPERTY(BlueprintReadOnly, Category = "Memory")
  FString Text;

  UPROPERTY(BlueprintReadOnly, Category = "Memory")
  TArray<float> Embedding;

  UPROPERTY(BlueprintReadOnly, Category = "Memory")
  int64 Timestamp;

  UPROPERTY(BlueprintReadOnly, Category = "Memory")
  FString Type;

  UPROPERTY(BlueprintReadOnly, Category = "Memory")
  float Importance;

  UPROPERTY(BlueprintReadOnly, Category = "Memory")
  float Similarity;

  FMemoryItem() : Timestamp(0), Importance(0.5f), Similarity(0.0f) {}
};

/**
 * Memory Configuration — Immutable data.
 * User Story: As an SDK integrator, I need this type or module note so I can understand the role of the surrounding API surface quickly.
 */
USTRUCT(BlueprintType)
struct FMemoryConfig {
  GENERATED_BODY()

  UPROPERTY(BlueprintReadOnly, Category = "Memory")
  FString DatabasePath;

  UPROPERTY(BlueprintReadOnly, Category = "Memory")
  int32 MaxMemories;

  UPROPERTY(BlueprintReadOnly, Category = "Memory")
  int32 VectorDimension;

  UPROPERTY(BlueprintReadOnly, Category = "Memory")
  bool UseGPU;

  UPROPERTY(BlueprintReadOnly, Category = "Memory")
  int32 MaxRecallResults;

  FMemoryConfig()
      : DatabasePath(TEXT("ForbocAI_Memory.db")), MaxMemories(10000),
        VectorDimension(384), UseGPU(false), MaxRecallResults(10) {}
};

/**
 * Memory Store — runtime-only data.
 * Native handles remain outside reflected UE types.
 * User Story: As an SDK integrator, I need this type or module note so I can understand the role of the surrounding API surface quickly.
 */
struct FMemoryStore {
  FMemoryConfig Config;
  TArray<FMemoryItem> Items;
  void *DatabaseHandle;
  bool bInitialized;

  FMemoryStore() : DatabaseHandle(nullptr), bInitialized(false) {}
};

/**
 * Memory Recall Request
 * User Story: As an SDK integrator, I need this type or module note so I can understand the role of the surrounding API surface quickly.
 */
USTRUCT(BlueprintType)
struct FMemoryRecallRequest {
  GENERATED_BODY()

  UPROPERTY(BlueprintReadOnly, Category = "Memory")
  FString Query;

  UPROPERTY(BlueprintReadOnly, Category = "Memory")
  int32 Limit;

  UPROPERTY(BlueprintReadOnly, Category = "Memory")
  float Threshold;

  FMemoryRecallRequest() : Limit(10), Threshold(0.7f) {}
};

USTRUCT(BlueprintType)
struct FRemoteMemoryStoreRequest {
  GENERATED_BODY()

  UPROPERTY(BlueprintReadOnly, Category = "Memory")
  FString Observation;

  UPROPERTY(BlueprintReadOnly, Category = "Memory")
  float Importance;

  FRemoteMemoryStoreRequest() : Importance(0.8f) {}
};

USTRUCT(BlueprintType)
struct FRemoteMemoryRecallRequest {
  GENERATED_BODY()

  UPROPERTY(BlueprintReadOnly, Category = "Memory")
  FString Query;

  UPROPERTY(BlueprintReadOnly, Category = "Memory")
  float Similarity;

  FRemoteMemoryRecallRequest() : Similarity(0.0f) {}
};

namespace TypeFactory {

inline FMemoryItem MemoryItem(FString Id, FString Text, FString Type,
                              float Importance, int64 Timestamp) {
  FMemoryItem M;
  M.Id = MoveTemp(Id);
  M.Text = MoveTemp(Text);
  M.Type = MoveTemp(Type);
  M.Importance = Importance;
  M.Timestamp = Timestamp;
  return M;
}

inline FMemoryConfig
MemoryConfig(FString DatabasePath = TEXT("ForbocAI_Memory.db"),
             int32 MaxMemories = 10000, int32 VectorDimension = 384,
             bool UseGPU = false, int32 MaxRecallResults = 10) {
  FMemoryConfig C;
  C.DatabasePath = MoveTemp(DatabasePath);
  C.MaxMemories = MaxMemories;
  C.VectorDimension = VectorDimension;
  C.UseGPU = UseGPU;
  C.MaxRecallResults = MaxRecallResults;
  return C;
}

inline FRemoteMemoryStoreRequest
RemoteMemoryStoreRequest(FString Observation, float Importance = 0.8f) {
  FRemoteMemoryStoreRequest Request;
  Request.Observation = MoveTemp(Observation);
  Request.Importance = Importance;
  return Request;
}

inline FRemoteMemoryRecallRequest
RemoteMemoryRecallRequest(FString Query, float Similarity = 0.0f) {
  FRemoteMemoryRecallRequest Request;
  Request.Query = MoveTemp(Query);
  Request.Similarity = Similarity;
  return Request;
}

} // namespace TypeFactory
