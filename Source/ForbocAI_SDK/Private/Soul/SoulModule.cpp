#include "Soul/SoulModule.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "JsonObjectConverter.h"
#include "RuntimeConfig.h"
#include "RuntimeStore.h"
#include "Serialization/JsonSerializer.h"
#include "Soul/SoulSlice.h"
#include "Soul/SoulThunks.h"

/**
 * SOUL OPERATIONS — Pure free functions
 * User Story: As a maintainer, I need this section note so related declarations and logic stay easy to locate.
 */

SoulTypes::SoulCreationResult
SoulOps::FromAgent(const FAgentState &State,
                   const TArray<FMemoryItem> &Memories, const FString &AgentId,
                   const FString &Persona) {
  try {
    FSoul Soul = TypeFactory::Soul(AgentId, TEXT("1.0.0"), TEXT("NPC"),
                                   Persona, State, Memories);
    return SoulTypes::make_right(FString(), Soul);
  } catch (const std::exception &e) {
    return SoulTypes::make_left(FString(e.what()), FSoul{});
  }
}

SoulTypes::SoulSerializationResult SoulOps::Serialize(const FSoul &Soul) {
  try {
    FString JsonString;
    return FJsonObjectConverter::UStructToJsonObjectString(Soul, JsonString)
               ? SoulTypes::make_right(FString(), JsonString)
               : SoulTypes::make_left(
                     FString(TEXT("Failed to serialize Soul to JSON")),
                     FString());
  } catch (const std::exception &e) {
    return SoulTypes::make_left(FString(e.what()), FString());
  }
}

SoulTypes::SoulDeserializationResult SoulOps::Deserialize(const FString &Json) {
  try {
    FSoul Soul;
    return FJsonObjectConverter::JsonObjectStringToUStruct(Json, &Soul, 0, 0)
               ? SoulTypes::make_right(FString(), Soul)
               : SoulTypes::make_left(
                     FString(TEXT("Failed to deserialize JSON to Soul")),
                     FSoul{});
  } catch (const std::exception &e) {
    return SoulTypes::make_left(FString(e.what()), FSoul{});
  }
}

SoulTypes::SoulValidationResult SoulOps::Validate(const FSoul &Soul) {
  /**
   * Use the functional validation pipeline from helpers
   * User Story: As a maintainer, I need this section note so related declarations and logic stay easy to locate.
   */
  auto result =
      func::runValidation(SoulHelpers::soulValidationPipeline(), Soul);

  return result.isLeft ? SoulTypes::make_left(result.left, FSoul{})
                       : SoulTypes::make_right(FString(), Soul);
}

SoulTypes::SoulExportResult SoulOps::ExportToArweave(const FSoul &Soul,
                                                     const FString &ApiUrl) {
  return SoulTypes::AsyncResult<FSoulExportResult>::create(
      [Soul, ApiUrl](std::function<void(FSoulExportResult)> resolve,
                     std::function<void(std::string)> reject) {
        SDKConfig::SetApiConfig(ApiUrl, SDKConfig::GetApiKey());
        auto Store = ConfigureStore();

        Store.dispatch(rtk::exportSoulThunk(Soul))
            .then([resolve](const FSoulExportResult &Result) { resolve(Result); })
            .catch_([reject](std::string Error) { reject(Error); })
            .execute();
      });
}
