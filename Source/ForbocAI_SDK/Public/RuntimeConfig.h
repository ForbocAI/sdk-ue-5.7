#pragma once

#include "CoreMinimal.h"
#include "Core/functional_core.hpp"
#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include <winsock2.h>
#include "Windows/HideWindowsPlatformTypes.h"
#elif PLATFORM_MAC || PLATFORM_UNIX
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

/**
 * SDK Configuration — env var > config file > defaults precedence.
 * Equivalent to TS nodeConfig.ts plus CLI config persistence in sdkOps.ts.
 * User Story: As an SDK integrator, I need this type or module note so I can understand the role of the surrounding API surface quickly.
 */
namespace SDKConfig {

/**
 * Default: localhost for dev. Override via FORBOCAI_API_URL env var or config file.
 * User Story: As a maintainer, I need this note so the surrounding API intent stays clear during maintenance and integration.
 */
inline constexpr TCHAR DEFAULT_API_URL[] = TEXT("http://localhost:8080");
inline constexpr TCHAR PRODUCTION_API_URL[] = TEXT("https://api.forboc.ai");
inline constexpr int32 DEFAULT_VECTOR_DIMENSION = 384;
inline constexpr int32 DEFAULT_MAX_RECALL_RESULTS = 10;

/**
 * Returns the mutable storage backing the configured API URL.
 * User Story: As config loading and override helpers, I need shared URL
 * storage so disk, environment, and runtime writes update one source of truth.
 */
inline FString &ApiUrlStorage() {
  static FString Value = DEFAULT_API_URL;
  return Value;
}

/**
 * Returns the mutable storage backing the configured API key.
 * User Story: As config loading and override helpers, I need shared API key
 * storage so every config source reads and writes the same secret value.
 */
inline FString &ApiKeyStorage() {
  static FString Value = TEXT("");
  return Value;
}

/**
 * Returns the mutable storage backing the configured model path.
 * User Story: As local model configuration, I need shared path storage so
 * runtime code and tooling resolve the same model location.
 */
inline FString &ModelPathStorage() {
  static FString Value = TEXT("");
  return Value;
}

/**
 * Returns the mutable storage backing the configured database path.
 * User Story: As memory persistence configuration, I need shared database path
 * storage so local memory features point at the same file.
 */
inline FString &DatabasePathStorage() {
  static FString Value = TEXT("");
  return Value;
}

/**
 * Returns the mutable storage backing the configured embedding width.
 * User Story: As embedding configuration, I need shared vector dimension
 * storage so recall and indexing use the same width.
 */
inline int32 &VectorDimensionStorage() {
  static int32 Value = DEFAULT_VECTOR_DIMENSION;
  return Value;
}

/**
 * Returns the mutable storage backing the configured recall limit.
 * User Story: As recall configuration, I need shared limit storage so default
 * recall behavior stays consistent across runtime entry points.
 */
inline int32 &MaxRecallResultsStorage() {
  static int32 Value = DEFAULT_MAX_RECALL_RESULTS;
  return Value;
}

/**
 * Returns the initialization flag used to guard lazy config loading.
 * User Story: As lazy config access, I need a shared initialized flag so
 * getters can avoid reloading config on every call.
 */
inline bool &InitializedStorage() {
  static bool bInitialized = false;
  return bInitialized;
}

/**
 * Returns the optional config-path override used by tests and tooling.
 * User Story: As tests and CLI tooling, I need an override slot so config I/O
 * can target a temporary file instead of the user default path.
 */
inline FString &ConfigFilePathOverrideStorage() {
  static FString Value;
  return Value;
}

/**
 * Resets all in-memory config values back to their built-in defaults.
 * User Story: As config reload flows, I need a default reset step so stale
 * values do not survive before disk and environment overrides are reapplied.
 */
inline void ResetToDefaults() {
  ApiUrlStorage() = DEFAULT_API_URL;
  ApiKeyStorage() = TEXT("");
  ModelPathStorage() = TEXT("");
  DatabasePathStorage() = TEXT("");
  VectorDimensionStorage() = DEFAULT_VECTOR_DIMENSION;
  MaxRecallResultsStorage() = DEFAULT_MAX_RECALL_RESULTS;
}

/**
 * Extracts the port number from a localhost URL (e.g. "http://localhost:8080" → 8080).
 */
inline int32 ExtractLocalhostPort(const FString &Url) {
  const int32 LastColon =
      Url.Find(TEXT(":"), ESearchCase::IgnoreCase, ESearchDir::FromEnd);
  if (LastColon == INDEX_NONE) return 8080;
  const int32 Port = FCString::Atoi(*Url.Mid(LastColon + 1));
  return (Port > 0 && Port < 65536) ? Port : 8080;
}

/**
 * Returns true if localhost:Port accepts a TCP connection within 200 ms.
 */
inline bool IsLocalHostReachable(int32 Port) {
#if PLATFORM_WINDOWS
  WSADATA WsaData;
  if (WSAStartup(MAKEWORD(2, 2), &WsaData) != 0) return false;
  SOCKET Sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (Sock == INVALID_SOCKET) { WSACleanup(); return false; }
  u_long NonBlocking = 1;
  ioctlsocket(Sock, FIONBIO, &NonBlocking);
  sockaddr_in Addr = {};
  Addr.sin_family = AF_INET;
  Addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  Addr.sin_port = htons(static_cast<u_short>(Port));
  connect(Sock, reinterpret_cast<sockaddr *>(&Addr), sizeof(Addr));
  fd_set WriteSet;
  FD_ZERO(&WriteSet);
  FD_SET(Sock, &WriteSet);
  timeval Timeout = {0, 200000};
  const bool bUp = select(0, nullptr, &WriteSet, nullptr, &Timeout) > 0;
  closesocket(Sock);
  WSACleanup();
  return bUp;
#elif PLATFORM_MAC || PLATFORM_UNIX
  const int Sock = socket(AF_INET, SOCK_STREAM, 0);
  if (Sock < 0) return false;
  fcntl(Sock, F_SETFL, O_NONBLOCK);
  sockaddr_in Addr = {};
  Addr.sin_family = AF_INET;
  Addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  Addr.sin_port = htons(static_cast<uint16_t>(Port));
  connect(Sock, reinterpret_cast<sockaddr *>(&Addr), sizeof(Addr));
  fd_set WriteSet;
  FD_ZERO(&WriteSet);
  FD_SET(Sock, &WriteSet);
  timeval Timeout = {0, 200000};
  const bool bUp =
      select(Sock + 1, nullptr, &WriteSet, nullptr, &Timeout) > 0;
  close(Sock);
  return bUp;
#else
  return false;
#endif
}

/**
 * Loads config from defaults, disk, and environment in precedence order.
 * User Story: As runtime startup, I need one initialization entry point so all
 * getters observe the same resolved configuration values.
 */
inline void InitializeConfig();

/**
 * Ensures config has been loaded before a getter reads from storage.
 * User Story: As config consumers, I need getters to self-initialize so reads
 * work even if startup code did not initialize config explicitly.
 */
inline void EnsureInitialized() {
  !InitializedStorage() ? (InitializeConfig(), void()) : void();
}

/**
 * Forces config to reload from disk and environment.
 * User Story: As config mutation flows, I need a reload helper so persisted or
 * environment changes can be reflected without restarting the runtime.
 */
inline void ReloadConfig() {
  InitializedStorage() = false;
  InitializeConfig();
}

/**
 * Returns the active API base URL.
 * User Story: As networked SDK features, I need the resolved API URL so all
 * requests target the current environment.
 */
inline FString GetApiUrl() {
  EnsureInitialized();
  return ApiUrlStorage();
}

/**
 * Returns the active API key.
 * User Story: As authenticated SDK requests, I need the resolved API key so
 * outbound calls can include the current credential.
 */
inline FString GetApiKey() {
  EnsureInitialized();
  return ApiKeyStorage();
}

/**
 * Returns the configured local model path.
 * User Story: As local inference setup, I need the resolved model path so the
 * runtime can load the configured on-device model.
 */
inline FString GetModelPath() {
  EnsureInitialized();
  return ModelPathStorage();
}

/**
 * Returns the configured local database path.
 * User Story: As local memory storage, I need the resolved database path so
 * persistence code opens the intended datastore.
 */
inline FString GetDatabasePath() {
  EnsureInitialized();
  return DatabasePathStorage();
}

/**
 * Returns the configured vector dimension for local memory embeddings.
 * User Story: As embedding-backed recall flows, I need the resolved dimension
 * so vector operations use the correct width.
 */
inline int32 GetVectorDimension() {
  EnsureInitialized();
  return VectorDimensionStorage();
}

/**
 * Returns the configured default recall limit.
 * User Story: As memory recall flows, I need the resolved default limit so
 * callers can cap results consistently when no explicit limit is provided.
 */
inline int32 GetMaxRecallResults() {
  EnsureInitialized();
  return MaxRecallResultsStorage();
}

/**
 * Returns the SDK version string baked into the plugin build.
 * User Story: As diagnostics and tooling, I need the runtime SDK version so I
 * can report which plugin build is running.
 */
inline FString GetSdkVersion() { return TEXT("0.6.3"); }

/**
 * Updates the in-memory API URL and API key overrides.
 * User Story: As runtime override flows, I need to swap API connection values
 * in memory so subsequent requests use the new endpoint and credential.
 */
inline void SetApiConfig(const FString &ApiUrl, const FString &ApiKey) {
  EnsureInitialized();
  !ApiUrl.IsEmpty() ? (void)(ApiUrlStorage() = ApiUrl) : (void)0;
  ApiKeyStorage() = ApiKey;
}

/**
 * Overrides the config file path used for subsequent disk reads and writes.
 * User Story: As tests and tooling, I need to redirect config I/O so I can
 * isolate runs from the user's real config file.
 */
inline void SetConfigFilePathOverride(const FString &ConfigFilePath) {
  ConfigFilePathOverrideStorage() = ConfigFilePath;
}

/**
 * Resolves the home directory from environment variables with project fallback.
 * User Story: As config path helpers, I need a home directory resolver so
 * config file location can be determined across platforms.
 */
inline FString ResolveHomeDir() {
  const FString Home = FPlatformMisc::GetEnvironmentVariable(TEXT("HOME"));
  const FString Profile =
      FPlatformMisc::GetEnvironmentVariable(TEXT("USERPROFILE"));
  return !Home.IsEmpty()
      ? Home
      : (!Profile.IsEmpty() ? Profile : FPaths::ProjectDir());
}

/**
 * Resolves the config file path, honoring any explicit override first.
 * User Story: As config file helpers, I need the effective path so reads and
 * writes target the correct config location.
 */
inline FString GetConfigFilePath() {
  return !ConfigFilePathOverrideStorage().IsEmpty()
      ? ConfigFilePathOverrideStorage()
      : FPaths::Combine(ResolveHomeDir(), TEXT(".forbocai.json"));
}

/**
 * Loads the raw config JSON object from disk, or an empty object when absent.
 * User Story: As config parsing helpers, I need raw JSON config data so I can
 * populate in-memory settings from persisted values.
 */
inline TSharedPtr<FJsonObject> LoadConfigJsonObject() {
  const FString ConfigPath = GetConfigFilePath();
  FString JsonString;
  return !FFileHelper::LoadFileToString(JsonString, *ConfigPath)
      ? MakeShared<FJsonObject>()
      : [&JsonString]() {
          TSharedPtr<FJsonObject> JsonObject;
          const TSharedRef<TJsonReader<>> Reader =
              TJsonReaderFactory<>::Create(JsonString);
          return (FJsonSerializer::Deserialize(Reader, JsonObject) &&
                  JsonObject.IsValid())
              ? JsonObject
              : MakeShared<FJsonObject>();
        }();
}

/**
 * Persists a raw config JSON object to disk with restricted file permissions.
 * User Story: As config persistence flows, I need a safe write helper so
 * resolved settings can be saved with private file permissions when possible.
 */
inline bool WriteConfigJsonObject(const TSharedRef<FJsonObject> &JsonObject) {
  const FString ConfigPath = GetConfigFilePath();
  const FString Directory = FPaths::GetPath(ConfigPath);
  !Directory.IsEmpty()
      ? (IFileManager::Get().MakeDirectory(*Directory, true), void())
      : void();

  FString JsonString;
  const TSharedRef<TJsonWriter<>> Writer =
      TJsonWriterFactory<>::Create(&JsonString);
  FJsonSerializer::Serialize(JsonObject, Writer);

  const bool bSaved = FFileHelper::SaveStringToFile(JsonString, *ConfigPath);
#if PLATFORM_MAC || PLATFORM_UNIX
  bSaved ? (void)chmod(TCHAR_TO_UTF8(*ConfigPath), 0600) : (void)0;
#endif
  return bSaved;
}

/**
 * Applies supported FORBOCAI_* environment variables to in-memory config.
 * User Story: As deployment configuration, I need environment overrides to win
 * so runtime settings can be injected without editing files.
 */
inline void LoadFromEnvironment() {
  const FString U =
      FPlatformMisc::GetEnvironmentVariable(TEXT("FORBOCAI_API_URL"));
  const FString K =
      FPlatformMisc::GetEnvironmentVariable(TEXT("FORBOCAI_API_KEY"));
  const FString M =
      FPlatformMisc::GetEnvironmentVariable(TEXT("FORBOCAI_MODEL_PATH"));
  const FString D =
      FPlatformMisc::GetEnvironmentVariable(TEXT("FORBOCAI_DATABASE_PATH"));
  const FString V =
      FPlatformMisc::GetEnvironmentVariable(TEXT("FORBOCAI_VECTOR_DIMENSION"));
  const FString R =
      FPlatformMisc::GetEnvironmentVariable(TEXT("FORBOCAI_MAX_RECALL"));
  !U.IsEmpty() ? (void)(ApiUrlStorage() = U) : (void)0;
  !K.IsEmpty() ? (void)(ApiKeyStorage() = K) : (void)0;
  !M.IsEmpty() ? (void)(ModelPathStorage() = M) : (void)0;
  !D.IsEmpty() ? (void)(DatabasePathStorage() = D) : (void)0;
  !V.IsEmpty() ? (void)(VectorDimensionStorage() = FCString::Atoi(*V))
               : (void)0;
  !R.IsEmpty() ? (void)(MaxRecallResultsStorage() = FCString::Atoi(*R))
               : (void)0;
}

/**
 * Applies persisted JSON config values to in-memory config storage.
 * User Story: As local config loading, I need persisted settings applied so
 * runtime defaults can be overridden by the saved config file.
 */
inline void LoadFromConfigFile() {
  const TSharedPtr<FJsonObject> J = LoadConfigJsonObject();
  !J.IsValid()
      ? void()
      : [&J]() {
          FString S;
          (J->TryGetStringField(TEXT("apiUrl"), S) && !S.IsEmpty())
              ? (void)(ApiUrlStorage() = S) : (void)0;
          (J->TryGetStringField(TEXT("apiKey"), S) && !S.IsEmpty())
              ? (void)(ApiKeyStorage() = S) : (void)0;
          (J->TryGetStringField(TEXT("modelPath"), S) && !S.IsEmpty())
              ? (void)(ModelPathStorage() = S) : (void)0;
          (J->TryGetStringField(TEXT("databasePath"), S) && !S.IsEmpty())
              ? (void)(DatabasePathStorage() = S) : (void)0;
          int32 I = 0;
          J->TryGetNumberField(TEXT("vectorDimension"), I)
              ? (void)(VectorDimensionStorage() = I) : (void)0;
          J->TryGetNumberField(TEXT("maxRecallResults"), I)
              ? (void)(MaxRecallResultsStorage() = I) : (void)0;
        }();
}

/**
 * Saves the current in-memory configuration values back to disk.
 * User Story: As config editing flows, I need current settings persisted so
 * future runs start with the same resolved configuration.
 */
inline bool SaveToConfigFile() {
  EnsureInitialized();

  const TSharedRef<FJsonObject> J = MakeShared<FJsonObject>();
  J->SetStringField(TEXT("apiUrl"), ApiUrlStorage());
  !ApiKeyStorage().IsEmpty()
      ? J->SetStringField(TEXT("apiKey"), ApiKeyStorage()) : (void)0;
  !ModelPathStorage().IsEmpty()
      ? J->SetStringField(TEXT("modelPath"), ModelPathStorage()) : (void)0;
  !DatabasePathStorage().IsEmpty()
      ? J->SetStringField(TEXT("databasePath"), DatabasePathStorage())
      : (void)0;
  J->SetNumberField(TEXT("vectorDimension"), VectorDimensionStorage());
  J->SetNumberField(TEXT("maxRecallResults"), MaxRecallResultsStorage());

  return WriteConfigJsonObject(J);
}

/**
 * Writes a single supported config value to disk and reloads in-memory state.
 * User Story: As config editing tools, I need to update one key at a time so
 * CLI and editor flows can persist targeted changes simply.
 */
inline void SetConfigValue(const FString &Key, const FString &Value) {
  TSharedPtr<FJsonObject> JsonObject = LoadConfigJsonObject();
  JsonObject = JsonObject.IsValid() ? JsonObject : MakeShared<FJsonObject>();

  const bool bHandled = func::or_else(
      func::multi_match<FString, bool>(Key, {
          func::when<FString, bool>(
              func::equals<FString>(FString(TEXT("apiUrl"))),
              [&](const FString &) {
                JsonObject->SetStringField(TEXT("apiUrl"), Value);
                return true;
              }),
          func::when<FString, bool>(
              func::equals<FString>(FString(TEXT("apiKey"))),
              [&](const FString &) {
                JsonObject->SetStringField(TEXT("apiKey"), Value);
                return true;
              }),
          func::when<FString, bool>(
              func::equals<FString>(FString(TEXT("modelPath"))),
              [&](const FString &) {
                JsonObject->SetStringField(TEXT("modelPath"), Value);
                return true;
              }),
          func::when<FString, bool>(
              func::equals<FString>(FString(TEXT("databasePath"))),
              [&](const FString &) {
                JsonObject->SetStringField(TEXT("databasePath"), Value);
                return true;
              }),
          func::when<FString, bool>(
              func::equals<FString>(FString(TEXT("vectorDimension"))),
              [&](const FString &) {
                JsonObject->SetNumberField(TEXT("vectorDimension"),
                                           FCString::Atoi(*Value));
                return true;
              }),
          func::when<FString, bool>(
              func::equals<FString>(FString(TEXT("maxRecallResults"))),
              [&](const FString &) {
                JsonObject->SetNumberField(TEXT("maxRecallResults"),
                                           FCString::Atoi(*Value));
                return true;
              }),
      }),
      false);

  bHandled
      ? (WriteConfigJsonObject(JsonObject.ToSharedRef())
            ? (ReloadConfig(), void()) : void())
      : void();
}

/**
 * Reads a single supported config value from disk or derived version state.
 * User Story: As config inspection tools, I need a single-key lookup helper so
 * CLI commands can print one value without loading call-site parsing logic.
 */
inline FString GetConfigValue(const FString &Key) {
  return Key == TEXT("version")
      ? GetSdkVersion()
      : [&Key]() {
          const TSharedPtr<FJsonObject> J = LoadConfigJsonObject();
          return !J.IsValid()
              ? FString(TEXT(""))
              : func::or_else(
                    func::multi_match<FString, FString>(Key, {
                        func::when<FString, FString>(
                            func::equals<FString>(FString(TEXT("apiUrl"))),
                            [&J](const FString &) {
                              FString V;
                              return J->TryGetStringField(TEXT("apiUrl"), V)
                                  ? V : FString(TEXT(""));
                            }),
                        func::when<FString, FString>(
                            func::equals<FString>(FString(TEXT("apiKey"))),
                            [&J](const FString &) {
                              FString V;
                              return J->TryGetStringField(TEXT("apiKey"), V)
                                  ? V : FString(TEXT(""));
                            }),
                        func::when<FString, FString>(
                            func::equals<FString>(FString(TEXT("modelPath"))),
                            [&J](const FString &) {
                              FString V;
                              return J->TryGetStringField(TEXT("modelPath"), V)
                                  ? V : FString(TEXT(""));
                            }),
                        func::when<FString, FString>(
                            func::equals<FString>(
                                FString(TEXT("databasePath"))),
                            [&J](const FString &) {
                              FString V;
                              return J->TryGetStringField(
                                         TEXT("databasePath"), V)
                                  ? V : FString(TEXT(""));
                            }),
                        func::when<FString, FString>(
                            func::equals<FString>(
                                FString(TEXT("vectorDimension"))),
                            [&J](const FString &) {
                              int32 V = 0;
                              return J->TryGetNumberField(
                                         TEXT("vectorDimension"), V)
                                  ? FString::FromInt(V) : FString(TEXT(""));
                            }),
                        func::when<FString, FString>(
                            func::equals<FString>(
                                FString(TEXT("maxRecallResults"))),
                            [&J](const FString &) {
                              int32 V = 0;
                              return J->TryGetNumberField(
                                         TEXT("maxRecallResults"), V)
                                  ? FString::FromInt(V) : FString(TEXT(""));
                            }),
                    }),
                    FString(TEXT("")));
        }();
}

/**
 * Initializes config once using defaults, persisted JSON, then env overrides.
 * User Story: As runtime startup, I need deterministic config precedence so
 * defaults, file values, and environment overrides resolve predictably.
 */
inline void InitializeConfig() {
  InitializedStorage()
      ? void()
      : (ResetToDefaults(), LoadFromConfigFile(), LoadFromEnvironment(),
         ((ApiUrlStorage().Contains(TEXT("localhost")) ||
           ApiUrlStorage().Contains(TEXT("127.0.0.1"))) &&
                  !IsLocalHostReachable(ExtractLocalhostPort(ApiUrlStorage()))
              ? (void)(ApiUrlStorage() = PRODUCTION_API_URL)
              : void()),
         (void)(InitializedStorage() = true));
}

} // namespace SDKConfig
