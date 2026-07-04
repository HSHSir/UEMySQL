using UnrealBuildTool;
using System.IO;

public class UEMySQL : ModuleRules
{
	public UEMySQL(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		bEnforceIWYU = false;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine"
		});

		PrivateDependencyModuleNames.AddRange(new string[] {
			"Projects"
		});

		CppStandard = CppStandardVersion.Cpp17;

		// ================= MySQL Connector/C 6.1 (libmysql) =================
		// ThirdParty 目录结构（需自行放置，详见 Documentation/使用文档.html）:
		//   ThirdParty/MySQL/include/  -> mysql.h 等头文件
		//   ThirdParty/MySQL/lib/      -> libmysql.lib (导入库)
		//   ThirdParty/MySQL/bin/      -> libmysql.dll (运行时依赖)
		string MySQLRoot   = Path.Combine(ModuleDirectory, "..", "..", "ThirdParty", "MySQL");
		string IncludePath = Path.Combine(MySQLRoot, "include");
		string LibPath     = Path.Combine(MySQLRoot, "lib");
		string BinPath     = Path.Combine(MySQLRoot, "bin");

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			PublicSystemIncludePaths.Add(IncludePath);

			string ImportLib = Path.Combine(LibPath, "libmysql.lib");
			PublicAdditionalLibraries.Add(ImportLib);

			string Dll = Path.Combine(BinPath, "libmysql.dll");
			// 声明运行时依赖，打包时会自动拷贝到 Binaries/Win64
			RuntimeDependencies.Add("$(BinaryOutputDir)/libmysql.dll", Dll);
			// 编辑器/独立运行时按需加载 DLL
			PublicDelayLoadDLLs.Add("libmysql.dll");

			PublicDefinitions.Add("WITH_UEMYSQL=1");
		}
		else
		{
			PublicDefinitions.Add("WITH_UEMYSQL=0");
		}
	}
}
