// Copyright NetGame. All Rights Reserved.
#include "UEMySQLConnectionPool.h"
#include "UEMySQLModule.h"
#include "HAL/PlatformProcess.h"

FMySQLConnectionPool::FMySQLConnectionPool()
{
	Available = FPlatformProcess::GetSynchEventFromPool(false);
}

FMySQLConnectionPool::~FMySQLConnectionPool()
{
	Shutdown();
	if (Available)
	{
		FPlatformProcess::ReturnSynchEventToPool(Available);
		Available = nullptr;
	}
}

FMySQLConnectionPtr FMySQLConnectionPool::CreateConnection(FString& OutError)
{
	FMySQLConnectionPtr Conn = MakeShared<FMySQLConnection, ESPMode::ThreadSafe>();
	if (!Conn->Connect(Config, OutError))
	{
		return nullptr;
	}
	return Conn;
}

bool FMySQLConnectionPool::Initialize(const FMySQLConnectionConfig& InConfig, FString& OutError)
{
	FScopeLock Lock(&Mutex);
	if (bInitialized)
	{
		return true;
	}

	Config = InConfig;
	const int32 PoolSize = FMath::Max(1, Config.PoolSize);

	UE_LOG(LogUEMySQL, Log, TEXT("连接池初始化开始: Host=%s, Port=%d, User=%s, Database=%s, Charset=%s, PoolSize=%d"),
		*Config.Host, Config.Port, *Config.User,
		Config.Database.IsEmpty() ? TEXT("(无)") : *Config.Database,
		*Config.CharsetName, PoolSize);

	for (int32 i = 0; i < PoolSize; ++i)
	{
		UE_LOG(LogUEMySQL, Verbose, TEXT("连接池: 正在建立第 %d/%d 个连接..."), i + 1, PoolSize);
		FMySQLConnectionPtr Conn = CreateConnection(OutError);
		if (!Conn.IsValid())
		{
			// 建立失败：回收已建立的连接
			UE_LOG(LogUEMySQL, Error, TEXT("连接池: 第 %d 个连接建立失败,已建立的 %d 个连接将被回收。错误: %s"),
				i + 1, TotalCreated, *OutError);
			Idle.Empty();
			TotalCreated = 0;
			UE_LOG(LogUEMySQL, Error, TEXT("连接池初始化失败：%s"), *OutError);
			return false;
		}
		Idle.Add(Conn);
		++TotalCreated;
		UE_LOG(LogUEMySQL, Verbose, TEXT("连接池: 第 %d/%d 个连接建立成功。"), i + 1, PoolSize);
	}

	bInitialized = true;
	Available->Trigger();
	UE_LOG(LogUEMySQL, Log, TEXT("连接池初始化成功，连接数=%d"), TotalCreated);
	return true;
}

void FMySQLConnectionPool::Shutdown()
{
	FScopeLock Lock(&Mutex);
	Idle.Empty();
	TotalCreated = 0;
	bInitialized = false;
}

FMySQLConnectionPtr FMySQLConnectionPool::Acquire(int32 TimeoutMs)
{
	const double StartTime = FPlatformTime::Seconds();

	while (true)
	{
		{
			FScopeLock Lock(&Mutex);
			if (!bInitialized)
			{
				UE_LOG(LogUEMySQL, Warning, TEXT("Acquire 失败：连接池未初始化。"));
				return nullptr;
			}
			if (Idle.Num() > 0)
			{
				FMySQLConnectionPtr Conn = Idle.Pop(false);
				return Conn;
			}
		}

		// 无空闲连接，等待信号
		if (TimeoutMs <= 0)
		{
			Available->Wait();
		}
		else
		{
			const double Elapsed = (FPlatformTime::Seconds() - StartTime) * 1000.0;
			const int32 Remaining = TimeoutMs - static_cast<int32>(Elapsed);
			if (Remaining <= 0)
			{
				UE_LOG(LogUEMySQL, Warning, TEXT("Acquire 超时（%d ms），无可用连接。"), TimeoutMs);
				return nullptr;
			}
			Available->Wait(static_cast<uint32>(Remaining));
		}
	}
}

void FMySQLConnectionPool::Release(FMySQLConnectionPtr Connection)
{
	if (!Connection.IsValid())
	{
		return;
	}

	// 归还前检测存活，失效则重建
	if (!Connection->IsAlive())
	{
		FString Err;
		FMySQLConnectionPtr NewConn = CreateConnection(Err);
		if (NewConn.IsValid())
		{
			Connection = NewConn;
			UE_LOG(LogUEMySQL, Log, TEXT("归还时检测到失效连接，已重建。"));
		}
		else
		{
			UE_LOG(LogUEMySQL, Error, TEXT("归还时连接失效且重建失败：%s"), *Err);
			// 重建失败也放回（避免池收缩到 0），下次 Acquire 时仍会检测
		}
	}

	{
		FScopeLock Lock(&Mutex);
		Idle.Add(Connection);
	}
	Available->Trigger();
}
