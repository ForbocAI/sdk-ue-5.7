#include "RuntimeCommandlet.h"
#include "CLI/CLIModule.h"
#include "Core/functional_core.hpp"
#include "Misc/Parse.h"
#include "RuntimeConfig.h"
#include "TestGame/TestGameOrchestrator.h"

namespace {

/**
 * Normalizes legacy agent-prefixed commands to the canonical npc names.
 * User Story: As CLI compatibility, I need old command aliases mapped forward
 * so legacy scripts keep working with the renamed NPC command surface.
 */
FString NormalizeCommand(const FString &Command) {
  return func::or_else(
      func::multi_match<FString, FString>(
          Command,
          {func::when<FString, FString>(
               func::equals<FString>(TEXT("agent_list")),
               [](const FString &) { return FString(TEXT("npc_list")); }),
           func::when<FString, FString>(
               func::equals<FString>(TEXT("agent_create")),
               [](const FString &) { return FString(TEXT("npc_create")); }),
           func::when<FString, FString>(
               func::equals<FString>(TEXT("agent_process")),
               [](const FString &) { return FString(TEXT("npc_process")); }),
           func::when<FString, FString>(
               func::equals<FString>(TEXT("agent_active")),
               [](const FString &) { return FString(TEXT("npc_active")); }),
           func::when<FString, FString>(
               func::equals<FString>(TEXT("agent_state")),
               [](const FString &) { return FString(TEXT("npc_state")); }),
           func::when<FString, FString>(
               func::equals<FString>(TEXT("agent_update")),
               [](const FString &) { return FString(TEXT("npc_update")); }),
           func::when<FString, FString>(
               func::equals<FString>(TEXT("agent_import")),
               [](const FString &) { return FString(TEXT("npc_import")); }),
           func::when<FString, FString>(
               func::equals<FString>(TEXT("agent_chat")),
               [](const FString &) { return FString(TEXT("npc_chat")); })}),
      Command);
}

/**
 * Extracts a named param from UE command-line params string.
 * Returns empty string if not found.
 * User Story: As commandlet parsing, I need named parameter extraction so raw
 * Unreal params can be translated into structured command arguments.
 */
FString ExtractParam(const FString &Params, const TCHAR *ParamName) {
  FString Value;
  FParse::Value(*Params, ParamName, Value);
  return Value;
}

/**
 * Appends a param value to the args array if non-empty.
 * User Story: As commandlet parsing, I need optional args appended only when
 * present so handlers receive a clean positional argument list.
 */
void AddIfPresent(TArray<FString> &Args, const FString &Value) {
  !Value.IsEmpty() ? (void)Args.Add(Value) : (void)0;
}

/**
 * Appends a flag argument when the raw params include the named switch.
 * User Story: As commandlet parsing, I need boolean switches preserved so CLI
 * handlers can receive the same flag-style arguments as the direct CLI path.
 */
void AddFlagIfPresent(TArray<FString> &Args, const FString &Params,
                      const TCHAR *ParamName, const TCHAR *FlagValue) {
  FParse::Param(*Params, ParamName) ? (void)Args.Add(FlagValue) : (void)0;
}

/**
 * Extracts a named param and prepends a flag prefix when the value is present.
 * Returns empty string when the param is absent, so AddIfPresent will skip it.
 * User Story: As commandlet parsing, I need prefixed flag construction so raw
 * params translate into the --key=value format expected by CLI handlers.
 */
FString ExtractPrefixed(const FString &Params, const TCHAR *ParamName,
                        const TCHAR *Prefix) {
  const FString Raw = ExtractParam(Params, ParamName);
  return Raw.IsEmpty() ? FString(TEXT("")) : FString(Prefix) + Raw;
}

/**
 * Recursively appends extracted params to an args array.
 * User Story: As commandlet parsing, I need recursive param extraction so
 * multiple named params can be gathered without imperative loops.
 */
namespace commandlet_detail {
TArray<FString> buildParamsRecursive(const FString &Params,
                                     const TCHAR *const *Names, int32 Count,
                                     int32 Index, TArray<FString> Args) {
  return Index >= Count
             ? Args
             : (AddIfPresent(Args, ExtractParam(Params, Names[Index])),
                buildParamsRecursive(Params, Names, Count, Index + 1,
                                    MoveTemp(Args)));
}

/**
 * Recursively appends flag arguments when named switches are present.
 * Takes parallel arrays of param names and flag values.
 * User Story: As commandlet parsing, I need recursive flag extraction so
 * multiple boolean switches can be gathered without imperative loops.
 */
TArray<FString> buildFlagsRecursive(const FString &Params,
                                    const TCHAR *const *ParamNames,
                                    const TCHAR *const *FlagValues,
                                    int32 Count, int32 Index,
                                    TArray<FString> Args) {
  return Index >= Count
             ? Args
             : (AddFlagIfPresent(Args, Params, ParamNames[Index],
                                 FlagValues[Index]),
                buildFlagsRecursive(Params, ParamNames, FlagValues, Count,
                                   Index + 1, MoveTemp(Args)));
}

/**
 * Recursively appends prefixed params (--key=value) to an args array.
 * Takes parallel arrays of param names and prefixes.
 * User Story: As commandlet parsing, I need recursive prefixed extraction so
 * multiple --key=value flags can be gathered without imperative loops.
 */
TArray<FString> buildPrefixedRecursive(const FString &Params,
                                       const TCHAR *const *ParamNames,
                                       const TCHAR *const *Prefixes,
                                       int32 Count, int32 Index,
                                       TArray<FString> Args) {
  return Index >= Count
             ? Args
             : (AddIfPresent(Args,
                             ExtractPrefixed(Params, ParamNames[Index],
                                            Prefixes[Index])),
                buildPrefixedRecursive(Params, ParamNames, Prefixes, Count,
                                      Index + 1, MoveTemp(Args)));
}

/**
 * Recursively appends elements from a source array into a destination array.
 * User Story: As commandlet parsing, I need recursive array merging so
 * separately built param and flag lists can be combined declaratively.
 */
TArray<FString> mergeArraysRecursive(TArray<FString> Dest,
                                     const TArray<FString> &Src,
                                     int32 Index) {
  return Index >= Src.Num()
             ? Dest
             : (Dest.Add(Src[Index]),
                mergeArraysRecursive(MoveTemp(Dest), Src, Index + 1));
}
} // namespace commandlet_detail

/**
 * Extracts multiple named params and collects them into an args array.
 * User Story: As commandlet parsing, I need batch param extraction so commands
 * with several named params can build their args list declaratively.
 */
TArray<FString> BuildParams(const FString &Params,
                            std::initializer_list<const TCHAR *> Names) {
  return commandlet_detail::buildParamsRecursive(
      Params, Names.begin(), static_cast<int32>(Names.size()), 0,
      TArray<FString>());
}

/**
 * Extracts multiple boolean switches and collects them as flag args.
 * User Story: As commandlet parsing, I need batch flag extraction so commands
 * with several switches can build their args list declaratively.
 */
TArray<FString> BuildFlags(const FString &Params,
                           std::initializer_list<const TCHAR *> ParamNames,
                           std::initializer_list<const TCHAR *> FlagValues) {
  return commandlet_detail::buildFlagsRecursive(
      Params, ParamNames.begin(), FlagValues.begin(),
      static_cast<int32>(ParamNames.size()), 0, TArray<FString>());
}

/**
 * Extracts multiple named params with flag prefixes and collects them.
 * User Story: As commandlet parsing, I need batch prefixed extraction so
 * commands with --key=value flags can build their args list declaratively.
 */
TArray<FString> BuildPrefixed(const FString &Params,
                              std::initializer_list<const TCHAR *> ParamNames,
                              std::initializer_list<const TCHAR *> Prefixes) {
  return commandlet_detail::buildPrefixedRecursive(
      Params, ParamNames.begin(), Prefixes.begin(),
      static_cast<int32>(ParamNames.size()), 0, TArray<FString>());
}

/**
 * Merges two args arrays into one via recursive append.
 * User Story: As commandlet parsing, I need array merging so separately built
 * param and flag lists can be combined into one args array declaratively.
 */
TArray<FString> MergeArgs(TArray<FString> Base, const TArray<FString> &Extra) {
  return commandlet_detail::mergeArraysRecursive(MoveTemp(Base), Extra, 0);
}

/**
 * Builds positional command arguments from raw Unreal commandlet params.
 * User Story: As command dispatch, I need raw commandlet params converted into
 * handler-friendly args so CLI routing can reuse the shared command handlers.
 */
TArray<FString> BuildCommandArgs(const FString &Command,
                                 const FString &Params) {
  return func::or_else(
      func::multi_match<FString, TArray<FString>>(
          Command,
          {
              /**
               * ---- NPC ----
               * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
               */
              func::when<FString, TArray<FString>>(
                  func::equals<FString>(TEXT("npc_create")),
                  [&Params](const FString &) {
                    TArray<FString> A;
                    A.Add(ExtractParam(Params, TEXT("Persona=")));
                    return A;
                  }),
              func::when<FString, TArray<FString>>(
                  func::equals<FString>(TEXT("npc_process")),
                  [&Params](const FString &) {
                    return BuildParams(Params, {TEXT("Id="), TEXT("Input=")});
                  }),
              func::when<FString, TArray<FString>>(
                  func::equals<FString>(TEXT("npc_update")),
                  [&Params](const FString &) {
                    return BuildParams(
                        Params,
                        {TEXT("Id="), TEXT("Mood="), TEXT("Inventory=")});
                  }),
              func::when<FString, TArray<FString>>(
                  func::equals<FString>(TEXT("npc_import")),
                  [&Params](const FString &) {
                    return BuildParams(Params, {TEXT("TxId=")});
                  }),
              func::when<FString, TArray<FString>>(
                  func::equals<FString>(TEXT("npc_chat")),
                  [&Params](const FString &) {
                    return BuildParams(Params,
                                      {TEXT("Id="), TEXT("Message=")});
                  }),

              /**
               * ---- Memory (remote) ----
               * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
               */
              func::when<FString, TArray<FString>>(
                  [](const FString &C) {
                    return C == TEXT("memory_list") ||
                           C == TEXT("memory_clear") ||
                           C == TEXT("memory_export");
                  },
                  [&Params](const FString &) {
                    return BuildParams(Params, {TEXT("Id=")});
                  }),
              func::when<FString, TArray<FString>>(
                  func::equals<FString>(TEXT("memory_recall")),
                  [&Params](const FString &) {
                    return BuildParams(Params,
                                      {TEXT("Id="), TEXT("Query=")});
                  }),
              func::when<FString, TArray<FString>>(
                  func::equals<FString>(TEXT("memory_store")),
                  [&Params](const FString &) {
                    return BuildParams(Params, {TEXT("Id="), TEXT("Obs=")});
                  }),

              /**
                                      {TEXT("Id="), TEXT("Prompt=")});
                  }),

              /**
               * ---- Ghost ----
               * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
               */
              func::when<FString, TArray<FString>>(
                  func::equals<FString>(TEXT("ghost_run")),
                  [&Params](const FString &) {
                    return BuildParams(Params,
                                      {TEXT("Suite="), TEXT("Duration=")});
                  }),
              func::when<FString, TArray<FString>>(
                  [](const FString &C) {
                    return C == TEXT("ghost_status") ||
                           C == TEXT("ghost_results") ||
                           C == TEXT("ghost_stop");
                  },
                  [&Params](const FString &) {
                    return BuildParams(Params, {TEXT("SessionId=")});
                  }),
              func::when<FString, TArray<FString>>(
                  func::equals<FString>(TEXT("ghost_history")),
                  [&Params](const FString &) {
                    return BuildParams(Params, {TEXT("Limit=")});
                  }),

              /**
               * ---- Bridge ----
               * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
               */
              func::when<FString, TArray<FString>>(
                  func::equals<FString>(TEXT("bridge_validate")),
                  [&Params](const FString &) {
                    return BuildParams(Params, {TEXT("Action=")});
                  }),
              func::when<FString, TArray<FString>>(
                  func::equals<FString>(TEXT("bridge_preset")),
                  [&Params](const FString &) {
                    return BuildParams(Params, {TEXT("Name=")});
                  }),

              /**
               * ---- Rules ----
               * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
               */
              func::when<FString, TArray<FString>>(
                  func::equals<FString>(TEXT("rules_register")),
                  [&Params](const FString &) {
                    return BuildParams(Params, {TEXT("Json=")});
                  }),
              func::when<FString, TArray<FString>>(
                  func::equals<FString>(TEXT("rules_delete")),
                  [&Params](const FString &) {
                    return BuildParams(Params, {TEXT("Id=")});
                  }),

              /**
               * ---- Soul ----
               * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
               */
              func::when<FString, TArray<FString>>(
                  func::equals<FString>(TEXT("soul_export")),
                  [&Params](const FString &) {
                    return BuildParams(Params, {TEXT("Id=")});
                  }),
              func::when<FString, TArray<FString>>(
                  [](const FString &C) {
                    return C == TEXT("soul_import") ||
                           C == TEXT("soul_import_npc") ||
                           C == TEXT("soul_verify");
                  },
                  [&Params](const FString &) {
                    return BuildParams(Params, {TEXT("TxId=")});
                  }),
              func::when<FString, TArray<FString>>(
                  func::equals<FString>(TEXT("soul_list")),
                  [&Params](const FString &) {
                    return BuildParams(Params, {TEXT("Limit=")});
                  }),

              /**
               * ---- Config ----
               * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
               */
              func::when<FString, TArray<FString>>(
                  func::equals<FString>(TEXT("config_set")),
                  [&Params](const FString &) {
                    return BuildParams(Params,
                                      {TEXT("Key="), TEXT("Value=")});
                  }),
              func::when<FString, TArray<FString>>(
                  func::equals<FString>(TEXT("config_get")),
                  [&Params](const FString &) {
                    return BuildParams(Params, {TEXT("Key=")});
                  }),

              /**
               * ---- Test Game ----
               * User Story: As test-game CLI entrypoints, I need one optional
               * mode arg so the harness can run through the same commandlet
               * validation surface as the rest of the CLI.
               */
              func::when<FString, TArray<FString>>(
                  func::equals<FString>(TEXT("test_game")),
                  [&Params](const FString &) {
                    return BuildParams(Params, {TEXT("Mode=")});
                  }),

              /**
               * ---- Setup ----
               * User Story: As setup command routing, I need commandlet params converted
               * into setup flags so the commandlet path matches the direct CLI surface.
               */
              func::when<FString, TArray<FString>>(
                  func::equals<FString>(TEXT("setup_build_llama")),
                  [&Params](const FString &) {
                    return BuildPrefixed(
                        Params,
                        {TEXT("Tag="), TEXT("MacOSDeploymentTarget=")},
                        {TEXT("--tag="),
                         TEXT("--macos-deployment-target=")});
                  }),
              func::when<FString, TArray<FString>>(
                  [](const FString &C) {
                    return C == TEXT("setup") || C == TEXT("setup_deps");
                  },
                  [&Params](const FString &) {
                    return BuildFlags(
                        Params,
                        {TEXT("SqliteOnly"), TEXT("LlamaOnly")},
                        {TEXT("--sqlite-only"), TEXT("--llama-only")});
                  }),
              func::when<FString, TArray<FString>>(
                  func::equals<FString>(TEXT("setup_runtime_check")),
                  [&Params](const FString &) {
                    return MergeArgs(
                        BuildPrefixed(
                            Params,
                            {TEXT("Model="), TEXT("EmbeddingModel="),
                             TEXT("Database="), TEXT("Prompt=")},
                            {TEXT("--model="), TEXT("--embedding-model="),
                             TEXT("--database="), TEXT("--prompt=")}),
                        BuildFlags(
                            Params,
                            {TEXT("AllowDownload"), TEXT("SkipCortex"),
                             TEXT("SkipVector"), TEXT("SkipMemory"),
                             TEXT("Cleanup")},
                            {TEXT("--allow-download"), TEXT("--skip-cortex"),
                             TEXT("--skip-vector"), TEXT("--skip-memory"),
                             TEXT("--cleanup")}));
                  }),
          }),
      TArray<FString>());
}

func::Maybe<TestGame::EPlayMode>
ParseTestGameMode(const TArray<FString> &Args) {
  return Args.Num() == 0
             ? func::just(TestGame::EPlayMode::Autoplay)
             : Args[0].Equals(TEXT("autoplay"), ESearchCase::IgnoreCase)
                   ? func::just(TestGame::EPlayMode::Autoplay)
                   : Args[0].Equals(TEXT("manual"), ESearchCase::IgnoreCase)
                         ? func::just(TestGame::EPlayMode::Manual)
                         : func::nothing<TestGame::EPlayMode>();
}

} // namespace

