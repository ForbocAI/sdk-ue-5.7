#include "CLI/CliHandlers.h"
#include "CLI/CliOperations.h"
#include "Core/ThunkDetail.h"
#include "NativeEngine.h"
#include "RuntimeConfig.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/Guid.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace CLIOps {
namespace Handlers {

namespace {

/**
 * Version constants (single source of truth for ThirdParty setup)
 * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
 */
static const TCHAR *SQLITE_VERSION = TEXT("3460100");
static const TCHAR *SQLITE_VEC_VERSION = TEXT("v0.1.6");
static const TCHAR *LLAMA_CPP_TAG = TEXT("b8420");
static const TCHAR *MACOS_DEPLOYMENT_TARGET = TEXT("14.0");
static const TCHAR *DEFAULT_CORTEX_MODEL_FILE =
    TEXT("SmolLM2-135M-Instruct-Q4_K_M.gguf");
static const TCHAR *DEFAULT_SMOKE_PROMPT = TEXT("Reply with the word ready.");

struct FRuntimeCheckOptions {
  bool bAllowDownload;
  bool bSkipCortex;
  bool bSkipVector;
  bool bSkipMemory;
  bool bCleanup;
  FString CortexModel;
  FString EmbeddingModel;
  FString DatabasePath;
  FString Prompt;
};

bool HasFlagRecursive(const TArray<FString> &Args, const FString &Flag,
                      int32 Index = 0) {
  return Index == Args.Num()
             ? false
             : Args[Index] == Flag ? true
                                   : HasFlagRecursive(Args, Flag, Index + 1);
}

FString FindOptionRecursive(const TArray<FString> &Args, const FString &Prefix,
                            int32 Index = 0) {
  return Index == Args.Num()
             ? TEXT("")
             : Args[Index].StartsWith(Prefix)
                   ? Args[Index].Mid(Prefix.Len())
                   : FindOptionRecursive(Args, Prefix, Index + 1);
}

FRuntimeCheckOptions RuntimeCheckOptions(const TArray<FString> &Args) {
  FRuntimeCheckOptions Options;
  Options.bAllowDownload = HasFlagRecursive(Args, TEXT("--allow-download"));
  Options.bSkipCortex = HasFlagRecursive(Args, TEXT("--skip-cortex"));
  Options.bSkipVector = HasFlagRecursive(Args, TEXT("--skip-vector"));
  Options.bSkipMemory = HasFlagRecursive(Args, TEXT("--skip-memory"));
  Options.bCleanup = HasFlagRecursive(Args, TEXT("--cleanup"));
  Options.CortexModel = FindOptionRecursive(Args, TEXT("--model="));
  Options.EmbeddingModel =
      FindOptionRecursive(Args, TEXT("--embedding-model="));
  Options.DatabasePath = FindOptionRecursive(Args, TEXT("--database="));
  Options.Prompt = FindOptionRecursive(Args, TEXT("--prompt="));
  return Options;
}

FString DefaultCortexModelPath() {
  return rtk::detail::GetLocalInfrastructureDir() + TEXT("models/") +
         DEFAULT_CORTEX_MODEL_FILE;
}

FString RuntimeSmokeDatabasePath() {
  return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("ForbocAI"),
                         TEXT("runtime-smoke"),
                         FString::Printf(TEXT("runtime-smoke-%s.db"),
                                         *FGuid::NewGuid().ToString(
                                             EGuidFormats::Digits)));
}

FString ResolveEmbeddingModelPath(const FRuntimeCheckOptions &Options,
                                  IPlatformFile &PF) {
  const FString DefaultPath = rtk::detail::DefaultEmbeddingModelPath();
  return !Options.EmbeddingModel.IsEmpty()
             ? Options.EmbeddingModel
             : PF.FileExists(*DefaultPath) ? DefaultPath
                                           : Options.bAllowDownload
                                                 ? FString()
                                                 : TEXT("");
}

FString ResolveCortexModelArg(const FRuntimeCheckOptions &Options,
                              IPlatformFile &PF) {
  const FString DefaultPath = DefaultCortexModelPath();
  return !Options.CortexModel.IsEmpty()
             ? Options.CortexModel
             : PF.FileExists(*DefaultPath) ? DefaultPath
                                           : Options.bAllowDownload
                                                 ? FString()
                                                 : TEXT("");
}

bool ContainsRecalledTextRecursive(const TArray<FMemoryItem> &Items,
                                   const FString &ExpectedText, int32 Index) {
  return Index == Items.Num()
             ? false
             : Items[Index].Text == ExpectedText
                   ? true
                   : ContainsRecalledTextRecursive(Items, ExpectedText,
                                                   Index + 1);
}

void EnsureParentDirectory(const FString &Path) {
  FPlatformFileManager::Get().GetPlatformFile().CreateDirectoryTree(
      *FPaths::GetPath(Path));
}

void CleanupSmokeDatabase(rtk::EnhancedStore<FStoreState> &Store,
                          const FString &DatabasePath) {
  try {
    Ops::ClearNodeMemory(Store);
  } catch (const std::exception &) {
  }
  IFileManager::Get().Delete(*DatabasePath, false, true, true);
}

void CleanupSmokeDatabaseIfNeeded(rtk::EnhancedStore<FStoreState> &Store,
                                  const FString &DatabasePath,
                                  bool bShouldCleanup) {
  bShouldCleanup
      ? [&]() { CleanupSmokeDatabase(Store, DatabasePath); }()
      : (void)0;
}

/**
 * Finds the first subdirectory matching a prefix inside a directory.
 * User Story: As dependency setup, I need directory discovery so extracted
 * archives can be relocated without hard-coding versioned folder names.
 */
FString FindSubdirWithPrefix(const FString &Dir, const FString &Prefix) {
  TArray<FString> Dirs;
  IFileManager::Get().FindFiles(Dirs, *(Dir / TEXT("*")), false, true);

  struct FindHelper {
    static FString apply(const TArray<FString> &Arr, const FString &Dir,
                         const FString &Prefix, int32 Idx) {
      return Idx >= Arr.Num()
                 ? TEXT("")
                 : Arr[Idx].StartsWith(Prefix)
                       ? Dir / Arr[Idx]
                       : apply(Arr, Dir, Prefix, Idx + 1);
    }
  };

  return FindHelper::apply(Dirs, Dir, Prefix, 0);
}

Result RunRuntimeSmokeCheck(rtk::EnhancedStore<FStoreState> &Store,
                            const TArray<FString> &Args) {
  IPlatformFile &PF = FPlatformFileManager::Get().GetPlatformFile();
  const FRuntimeCheckOptions Options = RuntimeCheckOptions(Args);
  const FString DatabasePath =
      Options.DatabasePath.IsEmpty() ? RuntimeSmokeDatabasePath()
                                     : FPaths::ConvertRelativePathToFull(
                                           Options.DatabasePath);
  const bool bOwnsDatabasePath = Options.DatabasePath.IsEmpty();
  const FString EmbeddingModelPath =
      ResolveEmbeddingModelPath(Options, PF);
  const FString CortexModelArg = ResolveCortexModelArg(Options, PF);
  const FString Prompt =
      Options.Prompt.IsEmpty() ? FString(DEFAULT_SMOKE_PROMPT) : Options.Prompt;
  const FString SmokeText =
      FString::Printf(TEXT("runtime-smoke-%s"),
                      *FGuid::NewGuid().ToString(EGuidFormats::Digits));
  const bool bNeedsVectorModel =
      !Options.bSkipVector || !Options.bSkipMemory;

  EnsureParentDirectory(DatabasePath);

  UE_LOG(LogTemp, Display, TEXT(""));
  UE_LOG(LogTemp, Display, TEXT("=== ForbocAI Native Runtime Smoke Check ==="));
  UE_LOG(LogTemp, Display, TEXT("  Database: %s"), *DatabasePath);
  UE_LOG(LogTemp, Display, TEXT("  Cortex: %s"),
         Options.bSkipCortex ? TEXT("skipped") : TEXT("enabled"));
  UE_LOG(LogTemp, Display, TEXT("  Vector: %s"),
         Options.bSkipVector ? TEXT("skipped") : TEXT("enabled"));
  UE_LOG(LogTemp, Display, TEXT("  Memory: %s"),
         Options.bSkipMemory ? TEXT("skipped") : TEXT("enabled"));

  /* 5 early-return guards as nested ternary chain */
  return !WITH_FORBOC_SQLITE_VEC
    ? Result::Failure(
        "setup_runtime_check requires WITH_FORBOC_SQLITE_VEC=1")
    : (bNeedsVectorModel && !WITH_FORBOC_NATIVE)
    ? Result::Failure(
        "setup_runtime_check requires WITH_FORBOC_NATIVE=1 for vector or memory verification")
    : (Options.bSkipVector && !Options.bSkipMemory)
    ? Result::Failure(
        "setup_runtime_check cannot verify memory storage while --skip-vector is set")
    : (bNeedsVectorModel && EmbeddingModelPath.IsEmpty())
    ? Result::Failure(
        "Embedding model missing. Re-run with --allow-download, --embedding-model=<path>, or --skip-vector --skip-memory.")
    : (!Options.bSkipCortex && CortexModelArg.IsEmpty())
    ? Result::Failure(
        "Cortex model missing. Re-run with --allow-download, --model=<path>, or --skip-cortex.")
    : [&]() -> Result {
      try {
        Ops::InitNodeMemory(Store, DatabasePath);
        return !PF.FileExists(*DatabasePath)
          ? Result::Failure("Node memory database was not created on disk")
          : [&]() -> Result {
            UE_LOG(LogTemp, Display, TEXT("  [OK] node memory initialized"));

            /* Vector init block */
            const Result VectorResult = !Options.bSkipVector
              ? [&]() -> Result {
                  Ops::WaitForResult(
                      Store.dispatch(rtk::initNodeVectorThunk(EmbeddingModelPath)), 180.0);
                  return !Store.getState().Cortex.bEmbedderReady
                    ? Result::Failure("Embedding model did not become ready")
                    : [&]() -> Result {
                        UE_LOG(LogTemp, Display, TEXT("  [OK] embedding runtime initialized"));
                        return Result::Success("");
                      }();
                }()
              : Result::Success("");

            return !VectorResult.bSuccess
              ? VectorResult
              : [&]() -> Result {
                /* Memory block */
                const Result MemoryResult = !Options.bSkipMemory
                  ? [&]() -> Result {
                      Ops::StoreNodeMemory(Store, SmokeText, 0.95f);
                      const TArray<FMemoryItem> Recalled =
                          Ops::RecallNodeMemory(Store, SmokeText, 5, 0.0f);
                      return !ContainsRecalledTextRecursive(Recalled, SmokeText, 0)
                        ? Result::Failure(
                            "Stored smoke memory was not recalled from the local vector store")
                        : [&]() -> Result {
                            UE_LOG(LogTemp, Display, TEXT("  [OK] memory store/recall verified"));
                            return Result::Success("");
                          }();
                    }()
                  : Result::Success("");

                return !MemoryResult.bSuccess
                  ? MemoryResult
                  : [&]() -> Result {
                    /* Cortex block */
                    return !Options.bSkipCortex
                      ? [&]() -> Result {
                          const FString ModelsDir = rtk::detail::GetLocalInfrastructureDir() +
                                                    TEXT("models");
                          TArray<FString> ModelsBefore;
                          TArray<FString> ModelsAfter;
                          PF.FindFiles(ModelsBefore, *ModelsDir, TEXT(".gguf"));
                          const FCortexStatus Status = Ops::InitCortex(Store, CortexModelArg);
                          PF.FindFiles(ModelsAfter, *ModelsDir, TEXT(".gguf"));
                          return !Status.bReady
                            ? Result::Failure("Local cortex did not become ready")
                            : (Options.CortexModel.IsEmpty() && ModelsAfter.Num() == 0)
                            ? Result::Failure(
                                "Local cortex initialized without a GGUF artifact in local_infrastructure/models")
                            : [&]() -> Result {
                                const FCortexResponse Response = Ops::CompleteCortex(Store, Prompt);
                                return (Response.Text.IsEmpty() || Response.Text.StartsWith(TEXT("Error:")))
                                  ? Result::Failure("Local cortex completion returned an error")
                                  : [&]() -> Result {
                                      UE_LOG(LogTemp, Display,
                                             TEXT("  [OK] cortex initialized and generated a completion"));
                                      UE_LOG(LogTemp, Display, TEXT("  Models before: %d | after: %d"),
                                             ModelsBefore.Num(), ModelsAfter.Num());
                                      return Result::Success("");
                                    }();
                              }();
                        }()
                      : Result::Success("");
                  }();
              }();
          }();
      } catch (const std::exception &Error) {
        CleanupSmokeDatabaseIfNeeded(Store, DatabasePath,
                                     bOwnsDatabasePath || Options.bCleanup);
        return Result::Failure(Error.what());
      }

      CleanupSmokeDatabaseIfNeeded(Store, DatabasePath,
                                   bOwnsDatabasePath || Options.bCleanup);
      return Result::Success("Native runtime smoke check passed");
    }();
}

/**
 * Runs an external process synchronously.
 * User Story: As setup automation, I need process execution wrapped so build
 * and extraction commands can run with shared logging and timeout handling.
 */
int32 RunProcess(const FString &Executable, const FString &Args,
                 const FString &WorkingDir = TEXT(""),
                 float TimeoutSeconds = 300.0f) {
  FString StdOut;
  FString StdErr;
  int32 ReturnCode = -1;

  const FString EffectiveDir =
      WorkingDir.IsEmpty() ? FPaths::ProjectDir() : WorkingDir;

  UE_LOG(LogTemp, Display, TEXT("  [exec] %s %s"), *Executable, *Args);

  void *ReadPipe = nullptr;
  void *WritePipe = nullptr;
  FPlatformProcess::CreatePipe(ReadPipe, WritePipe);

  FProcHandle Proc = FPlatformProcess::CreateProc(
      *Executable, *Args, false, true, true, nullptr, 0, *EffectiveDir,
      WritePipe, ReadPipe);

  return !Proc.IsValid()
    ? [&]() -> int32 {
        UE_LOG(LogTemp, Warning, TEXT("  [FAIL] Could not launch %s"),
               *Executable);
        FPlatformProcess::ClosePipe(ReadPipe, WritePipe);
        return -1;
      }()
    : [&]() -> int32 {
        /**
         * Wait with timeout — recursive helper replaces while loop
         * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
         */
        struct WaitHelper {
          static int32 poll(FProcHandle &Proc, void *ReadPipe, void *WritePipe,
                            FString &StdOut, float TimeoutSeconds,
                            double StartTime) {
            return !FPlatformProcess::IsProcRunning(Proc)
              ? 0
              : [&]() -> int32 {
                  StdOut += FPlatformProcess::ReadPipe(ReadPipe);
                  return (FPlatformTime::Seconds() - StartTime > TimeoutSeconds)
                    ? [&]() -> int32 {
                        FPlatformProcess::TerminateProc(Proc);
                        UE_LOG(LogTemp, Warning, TEXT("  [FAIL] Process timed out after %.0fs"),
                               TimeoutSeconds);
                        FPlatformProcess::ClosePipe(ReadPipe, WritePipe);
                        return -1;
                      }()
                    : [&]() -> int32 {
                        FPlatformProcess::Sleep(0.1f);
                        return poll(Proc, ReadPipe, WritePipe, StdOut,
                                    TimeoutSeconds, StartTime);
                      }();
                }();
          }
        };

        const double StartTime = FPlatformTime::Seconds();
        const int32 PollResult = WaitHelper::poll(
            Proc, ReadPipe, WritePipe, StdOut, TimeoutSeconds, StartTime);

        return PollResult == -1
          ? PollResult
          : [&]() -> int32 {
              StdOut += FPlatformProcess::ReadPipe(ReadPipe);
              FPlatformProcess::GetProcReturnCode(Proc, &ReturnCode);
              FPlatformProcess::ClosePipe(ReadPipe, WritePipe);

              (ReturnCode != 0 && !StdOut.IsEmpty())
                ? [&]() { UE_LOG(LogTemp, Warning, TEXT("  %s"), *StdOut); }()
                : (void)0;

              return ReturnCode;
            }();
      }();
}

/**
 * Extracts a zip file to a destination directory.
 * User Story: As dependency setup, I need zip extraction so downloaded archives
 * can be unpacked into the ThirdParty layout automatically.
 */
bool ExtractZip(const FString &ZipPath, const FString &DestDir) {
  IPlatformFile &PF = FPlatformFileManager::Get().GetPlatformFile();
  PF.CreateDirectoryTree(*DestDir);

#if PLATFORM_MAC || PLATFORM_LINUX
  return RunProcess(TEXT("/usr/bin/unzip"), FString::Printf(
      TEXT("-qo \"%s\" -d \"%s\""), *ZipPath, *DestDir)) == 0;
#elif PLATFORM_WINDOWS
  /**
   * PowerShell Expand-Archive
   * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
   */
  return RunProcess(TEXT("powershell.exe"), FString::Printf(
      TEXT("-NoProfile -Command \"Expand-Archive -Force -Path '%s' -DestinationPath '%s'\""),
      *ZipPath, *DestDir)) == 0;
#else
  UE_LOG(LogTemp, Warning, TEXT("  [FAIL] Zip extraction not supported on this platform"));
  return false;
#endif
}

/**
 * Extracts a tar.gz file to a destination directory.
 * User Story: As dependency setup, I need tarball extraction so downloaded
 * source archives can be unpacked into the expected build directories.
 */
bool ExtractTarGz(const FString &TarPath, const FString &DestDir) {
  IPlatformFile &PF = FPlatformFileManager::Get().GetPlatformFile();
  PF.CreateDirectoryTree(*DestDir);

#if PLATFORM_MAC || PLATFORM_LINUX
  return RunProcess(TEXT("/usr/bin/tar"), FString::Printf(
      TEXT("xzf \"%s\" -C \"%s\""), *TarPath, *DestDir)) == 0;
#elif PLATFORM_WINDOWS
  return RunProcess(TEXT("tar.exe"), FString::Printf(
      TEXT("xzf \"%s\" -C \"%s\""), *TarPath, *DestDir)) == 0;
#else
  return false;
#endif
}

/**
 * Reports ThirdParty dependency presence without modifying anything.
 * User Story: As setup diagnostics, I need a read-only verification pass so I
 * can inspect native dependency readiness before changing the workspace.
 */
Result VerifyThirdParty() {
  const FString PluginDir =
      FPaths::ProjectPluginsDir() / TEXT("ForbocAI_SDK");
  const FString ThirdPartyDir = PluginDir / TEXT("ThirdParty");

  const FString LlamaInclude = ThirdPartyDir / TEXT("llama.cpp/include");
  const FString LlamaHeader = LlamaInclude / TEXT("llama.h");
  const FString GgmlHeader = LlamaInclude / TEXT("ggml.h");
  const FString GgmlAllocHeader = LlamaInclude / TEXT("ggml-alloc.h");
  const FString GgmlBackendHeader = LlamaInclude / TEXT("ggml-backend.h");
  const FString GgmlCpuHeader = LlamaInclude / TEXT("ggml-cpu.h");
  const FString GgmlOptHeader = LlamaInclude / TEXT("ggml-opt.h");
  const FString SqliteInclude = ThirdPartyDir / TEXT("sqlite-vss/include");
  const FString SqliteSrc = ThirdPartyDir / TEXT("sqlite-vss/src");

  IPlatformFile &PF = FPlatformFileManager::Get().GetPlatformFile();

  const bool bLlamaHeaders = PF.DirectoryExists(*LlamaInclude) &&
                             PF.FileExists(*LlamaHeader) &&
                             PF.FileExists(*GgmlHeader) &&
                             PF.FileExists(*GgmlAllocHeader) &&
                             PF.FileExists(*GgmlBackendHeader) &&
                             PF.FileExists(*GgmlCpuHeader) &&
                             PF.FileExists(*GgmlOptHeader);

#if PLATFORM_MAC
  const FString LlamaLib =
      ThirdPartyDir / TEXT("llama.cpp/lib/Mac/libllama.a");
#elif PLATFORM_WINDOWS
  const FString LlamaLib =
      ThirdPartyDir / TEXT("llama.cpp/lib/Win64/llama.lib");
#else
  const FString LlamaLib = TEXT("");
#endif
  const bool bLlamaLib =
      !LlamaLib.IsEmpty() && PF.FileExists(*LlamaLib);

#if PLATFORM_MAC
  const FString LlamaLibDir = ThirdPartyDir / TEXT("llama.cpp/lib/Mac");
  const bool bGgmlCore =
      PF.FileExists(*(LlamaLibDir / TEXT("libggml.a"))) &&
      PF.FileExists(*(LlamaLibDir / TEXT("libggml-base.a"))) &&
      PF.FileExists(*(LlamaLibDir / TEXT("libggml-cpu.a")));
  const bool bGgmlMetal =
      PF.FileExists(*(LlamaLibDir / TEXT("libggml-metal.a")));
  const bool bGgmlBlas =
      PF.FileExists(*(LlamaLibDir / TEXT("libggml-blas.a")));
#elif PLATFORM_WINDOWS
  const FString LlamaLibDir = ThirdPartyDir / TEXT("llama.cpp/lib/Win64");
  const bool bGgmlCore =
      PF.FileExists(*(LlamaLibDir / TEXT("ggml.lib"))) &&
      PF.FileExists(*(LlamaLibDir / TEXT("ggml-base.lib"))) &&
      PF.FileExists(*(LlamaLibDir / TEXT("ggml-cpu.lib")));
  const bool bGgmlMetal = false;
  const bool bGgmlBlas = false;
#else
  const FString LlamaLibDir = TEXT("");
  const bool bGgmlCore = false;
  const bool bGgmlMetal = false;
  const bool bGgmlBlas = false;
#endif

  const bool bSqliteHeaders = PF.DirectoryExists(*SqliteInclude) &&
                              PF.FileExists(*(SqliteInclude / TEXT("sqlite3.h")));
  const bool bSqliteAmalgamation =
      PF.DirectoryExists(*SqliteSrc) &&
      PF.FileExists(*(SqliteSrc / TEXT("sqlite3.c")));
  const bool bVec0 = PF.FileExists(*(SqliteSrc / TEXT("vec0.c")));

  const FString LocalInfra = FPaths::ProjectDir() / TEXT("local_infrastructure");
  const FString ModelsDir = LocalInfra / TEXT("models");
  TArray<FString> ModelFiles;
  PF.FindFiles(ModelFiles, *ModelsDir, TEXT(".gguf"));
  const int32 ModelCount = ModelFiles.Num();

  const FString VectorsDir = LocalInfra / TEXT("vectors");
  const bool bVectorDb = PF.DirectoryExists(*VectorsDir);

  UE_LOG(LogTemp, Display, TEXT(""));
  UE_LOG(LogTemp, Display, TEXT("=== ForbocAI ThirdParty Dependency Check ==="));
  UE_LOG(LogTemp, Display, TEXT(""));
  UE_LOG(LogTemp, Display, TEXT("  [%s] llama.cpp headers     (%s)"),
         bLlamaHeaders ? TEXT("OK") : TEXT("--"), *LlamaInclude);
  UE_LOG(LogTemp, Display, TEXT("       required headers      (llama.h, ggml.h, ggml-alloc.h, ggml-backend.h, ggml-cpu.h, ggml-opt.h)"));
  UE_LOG(LogTemp, Display, TEXT("  [%s] llama.cpp library     (%s)"),
         bLlamaLib ? TEXT("OK") : TEXT("--"), *LlamaLib);
  UE_LOG(LogTemp, Display, TEXT("  [%s] ggml core libs        (ggml, ggml-base, ggml-cpu)"),
         bGgmlCore ? TEXT("OK") : TEXT("--"));
#if PLATFORM_MAC
  UE_LOG(LogTemp, Display, TEXT("  [%s] ggml-metal            (%s)"),
         bGgmlMetal ? TEXT("OK") : TEXT("--"),
         *(LlamaLibDir / TEXT("libggml-metal.a")));
  UE_LOG(LogTemp, Display, TEXT("  [%s] ggml-blas             (%s)"),
         bGgmlBlas ? TEXT("OK") : TEXT("--"),
         *(LlamaLibDir / TEXT("libggml-blas.a")));
#endif
  UE_LOG(LogTemp, Display, TEXT("  [%s] sqlite3 headers       (%s)"),
         bSqliteHeaders ? TEXT("OK") : TEXT("--"), *SqliteInclude);
  UE_LOG(LogTemp, Display, TEXT("  [%s] sqlite3 amalgamation  (%s)"),
         bSqliteAmalgamation ? TEXT("OK") : TEXT("--"), *SqliteSrc);
  UE_LOG(LogTemp, Display, TEXT("  [%s] sqlite-vec (vec0.c)   (%s)"),
         bVec0 ? TEXT("OK") : TEXT("--"),
         *(SqliteSrc / TEXT("vec0.c")));
  UE_LOG(LogTemp, Display, TEXT(""));
  UE_LOG(LogTemp, Display, TEXT("  Compile defines:"));
  UE_LOG(LogTemp, Display, TEXT("    WITH_FORBOC_NATIVE     = %d"),
         WITH_FORBOC_NATIVE);
  UE_LOG(LogTemp, Display, TEXT("    WITH_FORBOC_SQLITE_VEC = %d"),
         WITH_FORBOC_SQLITE_VEC);
  UE_LOG(LogTemp, Display, TEXT(""));
  UE_LOG(LogTemp, Display, TEXT("  Runtime assets:"));
  UE_LOG(LogTemp, Display, TEXT("  [%s] GGUF models           (%d file(s) in %s)"),
         ModelCount > 0 ? TEXT("OK") : TEXT("--"), ModelCount, *ModelsDir);
  UE_LOG(LogTemp, Display, TEXT("  [%s] Vector DB             (%s)"),
         bVectorDb ? TEXT("OK") : TEXT("--"), *VectorsDir);
  UE_LOG(LogTemp, Display, TEXT(""));

  (!bLlamaHeaders || !bLlamaLib)
    ? [&]() {
        UE_LOG(LogTemp, Display, TEXT(
            "  To set up: setup_deps --llama-only, then setup_build_llama"));
      }()
    : (void)0;

  (!bSqliteHeaders || !bSqliteAmalgamation || !bVec0)
    ? [&]() {
        UE_LOG(LogTemp, Display, TEXT(
            "  To set up: setup_deps --sqlite-only"));
      }()
    : (void)0;

  (ModelCount == 0)
    ? [&]() {
        UE_LOG(LogTemp, Display, TEXT(
            "  Models download automatically on first cortex_init."));
      }()
    : (void)0;

  const bool bAllBuild = bLlamaHeaders && bLlamaLib && bGgmlCore &&
                         bSqliteHeaders && bSqliteAmalgamation && bVec0;
  return bAllBuild ? Result::Success("All build-time dependencies present")
                   : Result::Failure("Some build-time dependencies missing");
}

/**
 * Installs or verifies the ThirdParty dependency bundle.
 * User Story: As setup flows, I need one dependency installer so llama.cpp and
 * sqlite-vss assets can be downloaded and arranged predictably.
 */
Result SetupThirdPartyDeps(rtk::EnhancedStore<FStoreState> &Store,
                           const TArray<FString> &Args) {
  const FString PluginDir =
      FPaths::ProjectPluginsDir() / TEXT("ForbocAI_SDK");
  const FString ThirdPartyDir = PluginDir / TEXT("ThirdParty");
  const FString TmpDir =
      FPaths::ProjectDir() / TEXT("local_infrastructure/tmp");

  IPlatformFile &PF = FPlatformFileManager::Get().GetPlatformFile();

  /**
   * Arg parsing via recursive helper replaces for/if loop
   * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
   */
  struct ArgFlags {
    bool bDoSqlite;
    bool bDoLlama;
  };

  struct ParseArgsHelper {
    static ArgFlags apply(const TArray<FString> &Arr, int32 Idx,
                          bool bSqlite, bool bLlama) {
      return Idx >= Arr.Num()
        ? ArgFlags{bSqlite, bLlama}
        : Arr[Idx] == TEXT("--sqlite-only")
          ? apply(Arr, Idx + 1, bSqlite, false)
          : Arr[Idx] == TEXT("--llama-only")
            ? apply(Arr, Idx + 1, false, bLlama)
            : apply(Arr, Idx + 1, bSqlite, bLlama);
    }
  };

  const ArgFlags Flags = ParseArgsHelper::apply(Args, 0, true, true);
  const bool bDoSqlite = Flags.bDoSqlite;
  const bool bDoLlama = Flags.bDoLlama;

  UE_LOG(LogTemp, Display, TEXT(""));
  UE_LOG(LogTemp, Display, TEXT("=== ForbocAI ThirdParty Setup ==="));

  const FString LlamaIncDir = ThirdPartyDir / TEXT("llama.cpp/include");
  const FString LlamaLibMac = ThirdPartyDir / TEXT("llama.cpp/lib/Mac");
  const FString LlamaLibWin = ThirdPartyDir / TEXT("llama.cpp/lib/Win64");
  const FString SqliteIncDir = ThirdPartyDir / TEXT("sqlite-vss/include");
  const FString SqliteSrcDir = ThirdPartyDir / TEXT("sqlite-vss/src");

  PF.CreateDirectoryTree(*LlamaIncDir);
  PF.CreateDirectoryTree(*LlamaLibMac);
  PF.CreateDirectoryTree(*LlamaLibWin);
  PF.CreateDirectoryTree(*SqliteIncDir);
  PF.CreateDirectoryTree(*SqliteSrcDir);
  PF.CreateDirectoryTree(*TmpDir);

  int32 DownloadCount = 0;
  int32 FailCount = 0;

  /**
   * DownloadOne lambda — if (FileExists) replaced with ternary
   */
  auto DownloadOne = [&Store, &DownloadCount,
                      &FailCount](const FString &Url, const FString &Dest) -> bool {
    return FPlatformFileManager::Get().GetPlatformFile().FileExists(*Dest)
      ? [&]() -> bool {
          UE_LOG(LogTemp, Display, TEXT("  [skip] %s (exists)"),
                 *FPaths::GetCleanFilename(Dest));
          return true;
        }()
      : [&]() -> bool {
          UE_LOG(LogTemp, Display, TEXT("  [download] %s"),
                 *FPaths::GetCleanFilename(Dest));
          try {
            Ops::WaitForResult(Native::File::DownloadBinary(Url, Dest), 120.0);
            ++DownloadCount;
            return true;
          } catch (const std::exception &E) {
            (void)E;
            UE_LOG(LogTemp, Warning, TEXT("  [FAIL] %s: %s"),
                   *FPaths::GetCleanFilename(Dest),
                   UTF8_TO_TCHAR(E.what()));
            ++FailCount;
            return false;
          }
        }();
  };

  /**
   * sqlite3 amalgamation + sqlite-vec (replaces vendor_sqlite in script)
   * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
   */
  bDoSqlite
    ? [&]() {
        UE_LOG(LogTemp, Display, TEXT(""));
        UE_LOG(LogTemp, Display, TEXT("  --- sqlite3 + sqlite-vec ---"));

        const FString Sqlite3hDest = SqliteIncDir / TEXT("sqlite3.h");
        const FString Sqlite3extDest = SqliteIncDir / TEXT("sqlite3ext.h");
        const FString Sqlite3cDest = SqliteSrcDir / TEXT("sqlite3.c");

        /**
         * Download + extract sqlite3 amalgamation if any file missing
         * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
         */
        (!PF.FileExists(*Sqlite3hDest) || !PF.FileExists(*Sqlite3cDest))
          ? [&]() {
              const FString SqliteZipUrl = FString::Printf(
                  TEXT("https://www.sqlite.org/2024/sqlite-amalgamation-%s.zip"),
                  SQLITE_VERSION);
              const FString SqliteZip = TmpDir / TEXT("sqlite-amalgamation.zip");
              const FString SqliteExtractDir = TmpDir / TEXT("sqlite-extract");

              DownloadOne(SqliteZipUrl, SqliteZip)
                ? [&]() {
                    UE_LOG(LogTemp, Display, TEXT("  [extract] sqlite-amalgamation.zip"));
                    ExtractZip(SqliteZip, SqliteExtractDir)
                      ? [&]() {
                          const FString InnerDir = FindSubdirWithPrefix(
                              SqliteExtractDir, TEXT("sqlite-amalgamation-"));

                          !InnerDir.IsEmpty()
                            ? [&]() {
                                IFileManager &FM = IFileManager::Get();
                                FM.Copy(*Sqlite3hDest, *(InnerDir / TEXT("sqlite3.h")));
                                FM.Copy(*Sqlite3extDest, *(InnerDir / TEXT("sqlite3ext.h")));
                                FM.Copy(*Sqlite3cDest, *(InnerDir / TEXT("sqlite3.c")));
                                UE_LOG(LogTemp, Display, TEXT("  [OK] sqlite3 amalgamation installed"));
                              }()
                            : [&]() {
                                UE_LOG(LogTemp, Warning,
                                       TEXT("  [FAIL] Could not find sqlite-amalgamation-* in archive"));
                                ++FailCount;
                              }();
                        }()
                      : [&]() {
                          ++FailCount;
                        }();
                  }()
                : (void)0;
            }()
          : [&]() {
              UE_LOG(LogTemp, Display, TEXT("  [skip] sqlite3 amalgamation (exists)"));
            }();

        /**
         * Download + extract sqlite-vec
         * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
         */
        const FString Vec0Dest = SqliteSrcDir / TEXT("vec0.c");
        !PF.FileExists(*Vec0Dest)
          ? [&]() {
              const FString VecZipUrl = FString::Printf(
                  TEXT("https://github.com/asg017/sqlite-vec/releases/download/%s/"
                       "sqlite-vec-%s-amalgamation.zip"),
                  SQLITE_VEC_VERSION,
                  *FString(SQLITE_VEC_VERSION).Replace(TEXT("v"), TEXT("")));
              const FString VecZip = TmpDir / TEXT("sqlite-vec-amalgamation.zip");
              const FString VecExtractDir = TmpDir / TEXT("sqlite-vec-extract");

              DownloadOne(VecZipUrl, VecZip)
                ? [&]() {
                    UE_LOG(LogTemp, Display, TEXT("  [extract] sqlite-vec-amalgamation.zip"));
                    ExtractZip(VecZip, VecExtractDir)
                      ? [&]() {
                          /**
                           * sqlite-vec amalgamation zip contains sqlite-vec.c at root
                           * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
                           */
                          IFileManager &FM = IFileManager::Get();
                          const FString VecSrc1 = VecExtractDir / TEXT("sqlite-vec.c");
                          const FString VecSrc2 = VecExtractDir / TEXT("vec0.c");
                          /**
                           * Check for nested dir too
                           * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
                           */
                          const FString VecSubDir = FindSubdirWithPrefix(
                              VecExtractDir, TEXT("sqlite-vec-"));
                          const FString VecSrc3 =
                              VecSubDir.IsEmpty() ? TEXT("")
                                                 : (VecSubDir / TEXT("sqlite-vec.c"));

                          PF.FileExists(*VecSrc1)
                            ? [&]() { FM.Copy(*Vec0Dest, *VecSrc1); }()
                            : PF.FileExists(*VecSrc2)
                            ? [&]() { FM.Copy(*Vec0Dest, *VecSrc2); }()
                            : (!VecSrc3.IsEmpty() && PF.FileExists(*VecSrc3))
                            ? [&]() { FM.Copy(*Vec0Dest, *VecSrc3); }()
                            : [&]() {
                                /**
                                 * Try src/ subdirectory
                                 * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
                                 */
                                const FString VecSrc4 =
                                    VecSubDir.IsEmpty()
                                        ? TEXT("")
                                        : (VecSubDir / TEXT("src/sqlite-vec.c"));
                                (!VecSrc4.IsEmpty() && PF.FileExists(*VecSrc4))
                                  ? [&]() { FM.Copy(*Vec0Dest, *VecSrc4); }()
                                  : [&]() {
                                      UE_LOG(LogTemp, Warning,
                                             TEXT("  [FAIL] Could not find sqlite-vec.c or vec0.c in archive"));
                                      ++FailCount;
                                    }();
                              }();

                          PF.FileExists(*Vec0Dest)
                            ? [&]() {
                                UE_LOG(LogTemp, Display, TEXT("  [OK] sqlite-vec (vec0.c) installed"));
                              }()
                            : (void)0;

                          /**
                           * Copy header if present
                           * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
                           */
                          const FString VecHdrSrc =
                              VecSubDir.IsEmpty() ? (VecExtractDir / TEXT("sqlite-vec.h"))
                                                 : (VecSubDir / TEXT("sqlite-vec.h"));
                          PF.FileExists(*VecHdrSrc)
                            ? [&]() { FM.Copy(*(SqliteIncDir / TEXT("sqlite-vec.h")), *VecHdrSrc); }()
                            : (void)0;
                        }()
                      : [&]() {
                          ++FailCount;
                        }();
                  }()
                : (void)0;
            }()
          : [&]() {
              UE_LOG(LogTemp, Display, TEXT("  [skip] sqlite-vec vec0.c (exists)"));
            }();
      }()
    : (void)0;

  /**
   * llama.cpp headers (replaces vendor_llama_headers in script)
   * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
   */
  bDoLlama
    ? [&]() {
        UE_LOG(LogTemp, Display, TEXT(""));
        UE_LOG(LogTemp, Display, TEXT("  --- llama.cpp headers (%s) ---"), LLAMA_CPP_TAG);

        const FString LlamaBase = FString::Printf(
            TEXT("https://raw.githubusercontent.com/ggml-org/llama.cpp/%s"),
            LLAMA_CPP_TAG);

        DownloadOne(LlamaBase / TEXT("include/llama.h"),
                    LlamaIncDir / TEXT("llama.h"));
        DownloadOne(LlamaBase / TEXT("ggml/include/ggml.h"),
                    LlamaIncDir / TEXT("ggml.h"));
        DownloadOne(LlamaBase / TEXT("ggml/include/ggml-alloc.h"),
                    LlamaIncDir / TEXT("ggml-alloc.h"));
        DownloadOne(LlamaBase / TEXT("ggml/include/ggml-backend.h"),
                    LlamaIncDir / TEXT("ggml-backend.h"));
        DownloadOne(LlamaBase / TEXT("ggml/include/ggml-cpu.h"),
                    LlamaIncDir / TEXT("ggml-cpu.h"));
        DownloadOne(LlamaBase / TEXT("ggml/include/ggml-opt.h"),
                    LlamaIncDir / TEXT("ggml-opt.h"));

        UE_LOG(LogTemp, Display, TEXT(""));
        UE_LOG(LogTemp, Display, TEXT(
            "  To build libllama, run: setup_build_llama"));
      }()
    : (void)0;

  /**
   * Clean up temp dir
   * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
   */
  IFileManager::Get().DeleteDirectory(*TmpDir, true, true);

  UE_LOG(LogTemp, Display, TEXT(""));
  UE_LOG(LogTemp, Display, TEXT("  Downloaded: %d | Failed: %d"),
         DownloadCount, FailCount);
  UE_LOG(LogTemp, Display, TEXT(
      "  Rebuild the UE project to pick up WITH_FORBOC_NATIVE / WITH_FORBOC_SQLITE_VEC."));
  UE_LOG(LogTemp, Display, TEXT(""));

  return FailCount == 0
             ? Result::Success("ThirdParty setup completed")
             : Result::Failure("Some downloads failed — see logs above");
}

/**
 * Builds the native llama.cpp library for the current platform.
 * User Story: As native setup, I need a build helper so the local inference
 * library can be compiled after sources are downloaded.
 */
Result BuildLlama(const TArray<FString> &Args) {
  const FString PluginDir =
      FPaths::ProjectPluginsDir() / TEXT("ForbocAI_SDK");
  const FString ThirdPartyDir = PluginDir / TEXT("ThirdParty");
  const FString TmpDir =
      FPaths::ProjectDir() / TEXT("local_infrastructure/tmp/llama-build");

  /**
   * Arg parsing via recursive helper replaces for/if loop
   * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
   */
  struct BuildArgFlags {
    FString Tag;
    FString DeploymentTarget;
  };

  struct ParseBuildArgsHelper {
    static BuildArgFlags apply(const TArray<FString> &Arr, int32 Idx,
                               const FString &Tag, const FString &DT) {
      return Idx >= Arr.Num()
        ? BuildArgFlags{Tag, DT}
        : Arr[Idx].StartsWith(TEXT("--tag="))
          ? apply(Arr, Idx + 1, Arr[Idx].Mid(6), DT)
#if PLATFORM_MAC
          : Arr[Idx].StartsWith(TEXT("--macos-deployment-target="))
            ? apply(Arr, Idx + 1, Tag,
                    Arr[Idx].Mid(FString(TEXT("--macos-deployment-target=")).Len()))
#endif
            : apply(Arr, Idx + 1, Tag, DT);
    }
  };

  const BuildArgFlags BAFlags = ParseBuildArgsHelper::apply(
      Args, 0, FString(LLAMA_CPP_TAG), FString(MACOS_DEPLOYMENT_TARGET));
  const FString Tag = BAFlags.Tag;
  const FString DeploymentTarget = BAFlags.DeploymentTarget;

  IPlatformFile &PF = FPlatformFileManager::Get().GetPlatformFile();

  /**
   * Check if lib already exists
   * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
   */
#if PLATFORM_MAC
  const FString LibDest = ThirdPartyDir / TEXT("llama.cpp/lib/Mac/libllama.a");
  const FString LibName = TEXT("libllama.a");
#elif PLATFORM_WINDOWS
  const FString LibDest = ThirdPartyDir / TEXT("llama.cpp/lib/Win64/llama.lib");
  const FString LibName = TEXT("llama.lib");
#else
  return Result::Failure("build_llama not supported on this platform");
#endif

  return PF.FileExists(*LibDest)
    ? [&]() -> Result {
        UE_LOG(LogTemp, Display, TEXT("  [skip] %s already exists at %s"),
               *LibName, *LibDest);
        return Result::Success("llama library already present");
      }()
    : [&]() -> Result {
        UE_LOG(LogTemp, Display, TEXT(""));
        UE_LOG(LogTemp, Display, TEXT("=== Building llama.cpp (%s) ==="), *Tag);
        UE_LOG(LogTemp, Display, TEXT("  This may take several minutes..."));
#if PLATFORM_MAC
        UE_LOG(LogTemp, Display, TEXT("  macOS deployment target: %s"),
               *DeploymentTarget);
#endif
        UE_LOG(LogTemp, Display, TEXT(""));

        /**
         * Clean and create build dir
         * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
         */
        IFileManager::Get().DeleteDirectory(*TmpDir, true, true);
        PF.CreateDirectoryTree(*TmpDir);

        /**
         * Step 1: Clone llama.cpp
         * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
         */
        UE_LOG(LogTemp, Display, TEXT("  Step 1/3: Cloning llama.cpp..."));
        const FString CloneDir = TmpDir / TEXT("llama.cpp");

#if PLATFORM_MAC || PLATFORM_LINUX
        const FString GitExe = TEXT("/usr/bin/git");
#elif PLATFORM_WINDOWS
        const FString GitExe = TEXT("git.exe");
#endif

        const int32 CloneRc = RunProcess(GitExe, FString::Printf(
            TEXT("clone --depth 1 --branch %s https://github.com/ggml-org/llama.cpp \"%s\""),
            *Tag, *CloneDir), TEXT(""), 120.0f);

        return CloneRc != 0
          ? Result::Failure("git clone failed")
          : [&]() -> Result {
              /**
               * Step 2: CMake configure
               * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
               */
              UE_LOG(LogTemp, Display, TEXT("  Step 2/3: Configuring with CMake..."));
              const FString BuildDir = CloneDir / TEXT("build");

#if PLATFORM_MAC || PLATFORM_LINUX
              /**
               * Try Homebrew paths first (Apple Silicon then Intel), fallback to PATH
               * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
               */
              const FString CmakeExe =
                  PF.FileExists(TEXT("/opt/homebrew/bin/cmake"))
                      ? FString(TEXT("/opt/homebrew/bin/cmake"))
                      : (PF.FileExists(TEXT("/usr/local/bin/cmake"))
                             ? FString(TEXT("/usr/local/bin/cmake"))
                             : FString(TEXT("cmake")));
              const FString CmakeExeAlt = TEXT("cmake");
#elif PLATFORM_WINDOWS
              const FString CmakeExe = TEXT("cmake.exe");
              const FString CmakeExeAlt = CmakeExe;
#endif

              FString CmakeConfigArgs = FString::Printf(
                  TEXT("-B \"%s\" -DBUILD_SHARED_LIBS=OFF"), *BuildDir);

#if PLATFORM_MAC
              CmakeConfigArgs += TEXT(" -DGGML_METAL=ON");
              CmakeConfigArgs += FString::Printf(
                  TEXT(" -DCMAKE_OSX_DEPLOYMENT_TARGET=%s"), *DeploymentTarget);
#endif

              const int32 ConfigRc = RunProcess(CmakeExe, CmakeConfigArgs, CloneDir, 120.0f);
              /**
               * Retry with alternative cmake path
               * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
               */
              const int32 EffectiveConfigRc = ConfigRc != 0
                  ? RunProcess(CmakeExeAlt, CmakeConfigArgs, CloneDir, 120.0f)
                  : ConfigRc;

              return EffectiveConfigRc != 0
                ? Result::Failure("cmake configure failed — is cmake installed?")
                : [&]() -> Result {
                    /**
                     * Step 3: Build
                     * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
                     */
                    UE_LOG(LogTemp, Display, TEXT("  Step 3/3: Building llama target..."));
                    const FString CmakeBuildArgs = FString::Printf(
                        TEXT("--build \"%s\" --target llama -j"), *BuildDir);

                    const int32 BuildRc = RunProcess(CmakeExe, CmakeBuildArgs, CloneDir, 600.0f);
                    const int32 EffectiveBuildRc = BuildRc != 0
                        ? RunProcess(CmakeExeAlt, CmakeBuildArgs, CloneDir, 600.0f)
                        : BuildRc;

                    return EffectiveBuildRc != 0
                      ? Result::Failure("cmake build failed")
                      : [&]() -> Result {
                          /**
                           * Copy llama + ggml libraries to ThirdParty
                           * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
                           */
#if PLATFORM_MAC
                          const FString BuiltLib = BuildDir / TEXT("src/libllama.a");
#elif PLATFORM_WINDOWS
                          const FString BuiltLib = BuildDir / TEXT("src/Release/llama.lib");
#endif

                          PF.CreateDirectoryTree(*FPaths::GetPath(LibDest));

                          return PF.FileExists(*BuiltLib)
                            ? [&]() -> Result {
                                IFileManager::Get().Copy(*LibDest, *BuiltLib);
                                UE_LOG(LogTemp, Display, TEXT(""));
                                UE_LOG(LogTemp, Display, TEXT("  [OK] %s copied to %s"), *LibName, *LibDest);

                                /**
                                 * Copy ggml dependency libraries (Build.cs links these alongside libllama)
                                 * User Story: As build setup, I need ggml libs copied so UE linking succeeds.
                                 */
                                const FString LibDestDir = FPaths::GetPath(LibDest);
                                IFileManager &FM = IFileManager::Get();
                                int32 GgmlCopied = 0;

#if PLATFORM_MAC
                                struct GgmlEntry {
                                  const TCHAR *BuildSubPath;
                                  const TCHAR *DestName;
                                };
                                const GgmlEntry GgmlLibs[] = {
                                    {TEXT("ggml/src/libggml.a"), TEXT("libggml.a")},
                                    {TEXT("ggml/src/libggml-base.a"), TEXT("libggml-base.a")},
                                    {TEXT("ggml/src/libggml-cpu.a"), TEXT("libggml-cpu.a")},
                                    {TEXT("ggml/src/ggml-metal/libggml-metal.a"), TEXT("libggml-metal.a")},
                                    {TEXT("ggml/src/ggml-blas/libggml-blas.a"), TEXT("libggml-blas.a")},
                                };
#elif PLATFORM_WINDOWS
                                struct GgmlEntry {
                                  const TCHAR *BuildSubPath;
                                  const TCHAR *DestName;
                                };
                                const GgmlEntry GgmlLibs[] = {
                                    {TEXT("ggml/src/Release/ggml.lib"), TEXT("ggml.lib")},
                                    {TEXT("ggml/src/Release/ggml-base.lib"), TEXT("ggml-base.lib")},
                                    {TEXT("ggml/src/Release/ggml-cpu.lib"), TEXT("ggml-cpu.lib")},
                                };
#endif

                                const int32 GgmlCount = sizeof(GgmlLibs) / sizeof(GgmlLibs[0]);

                                /**
                                 * Recursive helper replaces for loop over ggml libs
                                 */
                                struct CopyGgmlHelper {
                                  static int32 apply(const GgmlEntry *Libs, int32 Count, int32 Idx,
                                                     const FString &BuildDir, const FString &LibDestDir,
                                                     IPlatformFile &PF, IFileManager &FM, int32 Copied) {
                                    return Idx >= Count
                                      ? Copied
                                      : [&]() -> int32 {
                                          const FString Src = BuildDir / Libs[Idx].BuildSubPath;
                                          const FString Dst = LibDestDir / Libs[Idx].DestName;
                                          return PF.FileExists(*Src)
                                            ? [&]() -> int32 {
                                                FM.Copy(*Dst, *Src);
                                                UE_LOG(LogTemp, Display, TEXT("  [OK] %s"), Libs[Idx].DestName);
                                                return apply(Libs, Count, Idx + 1, BuildDir,
                                                             LibDestDir, PF, FM, Copied + 1);
                                              }()
                                            : [&]() -> int32 {
                                                UE_LOG(LogTemp, Warning, TEXT("  [--] %s not found at %s"),
                                                       Libs[Idx].DestName, *Src);
                                                return apply(Libs, Count, Idx + 1, BuildDir,
                                                             LibDestDir, PF, FM, Copied);
                                              }();
                                        }();
                                  }
                                };

                                GgmlCopied = CopyGgmlHelper::apply(
                                    GgmlLibs, GgmlCount, 0, BuildDir, LibDestDir, PF, FM, 0);

                                UE_LOG(LogTemp, Display, TEXT("  Copied %d ggml libraries"), GgmlCopied);

                                /**
                                 * Clean up build dir
                                 * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
                                 */
                                IFileManager::Get().DeleteDirectory(*TmpDir, true, true);

                                UE_LOG(LogTemp, Display, TEXT(
                                    "  Rebuild the UE project to enable WITH_FORBOC_NATIVE=1."));
                                UE_LOG(LogTemp, Display, TEXT(""));

                                return Result::Success("llama.cpp built and installed");
                              }()
                            : [&]() -> Result {
                                UE_LOG(LogTemp, Warning, TEXT("  [FAIL] Built library not found at %s"),
                                       *BuiltLib);
                                return Result::Failure("Build succeeded but library not found");
                              }();
                        }();
                  }();
            }();
      }();
}

} // anonymous namespace

/**
 * Routes setup-related CLI commands to the appropriate setup helper.
 * User Story: As CLI users, I need setup commands dispatched through one
 * handler so verification, install, and build flows share parsing logic.
 */
HandlerResult HandleSetup(rtk::EnhancedStore<FStoreState> &Store,
                          const FString &CommandKey,
                          const TArray<FString> &Args) {
  using func::just;
  using func::nothing;

  return (CommandKey == TEXT("setup") || CommandKey == TEXT("setup_deps"))
    ? just(SetupThirdPartyDeps(Store, Args))
    : (CommandKey == TEXT("setup_check") || CommandKey == TEXT("setup_verify"))
    ? [&]() -> HandlerResult {
        (void)Store;
        return just(VerifyThirdParty());
      }()
    : (CommandKey == TEXT("setup_build_llama"))
    ? [&]() -> HandlerResult {
        (void)Store;
        return just(BuildLlama(Args));
      }()
    : (CommandKey == TEXT("setup_runtime_check"))
    ? just(RunRuntimeSmokeCheck(Store, Args))
    : nothing<Result>();
}

} // namespace Handlers
} // namespace CLIOps
