#include "Core/rtk.hpp"
#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "rtk_test_mocks.h"

using namespace rtk;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRtkApiSliceTest, "ForbocAI.Core.RTK.ApiSlice",
                                 EAutomationTestFlags_ApplicationContextMask |
                                     EAutomationTestFlags::EngineFilter)
bool FRtkApiSliceTest::RunTest(const FString &Parameters) {
  /**
   * 1. Define an API Endpoint
   * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
   */
  ApiEndpoint<FString, int32> GetUserEndpoint;
  GetUserEndpoint.EndpointName = TEXT("getUser");
  GetUserEndpoint.ProvidesTags = {{TEXT("User"), TEXT("ID")}};

  /**
   * Mock HTTP Builder that resolves after parsing
   * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
   */
  GetUserEndpoint.RequestBuilder = [](const FString &UserId) {
    return func::AsyncResult<func::HttpResult<int32>>::create(
        [UserId](auto Resolve, auto Reject) {
          if (UserId == TEXT("error")) {
            func::HttpResult<int32> FailedRes;
            FailedRes.bSuccess = false;
            FailedRes.error = "Mock Network Failure";
            FailedRes.ResponseCode = 500;
            Resolve(FailedRes);
          } else {
            func::HttpResult<int32> SuccessRes;
            SuccessRes.bSuccess = true;
            SuccessRes.data = 42;
            SuccessRes.ResponseCode = 200;
            Resolve(SuccessRes);
          }
        });
  };

  /**
   * 2. Register Endpoint in ApiSlice
   * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
   */
  ApiSlice<FAppMockState> TestApi =
      createApiSlice<FAppMockState>(TEXT("testApi"), TArray<FString>());

  auto GetUserThunk = injectEndpoint(TestApi, GetUserEndpoint);

  TArray<FString> EventLog;
  std::function<AnyAction(const AnyAction &)> MockDispatch =
      [&EventLog](const AnyAction &Action) {
        EventLog.Add(Action.Type);
        return Action;
      };
  std::function<FAppMockState()> MockGetState = []() {
    return FAppMockState{};
  };

  /**
   * 3. Test Successful HTTP Call
   * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
   */
  auto SuccessOp = GetUserThunk(TEXT("123"));
  SuccessOp(MockDispatch, MockGetState).execute();

  TestEqual("Dispatched pending first (API success)", EventLog[0],
            FString(TEXT("testApi/getUser/pending")));
  TestEqual("Dispatched fulfilled second (API success)", EventLog[1],
            FString(TEXT("testApi/getUser/fulfilled")));
  EventLog.Empty();

  /**
   * 4. Test Failed HTTP Call
   * User Story: As a maintainer, I need this step note so I can follow the scenario progression and reason about the expected state changes.
   */
  auto FailOp = GetUserThunk(TEXT("error"));
  FailOp(MockDispatch, MockGetState).execute();

  TestEqual("Dispatched pending first (API fail)", EventLog[0],
            FString(TEXT("testApi/getUser/pending")));
  TestEqual("Dispatched rejected second (API fail)", EventLog[1],
            FString(TEXT("testApi/getUser/rejected")));

  return true;
}