UForbocAICommandlet::UForbocAICommandlet() {
  IsClient = false;
  IsEditor = false;
  IsServer = false;
  LogToConsole = true;
}

/**
 * Runs the commandlet entrypoint for the requested CLI command.
 * User Story: As Unreal CLI execution, I need one main entrypoint so command
 * parameters can be validated, normalized, and dispatched consistently.
 */
int32 UForbocAICommandlet::Main(const FString &Params) {
  SDKConfig::InitializeConfig();

  FString Command;
  FParse::Value(*Params, TEXT("Command="), Command);
  Command = NormalizeCommand(Command);

  FString ApiUrl;
  FParse::Value(*Params, TEXT("ApiUrl="), ApiUrl);

  FString ApiKey;
  FParse::Value(*Params, TEXT("ApiKey="), ApiKey);

  (!ApiUrl.IsEmpty() || !ApiKey.IsEmpty())
      ? (SDKConfig::SetApiConfig(
             ApiUrl.IsEmpty() ? SDKConfig::GetApiUrl() : ApiUrl,
             ApiKey.IsEmpty() ? SDKConfig::GetApiKey() : ApiKey),
         void())
      : void();

  UE_LOG(LogTemp, Display, TEXT("ForbocAI CLI (UE5) - Command: %s"),
         *Command);

  const TArray<FString> Args = BuildCommandArgs(Command, Params);
  bool bCommandFailed = false;
  FString CommandError;
  createCommandPipeline(Command, Args)
      .then([]() {
        UE_LOG(LogTemp, Display, TEXT(""));
        UE_LOG(LogTemp, Display,
               TEXT("[RESULT] Command completed successfully"));
      })
      .catch_([&bCommandFailed, &CommandError](std::string Message) {
        bCommandFailed = true;
        CommandError = UTF8_TO_TCHAR(Message.c_str());
        UE_LOG(LogTemp, Error, TEXT(""));
        UE_LOG(LogTemp, Error, TEXT("[RESULT] Command failed: %s"),
               *CommandError);
      })
      .execute();
  return bCommandFailed ? 1 : 0;
}

