#pragma once

// clang-format off
#include "CoreMinimal.h"
#include "NPC/NPCBaseTypes.h"
#include "GhostTypes.generated.h"
// clang-format on

USTRUCT(BlueprintType)
struct FGhostTestResult {
  GENERATED_BODY()

  UPROPERTY(BlueprintReadOnly, Category = "Ghost")
  FString Scenario;

  UPROPERTY(BlueprintReadOnly, Category = "Ghost")
  bool bPassed;

  UPROPERTY(BlueprintReadOnly, Category = "Ghost")
  FString ActualResponse;

  UPROPERTY(BlueprintReadOnly, Category = "Ghost")
  FString ErrorMessage;

  UPROPERTY(BlueprintReadOnly, Category = "Ghost")
  FString Screenshot;

  UPROPERTY(BlueprintReadOnly, Category = "Ghost")
  int32 Iteration;

  UPROPERTY(BlueprintReadOnly, Category = "Ghost")
  int64 Duration;

  FGhostTestResult() : bPassed(false), Iteration(0), Duration(0) {}
};

USTRUCT(BlueprintType)
struct FGhostHistoryEntry {
  GENERATED_BODY()

  UPROPERTY(BlueprintReadOnly, Category = "Ghost")
  FString SessionId;

  UPROPERTY(BlueprintReadOnly, Category = "Ghost")
  FString TestSuite;

  UPROPERTY(BlueprintReadOnly, Category = "Ghost")
  int64 StartedAt;

  UPROPERTY(BlueprintReadOnly, Category = "Ghost")
  int64 CompletedAt;

  UPROPERTY(BlueprintReadOnly, Category = "Ghost")
  FString Status;

  UPROPERTY(BlueprintReadOnly, Category = "Ghost")
  float PassRate;

  FGhostHistoryEntry() : StartedAt(0), CompletedAt(0), PassRate(0.0f) {}
};

USTRUCT(BlueprintType)
struct FGhostRunResponse {
  GENERATED_BODY()

  UPROPERTY(BlueprintReadOnly, Category = "Ghost")
  FString SessionId;

  UPROPERTY(BlueprintReadOnly, Category = "Ghost")
  FString RunStatus;
};

USTRUCT(BlueprintType)
struct FGhostRunRequest {
  GENERATED_BODY()

  UPROPERTY(BlueprintReadOnly, Category = "Ghost")
  FString TestSuite;

  UPROPERTY(BlueprintReadOnly, Category = "Ghost")
  int32 Duration;

  FGhostRunRequest() : Duration(300) {}
};

USTRUCT(BlueprintType)
struct FGhostConfig {
  GENERATED_BODY()

  UPROPERTY(BlueprintReadOnly, Category = "Ghost")
  FAgent Agent;

  UPROPERTY(BlueprintReadOnly, Category = "Ghost")
  TArray<FString> Scenarios;

  UPROPERTY(BlueprintReadOnly, Category = "Ghost")
  int32 MaxIterations;

  UPROPERTY(BlueprintReadOnly, Category = "Ghost")
  bool bVerbose;

  UPROPERTY(BlueprintReadOnly, Category = "Ghost")
  FString ApiUrl;

  UPROPERTY(BlueprintReadOnly, Category = "Ghost")
  FString ApiKey;

  UPROPERTY(BlueprintReadOnly, Category = "Ghost")
  FString TestSuite;

  UPROPERTY(BlueprintReadOnly, Category = "Ghost")
  int32 Duration;

  FGhostConfig() : MaxIterations(100), bVerbose(false), Duration(300) {}
};

USTRUCT(BlueprintType)
struct FGhostTestReport {
  GENERATED_BODY()

  UPROPERTY(BlueprintReadOnly, Category = "Ghost")
  FString SessionId;

  UPROPERTY(BlueprintReadOnly, Category = "Ghost")
  FGhostConfig Config;

  UPROPERTY(BlueprintReadOnly, Category = "Ghost")
  TArray<FGhostTestResult> Results;

  UPROPERTY(BlueprintReadOnly, Category = "Ghost")
  int32 TotalTests;

  UPROPERTY(BlueprintReadOnly, Category = "Ghost")
  int32 PassedTests;

  UPROPERTY(BlueprintReadOnly, Category = "Ghost")
  int32 FailedTests;

  UPROPERTY(BlueprintReadOnly, Category = "Ghost")
  int32 SkippedTests;

  UPROPERTY(BlueprintReadOnly, Category = "Ghost")
  int64 Duration;

  UPROPERTY(BlueprintReadOnly, Category = "Ghost")
  float Coverage;

  UPROPERTY(BlueprintReadOnly, Category = "Ghost")
  TMap<FString, float> Metrics;

  UPROPERTY(BlueprintReadOnly, Category = "Ghost")
  float SuccessRate;

  UPROPERTY(BlueprintReadOnly, Category = "Ghost")
  FString Summary;

