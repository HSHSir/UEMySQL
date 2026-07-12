using UnrealBuildTool;

public class UEMySQL : ModuleRules
{
	public UEMySQL(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		bEnforceIWYU = false;

		// 业务 / 反射层包装模块（可选占位）。
		// 当前不引入 Engine 依赖，避免无谓的引擎编译；服务器程序只需链接
		// 底层核心模块 UEMySQLCore 即可。若后续需要把 MySQL 能力以
		// UFUNCTION(BlueprintCallable) 暴露给蓝图，请在本模块中加入 UObject
		// 包装类，并为本 Build.cs 增加 "Engine" 依赖。
		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"UEMySQLCore"
		});

		CppStandard = CppStandardVersion.Cpp17;
	}
}
