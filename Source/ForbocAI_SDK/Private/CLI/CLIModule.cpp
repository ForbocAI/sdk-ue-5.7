#include "CLI/CLIModule.h"
#include "CLI/CliHandlers.h"
#include "CLI/CliOperations.h"
#include "RuntimeStore.h"
#include <exception>

namespace CLIOps {

namespace {

rtk::EnhancedStore<FStoreState> &GetStore() {
  static rtk::EnhancedStore<FStoreState> Store = ConfigureStore();
  return Store;
}

} // namespace

func::TestResult<void> DispatchCommand(const FString &CommandKey,
                                       const TArray<FString> &Args) {
  using Result = func::TestResult<void>;
  using namespace Handlers;
  using Handler = std::function<HandlerResult(rtk::EnhancedStore<FStoreState> &,
                                              const FString &,
                                              const TArray<FString> &)>;

  rtk::EnhancedStore<FStoreState> &Store = GetStore();

  /**
   * Phase 3.4: Handler chain — first match wins
   * User Story: As a maintainer, I need this implementation note so I can understand which milestone behavior the surrounding code is preserving.
   */
  static const std::vector<Handler> Handlers = {
      HandleSystem, HandleNpc,    HandleMemory, HandleCortex, HandleGhost,
      HandleBridge, HandleSoul,   HandleConfig, HandleVector, HandleSetup,
  };

  struct DispatchRecursive {
    static func::TestResult<void>
    apply(const std::vector<Handler> &Hs, size_t Index,
          rtk::EnhancedStore<FStoreState> &Store, const FString &Key,
          const TArray<FString> &Args) {
      return Index >= Hs.size()
                 ? Result::Failure(TCHAR_TO_UTF8(
                       *FString::Printf(TEXT("Unknown command: %s"), *Key)))
                 : [&]() -> func::TestResult<void> {
                     HandlerResult R = Hs[Index](Store, Key, Args);
                     return R.hasValue ? R.value
                                       : apply(Hs, Index + 1, Store, Key, Args);
                   }();
    }
  };

  try {
    return DispatchRecursive::apply(Handlers, 0, Store, CommandKey, Args);
  } catch (const std::exception &Error) {
    return Result::Failure(std::string(Error.what()));
  }
}

} // namespace CLIOps
