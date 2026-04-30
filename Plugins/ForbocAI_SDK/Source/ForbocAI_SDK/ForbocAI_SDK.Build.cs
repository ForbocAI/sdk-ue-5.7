using UnrealBuildTool;

public class ForbocAI_SDK : ModuleRules
{
	public ForbocAI_SDK(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "HTTP", "Json", "JsonUtilities" });

		/**
		 * --- NATIVE PARITY: sqlite-vss (vector memory) ---
		 * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
		 *
		 * Architecture note (0.4.0+): NPC SLM inference is API-hosted via the
		 * proprietary ForbocAI-NPC-SLM model. The SDK no longer ships or links
		 * llama.cpp — ThirdParty/llama.cpp was removed. Local capabilities
		 * retained by the SDK: vector memory (sqlite-vss), actor identification,
		 * and web3/soul transport.
		 */
		string ThirdPartyPath = System.IO.Path.Combine(ModuleDirectory, "../../ThirdParty");
		string SqliteIncludePath = System.IO.Path.Combine(ThirdPartyPath, "sqlite-vss/include");
		bool bHasSqliteHeaders = System.IO.Directory.Exists(SqliteIncludePath);
		bool bHasSqlite3Header = bHasSqliteHeaders
			&& System.IO.File.Exists(System.IO.Path.Combine(SqliteIncludePath, "sqlite3.h"));

		/**
		 * Auto-detect sqlite-vec amalgamation source files
		 * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
		 */
		string SqliteAmalgamationPath = System.IO.Path.Combine(ThirdPartyPath, "sqlite-vss/src");
		bool bHasSqliteAmalgamation = System.IO.Directory.Exists(SqliteAmalgamationPath)
			&& System.IO.File.Exists(System.IO.Path.Combine(SqliteAmalgamationPath, "sqlite3.c"));

		if (bHasSqliteHeaders)
		{
			PublicIncludePaths.Add(SqliteIncludePath);
		}

		/**
		 * Compile sqlite3 amalgamation + sqlite-vec into the plugin when source is present
		 * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
		 */
		if (bHasSqliteAmalgamation)
		{
			PrivateIncludePaths.Add(SqliteAmalgamationPath);

			/**
			 * Compile sqlite3 + sqlite-vec amalgamation as C source
			 * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
			 */
			string Sqlite3Source = System.IO.Path.Combine(SqliteAmalgamationPath, "sqlite3.c");
			string Vec0Source = System.IO.Path.Combine(SqliteAmalgamationPath, "vec0.c");

			if (System.IO.File.Exists(Sqlite3Source))
			{
				PrivateDefinitions.Add("SQLITE_CORE=1");
				PrivateDefinitions.Add("SQLITE_THREADSAFE=1");
			}

			/**
			 * Suppress warnings in third-party C code
			 * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
			 */
			bEnableExceptions = true;
			CppCompileWarningSettings.UndefinedIdentifierWarningLevel = WarningLevel.Off;
			CppCompileWarningSettings.ShadowVariableWarningLevel = WarningLevel.Off;
		}

		/**
		 * WITH_FORBOC_NATIVE is always 0 now that SLM inference is API-hosted.
		 * Retained for compile-time guards in source that still reference it.
		 */
		PublicDefinitions.Add("WITH_FORBOC_NATIVE=0");
		/**
		 * sqlite-vec auto-enabled when sqlite3.h header and amalgamation source are present
		 * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
		 */
		bool bEnableSqliteVec = bHasSqlite3Header && bHasSqliteAmalgamation;
		PublicDefinitions.Add("WITH_FORBOC_SQLITE_VEC=" + (bEnableSqliteVec ? "1" : "0"));
	}
}
