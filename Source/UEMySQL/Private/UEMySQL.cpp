// =============================================================================
// 文件名：UEMySQL.cpp
// 所属模块：UEMySQL（业务 / 反射层包装模块，可选占位）
// 用途：本模块不引入 Engine 依赖，仅作为 UEMySQLCore 的上层包装占位。
//       若后续需要蓝图暴露（UFUNCTION(BlueprintCallable) 包装类），在此扩展，
//       并为本模块的 Build.cs 增加 "Engine" 依赖即可。
// =============================================================================

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FUEMySQLWrapperModule : public IModuleInterface
{
public:
	virtual void StartupModule() override {}
	virtual void ShutdownModule() override {}
};

IMPLEMENT_MODULE(FUEMySQLWrapperModule, UEMySQL)
