// Copyright NetGame. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UEMySQLTypes.h"
#include "UEMySQLConnectionPool.h"
#include "Async/Future.h"

/** 异步结果回调（默认在游戏线程触发）。 */
DECLARE_DELEGATE_OneParam(FMySQLResultDelegate, const FMySQLResult&);

/**
 * MySQL 异步执行器。
 *
 * 基于连接池，在后台线程执行 SQL，避免阻塞游戏主线程。
 * 结果回调默认切回游戏线程，方便更新 UI / Actor 状态。
 *
 * 典型用法（C++）：
 *   FMySQLConnectionConfig Cfg; Cfg.Host = ...; Cfg.Database = ...;
 *   auto Exec = MakeShared<FMySQLAsyncExecutor>();
 *   FString Err;
 *   Exec->Start(Cfg, Err);
 *
 *   TArray<FMySQLParam> P{ FMySQLParam::MakeString(TEXT("张三")) };
 *   Exec->ExecutePreparedAsync(TEXT("SELECT * FROM users WHERE name=?"), P,
 *       FMySQLResultDelegate::CreateLambda([](const FMySQLResult& R)
 *       {
 *           // 已在游戏线程
 *           if (R.bSuccess) { for (auto& Row : R.Rows) { ... } }
 *       }));
 */
class UEMYSQLCORE_API FMySQLAsyncExecutor : public TSharedFromThis<FMySQLAsyncExecutor, ESPMode::ThreadSafe>
{
public:
	FMySQLAsyncExecutor();
	~FMySQLAsyncExecutor();

	/** 启动：内部创建并初始化连接池。 */
	bool Start(const FMySQLConnectionConfig& InConfig, FString& OutError);

	/** 停止：关闭连接池。 */
	void Stop();

	bool IsRunning() const { return Pool.IsValid() && Pool->IsInitialized(); }

	/** 直接访问底层连接池（需要手动借还连接的高级用法）。 */
	FMySQLConnectionPoolPtr GetPool() const { return Pool; }

	// ---------------- 异步接口（回调在游戏线程） ----------------

	/** 异步执行原始 SQL。 */
	void ExecuteAsync(const FString& SQL, FMySQLResultDelegate OnComplete);

	/** 异步执行预处理语句（防注入，推荐）。 */
	void ExecutePreparedAsync(const FString& SQL, const TArray<FMySQLParam>& Params, FMySQLResultDelegate OnComplete);

	// ---------------- 基于 TFuture 的接口 ----------------

	/** 返回 TFuture 的预处理执行（在后台线程完成，调用方自行决定何时取结果）。 */
	TFuture<FMySQLResult> ExecutePreparedFuture(const FString& SQL, const TArray<FMySQLParam>& Params);

	// ---------------- 同步接口（当前线程阻塞，供后台线程/工具使用） ----------------

	FMySQLResult ExecuteSync(const FString& SQL);
	FMySQLResult ExecutePreparedSync(const FString& SQL, const TArray<FMySQLParam>& Params);

private:
	/** 在后台线程借连接执行，返回结果。 */
	FMySQLResult RunOnPool(const FString& SQL, const TArray<FMySQLParam>* Params);

	/** 把回调调度回游戏线程执行。 */
	static void DispatchToGameThread(FMySQLResultDelegate Delegate, FMySQLResult Result);

private:
	FMySQLConnectionPoolPtr Pool;
};

typedef TSharedPtr<FMySQLAsyncExecutor, ESPMode::ThreadSafe> FMySQLAsyncExecutorPtr;
