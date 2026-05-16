#include "API/APISlice.h"
#include "RuntimeStore.h"

/**
 * Define the global API slice instance
 * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
 */
rtk::ApiSlice<FStoreState> APISlice::ForbocAiApi = []() {
  return rtk::createApiSlice<FStoreState>(
      TEXT("forbocApi"),
      TArray<FString>{TEXT("NPC"),  TEXT("Memory"), TEXT("Cortex"),
                      TEXT("Ghost"), TEXT("Soul"), TEXT("Bridge"),
                      TEXT("Rule")});
}();
