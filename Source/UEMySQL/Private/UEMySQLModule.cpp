// Copyright NetGame. All Rights Reserved.
#include "UEMySQLModule.h"
#include "Core.h"
#include "Modules/ModuleManager.h"
#include "Interfaces/IPluginManager.h"
#include "HAL/PlatformProcess.h"

DEFINE_LOG_CATEGORY(LogUEMySQL);

#define LOCTEXT_NAMESPACE "FUEMySQLModule"

void FUEMySQLModule::StartupModule()
{
#if WITH_UEMYSQL
	// libmysql.dll 由 Build.cs 通过 RuntimeDependencies 声明,打包时会自动拷贝到
	// BinaryOutputDir(与 .exe / 模块 dll 同目录)。Build.cs 同时声明了
	// PublicDelayLoadDLLs,因此即使这里不显式加载,首次调用 mysql 函数时
	// Windows 也会从 .exe 目录找到它。这里显式加载是为了:
	//   1) 提前发现 DLL 缺失(而非等到第一次查询才报错);
	//   2) 让延迟加载直接复用已加载的 DLL,避免重复搜索。
	TArray<FString> CandidatePaths;

	// 1. 编辑器运行:从插件 ThirdParty 目录加载
	if (IPluginManager::Get().FindPlugin(TEXT("UEMySQL")))
	{
		FString BaseDir = IPluginManager::Get().FindPlugin(TEXT("UEMySQL"))->GetBaseDir();
		CandidatePaths.Add(FPaths::Combine(BaseDir, TEXT("ThirdParty/MySQL/bin/libmysql.dll")));
	}

	// 2. 打包后:FPaths::ProjectDir() 返回 .exe 所在目录(Build.cs 把 DLL 拷到了这里)
	CandidatePaths.Add(FPaths::Combine(FPaths::ProjectDir(), TEXT("libmysql.dll")));

	// 3. 编辑器未打包场景:ProjectDir/Binaries/Win64/
	CandidatePaths.Add(FPaths::Combine(FPaths::ProjectDir(), TEXT("Binaries/Win64/libmysql.dll")));

	// 4. 引擎二进制目录(兜底)
	CandidatePaths.Add(FPaths::Combine(FPaths::EngineDir(), TEXT("Binaries/Win64/libmysql.dll")));

	for (const FString& Candidate : CandidatePaths)
	{
		if (FPaths::FileExists(Candidate))
		{
			LibmysqlHandle = FPlatformProcess::GetDllHandle(*Candidate);
			if (LibmysqlHandle)
			{
				UE_LOG(LogUEMySQL, Log, TEXT("UEMySQL 模块启动:libmysql.dll 加载成功 (%s)。"), *Candidate);
				break;
			}
		}
	}

	if (!LibmysqlHandle)
	{
		// 即使显式加载失败,延迟加载机制仍可能在首次调用 mysql 函数时自动找到 DLL
		// (只要 DLL 与 .exe 同目录)。这里只记 Warning,不阻止模块加载。
		UE_LOG(LogUEMySQL, Warning,
			TEXT("UEMySQL 模块启动:未在候选路径找到 libmysql.dll,将依赖延迟加载机制。")
			TEXT("若后续连接失败,请确认 libmysql.dll 已随打包输出到 .exe 同目录。"));
	}
#else
	UE_LOG(LogUEMySQL, Warning, TEXT("UEMySQL 模块未启用（非 Win64 平台）。"));
#endif
}

void FUEMySQLModule::ShutdownModule()
{
#if WITH_UEMYSQL
	if (LibmysqlHandle)
	{
		FPlatformProcess::FreeDllHandle(LibmysqlHandle);
		LibmysqlHandle = nullptr;
		UE_LOG(LogUEMySQL, Log, TEXT("UEMySQL 模块关闭：libmysql.dll 已卸载。"));
	}
#endif
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FUEMySQLModule, UEMySQL)
