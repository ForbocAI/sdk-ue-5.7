#include "Core/rtk.hpp"
#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "rtk_test_mocks.h"

using namespace rtk;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRtkEntityAdapterTest,
                                 "ForbocAI.Core.RTK.EntityAdapter",
                                 EAutomationTestFlags_ApplicationContextMask |
                                     EAutomationTestFlags::EngineFilter)
bool FRtkEntityAdapterTest::RunTest(const FString &Parameters) {
  auto Adapter = createEntityAdapter<FNpcMockState>(
      [](const FNpcMockState &E) { return E.Id; });
  auto State = Adapter.getInitialState();
  auto Selectors = Adapter.getSelectors();

  State = Adapter.addOne(State, FNpcMockState{TEXT("1"), 100});
  TestEqual("addOne total count", Selectors.selectTotal(State), 1);

  auto Ent1 = Selectors.selectById(State, TEXT("1"));
  TestTrue("selectById finds entity", Ent1.hasValue);
  TestEqual("selectById accurate health", Ent1.value.Health, 100);

  State = Adapter.addMany(
      State, {FNpcMockState{TEXT("2"), 200}, FNpcMockState{TEXT("3"), 300}});
  TestEqual("addMany total count", Selectors.selectTotal(State), 3);

  State = Adapter.updateOne(State, TEXT("3"), [](const FNpcMockState &E) {
    FNpcMockState Next = E;
    Next.Health = 350;
    return Next;
  });

  auto Ent3 = Selectors.selectById(State, TEXT("3"));
  TestEqual("updateOne payload accurate", Ent3.value.Health, 350);

  State = Adapter.removeOne(State, TEXT("1"));
  TestEqual("removeOne total count", Selectors.selectTotal(State), 2);
  TestFalse("removeOne removed entity from lookup",
            Selectors.selectById(State, TEXT("1")).hasValue);

  State = Adapter.setAll(State, {FNpcMockState{TEXT("4"), 400}});
  TestEqual("setAll total count", Selectors.selectTotal(State), 1);

  return true;
}