UForbocAICommandlet::CommandResult
UForbocAICommandlet::executeCommand(const FString &Command,
                                    const TArray<FString> &Args) {
  return CLIOps::DispatchCommand(Command, Args);
}

UForbocAICommandlet::CommandExecution
UForbocAICommandlet::createCommandPipeline(const FString &Command,
                                           const TArray<FString> &Args) {
  return createCommandPipeline(Command, Args,
                               TestGame::CommandSurface::FCommandExecutor());
}

UForbocAICommandlet::CommandExecution
UForbocAICommandlet::createCommandPipeline(
    const FString &Command, const TArray<FString> &Args,
    const TestGame::CommandSurface::FCommandExecutor &TestGameExecutor) {
  return UForbocAICommandlet::CommandExecution::create(
      [this, Command, Args,
       TestGameExecutor](std::function<void()> Resolve,
                         std::function<void(std::string)> Reject) {
        auto RejectFString = [&Reject](const FString &Error) {
          Reject(TCHAR_TO_UTF8(*Error));
        };
        auto RejectStd = [&Reject](const std::string &Error) {
          Reject(Error);
        };
        (void)RejectStd; // Available for std::string call sites

        const auto Validation =
            func::runValidation(commandValidationPipeline(), Command);
        func::ematch(
            Validation,
            [&RejectFString](const FString &Err) { RejectFString(Err); },
            [this, &Command, &Args, &Resolve,
             &RejectFString, &Reject,
             &TestGameExecutor](const FString &) {
              if (Command == TEXT("test_game")) {
                const func::Maybe<TestGame::EPlayMode> Mode =
                    ParseTestGameMode(Args);
                if (!Mode.hasValue) {
                  RejectFString(
                      TEXT("Invalid test_game mode. Use manual or autoplay."));
                  return;
                }

                const TestGame::FGameRunResult Result =
                    TestGame::RunGame(Mode.value, TestGameExecutor);
                Result.bComplete ? Resolve()
                                 : RejectFString(Result.Summary);
                return;
              }

              const CommandResult Result = executeCommand(Command, Args);
              const FString ResultMessage =
                  Result.message.empty()
                      ? FString()
                      : FString(UTF8_TO_TCHAR(Result.message.c_str()));
              Result.isSuccessful()
                  ? (Resolve(), void())
                  : (RejectFString(
                         ResultMessage.IsEmpty()
                             ? FString::Printf(TEXT("Command failed: %s"),
                                               *Command)
                             : ResultMessage),
                     void());
            });
      });
}

