// Copyright NetGame. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

DECLARE_LOG_CATEGORY_EXTERN(LogUEMySQL, Log, All);

class FUEMySQLModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	static inline FUEMySQLModule& Get()
	{
		return FModuleManager::LoadModuleChecked<FUEMySQLModule>("UEMySQL");
	}

	/** libmysql.dll 是否已成功加载 */
	bool IsMySQLLibraryLoaded() const { return LibmysqlHandle != nullptr; }

private:
	/** 延迟加载的 libmysql.dll 句柄 */
	void* LibmysqlHandle = nullptr;
};
