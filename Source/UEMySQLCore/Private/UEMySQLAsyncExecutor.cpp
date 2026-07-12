// Copyright NetGame. All Rights Reserved.
#include "UEMySQLAsyncExecutor.h"
#include "UEMySQLModule.h"
#include "Async/Async.h"

FMySQLAsyncExecutor::FMySQLAsyncExecutor()
{
}

FMySQLAsyncExecutor::~FMySQLAsyncExecutor()
{
	Stop();
}

bool FMySQLAsyncExecutor::Start(const FMySQLConnectionConfig& InConfig, FString& OutError)
{
	if (Pool.IsValid() && Pool->IsInitialized())
	{
		return true;
	}
	Pool = MakeShared<FMySQLConnectionPool, ESPMode::ThreadSafe>();
	if (!Pool->Initialize(InConfig, OutError))
	{
		Pool.Reset();
		return false;
	}
	return true;
}

void FMySQLAsyncExecutor::Stop()
{
	if (Pool.IsValid())
	{
		Pool->Shutdown();
		Pool.Reset();
	}
}

FMySQLResult FMySQLAsyncExecutor::RunOnPool(const FString& SQL, const TArray<FMySQLParam>* Params)
{
	FMySQLResult Result;
	if (!Pool.IsValid() || !Pool->IsInitialized())
	{
		Result.ErrorMessage = TEXT("执行器未启动或连接池未初始化。");
		return Result;
	}

	FMySQLScopedConnection Conn(Pool);
	if (!Conn.IsValid())
	{
		Result.ErrorMessage = TEXT("获取连接超时（连接池繁忙）。");
		return Result;
	}

	if (Params)
	{
		Result = Conn->ExecutePrepared(SQL, *Params);
	}
	else
	{
		Result = Conn->Execute(SQL);
	}
	return Result;
}

void FMySQLAsyncExecutor::DispatchToGameThread(FMySQLResultDelegate Delegate, FMySQLResult Result)
{
	if (!Delegate.IsBound())
	{
		return;
	}
	if (IsInGameThread())
	{
		Delegate.Execute(Result);
	}
	else
	{
		// 结果拷贝进 lambda，调度到游戏线程
		AsyncTask(ENamedThreads::GameThread, [Delegate, Result]()
		{
			Delegate.ExecuteIfBound(Result);
		});
	}
}

void FMySQLAsyncExecutor::ExecuteAsync(const FString& SQL, FMySQLResultDelegate OnComplete)
{
	TSharedRef<FMySQLAsyncExecutor, ESPMode::ThreadSafe> SelfRef = AsShared();
	const FString SqlCopy = SQL;
	Async(EAsyncExecution::ThreadPool, [SelfRef, SqlCopy, OnComplete]()
	{
		FMySQLResult R = SelfRef->RunOnPool(SqlCopy, nullptr);
		DispatchToGameThread(OnComplete, MoveTemp(R));
	});
}

void FMySQLAsyncExecutor::ExecutePreparedAsync(const FString& SQL, const TArray<FMySQLParam>& Params, FMySQLResultDelegate OnComplete)
{
	TSharedRef<FMySQLAsyncExecutor, ESPMode::ThreadSafe> SelfRef = AsShared();
	const FString SqlCopy = SQL;
	TArray<FMySQLParam> ParamsCopy = Params;
	Async(EAsyncExecution::ThreadPool, [SelfRef, SqlCopy, ParamsCopy, OnComplete]()
	{
		FMySQLResult R = SelfRef->RunOnPool(SqlCopy, &ParamsCopy);
		DispatchToGameThread(OnComplete, MoveTemp(R));
	});
}

TFuture<FMySQLResult> FMySQLAsyncExecutor::ExecutePreparedFuture(const FString& SQL, const TArray<FMySQLParam>& Params)
{
	TSharedRef<FMySQLAsyncExecutor, ESPMode::ThreadSafe> SelfRef = AsShared();
	const FString SqlCopy = SQL;
	TArray<FMySQLParam> ParamsCopy = Params;
	return Async(EAsyncExecution::ThreadPool, [SelfRef, SqlCopy, ParamsCopy]()
	{
		return SelfRef->RunOnPool(SqlCopy, &ParamsCopy);
	});
}

FMySQLResult FMySQLAsyncExecutor::ExecuteSync(const FString& SQL)
{
	return RunOnPool(SQL, nullptr);
}

FMySQLResult FMySQLAsyncExecutor::ExecutePreparedSync(const FString& SQL, const TArray<FMySQLParam>& Params)
{
	return RunOnPool(SQL, &Params);
}