CLITypes::ValidationPipeline<FString, FString>
UForbocAICommandlet::commandValidationPipeline() {
  return func::validationPipeline<FString, FString>() |
         [](const FString &Command) -> CLITypes::Either<FString, FString> {
           return Command.IsEmpty()
                      ? CLITypes::make_left(
                            FString(TEXT("Command cannot be empty")), FString())
                      : CLITypes::make_right(FString(), Command);
         } |
         [](const FString &Command) -> CLITypes::Either<FString, FString> {
        static const TArray<FString> ValidCommands = {
            TEXT("version"),          TEXT("status"),
            TEXT("doctor"),           TEXT("system_status"),
            TEXT("npc_list"),         TEXT("npc_create"),
            TEXT("npc_process"),      TEXT("npc_active"),
            TEXT("npc_state"),        TEXT("npc_update"),
            TEXT("npc_import"),       TEXT("npc_chat"),
            TEXT("memory_list"),      TEXT("memory_recall"),
            TEXT("memory_store"),     TEXT("memory_clear"),
            TEXT("memory_export"),
            
            
            TEXT("ghost_run"),        TEXT("ghost_status"),
            TEXT("ghost_results"),    TEXT("ghost_stop"),
            TEXT("ghost_history"),
            TEXT("bridge_validate"),  TEXT("bridge_rules"),
            TEXT("bridge_preset"),
            TEXT("rules_list"),       TEXT("rules_presets"),
            TEXT("rules_register"),   TEXT("rules_delete"),
            TEXT("soul_export"),      TEXT("soul_import"),
            TEXT("soul_import_npc"),  TEXT("soul_list"),
            TEXT("soul_verify"),
            TEXT("config_set"),       TEXT("config_get"),
            TEXT("config_list"),
            TEXT("vector_init"),
            TEXT("setup"),            TEXT("setup_deps"),
            TEXT("setup_check"),      TEXT("setup_verify"),
            TEXT("setup_build_llama"), TEXT("setup_runtime_check"),
            TEXT("test_game")};
           return !ValidCommands.Contains(Command)
                      ? CLITypes::make_left(
                            FString::Printf(TEXT("Invalid command: %s"),
                                             *Command),
                            FString())
                      : CLITypes::make_right(FString(), Command);
         };
}