  FGhostTestReport()
      : TotalTests(0), PassedTests(0), FailedTests(0), SkippedTests(0),
        Duration(0), Coverage(0.0f), SuccessRate(0.0f) {}
};

struct FGhost {
  FGhostConfig Config;
  bool bInitialized;

  FGhost() : bInitialized(false) {}
};

USTRUCT(BlueprintType)
struct FGhostStatusResponse {
  GENERATED_BODY()

  UPROPERTY(BlueprintReadOnly, Category = "Ghost")
  FString GhostSessionId;

  UPROPERTY(BlueprintReadOnly, Category = "Ghost")
  FString GhostStatus;

  UPROPERTY(BlueprintReadOnly, Category = "Ghost")
  float GhostProgress;

  UPROPERTY(BlueprintReadOnly, Category = "Ghost")
  int64 GhostStartedAt;

  UPROPERTY(BlueprintReadOnly, Category = "Ghost")
  int32 GhostDuration;

  UPROPERTY(BlueprintReadOnly, Category = "Ghost")
  TArray<FString> GhostErrors;

  FGhostStatusResponse()
      : GhostProgress(0.0f), GhostStartedAt(0), GhostDuration(0) {}
};

USTRUCT(BlueprintType)
struct FGhostResultRecord {
  GENERATED_BODY()

  UPROPERTY(BlueprintReadOnly, Category = "Ghost")
  FString TestName;

  UPROPERTY(BlueprintReadOnly, Category = "Ghost")
  bool bTestPassed;

  UPROPERTY(BlueprintReadOnly, Category = "Ghost")
  int64 TestDuration;

  UPROPERTY(BlueprintReadOnly, Category = "Ghost")
  FString TestError;

  UPROPERTY(BlueprintReadOnly, Category = "Ghost")
  FString TestScreenshot;

  FGhostResultRecord() : bTestPassed(false), TestDuration(0) {}
};

USTRUCT(BlueprintType)
struct FGhostResultsResponse {
  GENERATED_BODY()

  UPROPERTY(BlueprintReadOnly, Category = "Ghost")
  FString ResultsSessionId;

  UPROPERTY(BlueprintReadOnly, Category = "Ghost")
  int32 ResultsTotalTests;

  UPROPERTY(BlueprintReadOnly, Category = "Ghost")
  int32 ResultsPassed;

  UPROPERTY(BlueprintReadOnly, Category = "Ghost")
  int32 ResultsFailed;

  UPROPERTY(BlueprintReadOnly, Category = "Ghost")
  int32 ResultsSkipped;

  UPROPERTY(BlueprintReadOnly, Category = "Ghost")
  int64 ResultsDuration;

  UPROPERTY(BlueprintReadOnly, Category = "Ghost")
  TArray<FGhostResultRecord> ResultsTests;

  UPROPERTY(BlueprintReadOnly, Category = "Ghost")
  float ResultsCoverage;

  UPROPERTY(BlueprintReadOnly, Category = "Ghost")
  TMap<FString, float> ResultsMetrics;

  FGhostResultsResponse()
      : ResultsTotalTests(0), ResultsPassed(0), ResultsFailed(0),
        ResultsSkipped(0), ResultsDuration(0), ResultsCoverage(0.0f) {}
};

USTRUCT(BlueprintType)
struct FGhostStopResponse {
  GENERATED_BODY()

  UPROPERTY(BlueprintReadOnly, Category = "Ghost")
  bool bStopped;

  UPROPERTY(BlueprintReadOnly, Category = "Ghost")
  FString StopStatus;

  UPROPERTY(BlueprintReadOnly, Category = "Ghost")
  FString StopSessionId;

  FGhostStopResponse() : bStopped(false) {}
};

USTRUCT(BlueprintType)
struct FGhostHistoryResponse {
  GENERATED_BODY()

  UPROPERTY(BlueprintReadOnly, Category = "Ghost")
  TArray<FGhostHistoryEntry> Sessions;
};

namespace TypeFactory {

inline FGhostConfig
GhostConfig(const FAgent &Agent,
            const TArray<FString> &Scenarios = TArray<FString>(),
            int32 MaxIterations = 100, bool bVerbose = false,
            const FString &ApiUrl = TEXT(""), const FString &ApiKey = TEXT(""),
            const FString &TestSuite = TEXT(""), int32 Duration = 300) {
  FGhostConfig Config;
  Config.Agent = Agent;
  Config.Scenarios = Scenarios;
  Config.MaxIterations = MaxIterations;
  Config.bVerbose = bVerbose;
  Config.ApiUrl = ApiUrl;
  Config.ApiKey = ApiKey;
  Config.TestSuite = TestSuite;
  Config.Duration = Duration;
  return Config;
}

inline FGhostRunRequest GhostRunRequest(const FString &TestSuite,
                                        int32 Duration = 300) {
  FGhostRunRequest Request;
  Request.TestSuite = TestSuite;
  Request.Duration = Duration;
  return Request;
}

} // namespace TypeFactory
