using UnrealBuildTool;
using System.IO;

public class UEMySQLCore : ModuleRules
{
	public UEMySQLCore(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		bEnforceIWYU = false;

		// 底层核心模块：MySQL 客户端逻辑（libmysql 封装），不依赖 Engine。
		// 服务器程序只需链接本模块即可使用数据库连接 / 连接池 / 预处理语句。
		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject"
		});

		PrivateDependencyModuleNames.AddRange(new string[] {
			"Projects"
		});

		CppStandard = CppStandardVersion.Cpp17;

		// ================= MySQL Connector/C 6.1 (libmysql) =================
		// ThirdParty 目录结构：
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
			// 声明运行时依赖，打包/构建时会自动拷贝到输出目录
			RuntimeDependencies.Add("$(BinaryOutputDir)/libmysql.dll", Dll);
			// 延迟加载：缺失时程序仍可启动（仅 MySQL 功能不可用）
			PublicDelayLoadDLLs.Add("libmysql.dll");

			PublicDefinitions.Add("WITH_UEMYSQL=1");
		}
		else
		{
			PublicDefinitions.Add("WITH_UEMYSQL=0");
		}
	}
}
