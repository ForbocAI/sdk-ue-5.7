#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "Internationalization/Regex.h"

// Define a test that natively runs the BDDCoverage evaluation check
DEFINE_SPEC(FBddCoverageSpec, "ForbocAI.Build.BDDCoverage",
            EAutomationTestFlags::ProductFilter |
                EAutomationTestFlags_ApplicationContextMask)

namespace {
TArray<FString> FindFiles(const FString& Directory, const FString& Extension) {
    TArray<FString> Files;
    IFileManager::Get().FindFilesRecursive(Files, *Directory, *FString::Printf(TEXT("*%s"), *Extension), true, false, false);
    return Files;
}

FString ReadFile(const FString& Path) {
    FString Content;
    FFileHelper::LoadFileToString(Content, *Path);
    return Content;
}

void ExtractRegexMatches(const FString& Content, const FString& PatternStr, int32 GroupIndex, TSet<FString>& OutMatches) {
    FRegexPattern Pattern(PatternStr);
    FRegexMatcher Matcher(Pattern, Content);
    while (Matcher.FindNext()) {
        OutMatches.Add(Matcher.GetCaptureGroup(GroupIndex));
    }
}
} // namespace

void FBddCoverageSpec::Define() {
  Describe("BDD Coverage", [this]() {
    It("must pass with 0 missing tags across all tested areas", [this]() {
      // Find the absolute root for the SDK Plugin directory
      FString PluginDir = FPaths::Combine(FPaths::ProjectDir(), TEXT("Plugins/ForbocAI_SDK"));
      if (!IFileManager::Get().DirectoryExists(*PluginDir)) { 
        // fallback mapping depending on UE directory structuring
        PluginDir = FPaths::Combine(FPaths::ProjectDir(), TEXT(".."));
      }
      
      FString SourceDir = FPaths::Combine(PluginDir, TEXT("Source/ForbocAI_SDK"));
      
      TSet<FString> CoveredTags;
      
      // Parse @covers tags from test files
      FString TestsDir = FPaths::Combine(SourceDir, TEXT("Private/Tests"));
      TArray<FString> TestFiles = FindFiles(TestsDir, TEXT(".cpp"));
      TestFiles.Append(FindFiles(TestsDir, TEXT(".h")));
      
      FRegexPattern CoversPattern(TEXT("@covers:([a-zA-Z]+):([A-Za-z0-9_./{}:-]+)"));
      for (const FString& File : TestFiles) {
          FString Content = ReadFile(File);
          FRegexMatcher Matcher(CoversPattern, Content);
          while (Matcher.FindNext()) {
              CoveredTags.Add(FString::Printf(TEXT("%s:%s"), *Matcher.GetCaptureGroup(1), *Matcher.GetCaptureGroup(2)));
          }
      }
      
      auto CheckCoverage = [this, &CoveredTags](const FString& Category, const TSet<FString>& Expected) {
          int32 MissingCount = 0;
          for (const FString& Item : Expected) {
              FString Tag = FString::Printf(TEXT("%s:%s"), *Category, *Item);
              if (!CoveredTags.Contains(Tag)) {
                  AddError(FString::Printf(TEXT("Missing BDD coverage: %s"), *Tag));
                  MissingCount++;
              }
          }
          if (Expected.Num() > 0) {
              TestTrue(FString::Printf(TEXT("All %s coverage resolved (Missing: %d)"), *Category, MissingCount), MissingCount == 0);
          }
      };

      // 1. Expected API
      TSet<FString> ExpectedApi;
      FString ApiEndpointsContent = ReadFile(FPaths::Combine(SourceDir, TEXT("Public/API/APIEndpoints.h")));
      ExtractRegexMatches(ApiEndpointsContent, TEXT("inline\\s+Thunk<[^>]+>\\s+(\\w+)\\s*\\("), 1, ExpectedApi);
      CheckCoverage(TEXT("api"), ExpectedApi);
      
      // 2. Expected Core Thunks
      TSet<FString> ExpectedThunks;
      TArray<FString> PublicFiles = FindFiles(FPaths::Combine(SourceDir, TEXT("Public")), TEXT(".h"));
      for (const FString& File : PublicFiles) {
          ExtractRegexMatches(ReadFile(File), TEXT("inline\\s+ThunkAction<[^,]+,\\s*[^>]+>\\s+(\\w+)\\s*\\("), 1, ExpectedThunks);
      }
      CheckCoverage(TEXT("coreThunk"), ExpectedThunks);
      
      // 3. Expected CLI Ops
      TSet<FString> ExpectedCliOps;
      FString CliOpsContent = ReadFile(FPaths::Combine(SourceDir, TEXT("Public/CLI/CliOperations.h")));
      
      FRegexPattern NsPattern(TEXT("namespace Ops \\{((?s).*?)\\} // namespace Ops"));
      FRegexMatcher NsMatcher(NsPattern, CliOpsContent);
      if (NsMatcher.FindNext()) {
          FString NsContent = NsMatcher.GetCaptureGroup(1);
          FRegexPattern FnPattern(TEXT("inline\\s+(?:[\\w:<>\\s\\[\\]*&]+?\\s+)(\\w+)\\s*\\("));
          FRegexMatcher FnMatcher(FnPattern, NsContent);
          while (FnMatcher.FindNext()) {
              FString Name = FnMatcher.GetCaptureGroup(1);
              if (Name != TEXT("Wait") && Name != TEXT("WaitForResult")) {
                  ExpectedCliOps.Add(Name);
              }
          }
      }
      CheckCoverage(TEXT("cliOp"), ExpectedCliOps);
      
      // 4. Expected CLI Actions
      TSet<FString> ExpectedCli;
      FString CommandletContent = ReadFile(FPaths::Combine(SourceDir, TEXT("Private/Commandlet.cpp")));
      FRegexPattern ValidCmdsPattern(TEXT("ValidCommands\\s*=\\s*\\{((?s)[^}]+)\\};"));
      FRegexMatcher ValidCmdsMatcher(ValidCmdsPattern, CommandletContent);
      if (ValidCmdsMatcher.FindNext()) {
          FString CmdList = ValidCmdsMatcher.GetCaptureGroup(1);
          ExtractRegexMatches(CmdList, TEXT("TEXT\\(\"([^\"]+)\"\\)"), 1, ExpectedCli);
      }
      CheckCoverage(TEXT("cli"), ExpectedCli);
    });
  });
}
