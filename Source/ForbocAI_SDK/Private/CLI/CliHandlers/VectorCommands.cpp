#include "CLI/CliHandlers.h"
#include "CLI/CliOperations.h"
#include "RuntimeStore.h"

namespace CLIOps {
namespace Handlers {

HandlerResult HandleVector(rtk::EnhancedStore<FStoreState> &Store,
                          const FString &CommandKey,
                          const TArray<FString> &Args) {
  (void)Args;
  using func::just;
  using func::nothing;

  return CommandKey == TEXT("vector_init")
             ? (Ops::InitVector(Store),
                just(Result::Success("Vector initialized")))
             : nothing<Result>();
}

} // namespace Handlers
} // namespace CLIOps
