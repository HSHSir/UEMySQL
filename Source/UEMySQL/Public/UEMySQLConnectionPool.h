// Copyright NetGame. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UEMySQLTypes.h"
#include "UEMySQLConnection.h"
#include "HAL/CriticalSection.h"
#include "HAL/Event.h"

/**
 * 线程安全的 MySQL 连接池。
 *
 * 用法：
 *   Pool->Initialize(Config);
 *   {
 *       FMySQLScopedConnection Conn(Pool);   // RAII 借出连接
 *       if (Conn.IsValid()) Conn->ExecutePrepared(...);
 *   }                                        // 作用域结束自动归还
 *
 * 归还时会做存活检测（mysql_ping），失效连接会被重建。
 */
class UEMYSQL_API FMySQLConnectionPool : public TSharedFromThis<FMySQLConnectionPool, ESPMode::ThreadSafe>
{
public:
	FMySQLConnectionPool();
	~FMySQLConnectionPool();

	/** 初始化连接池（会立即建立 Config.PoolSize 个连接）。返回是否全部成功。 */
	bool Initialize(const FMySQLConnectionConfig& InConfig, FString& OutError);

	/** 关闭池，释放所有连接。 */
	void Shutdown();

	/**
	 * 从池中借出一个连接（阻塞直到有可用连接或超时）。
	 * @param TimeoutMs  等待超时毫秒；<=0 表示无限等待。
	 * @return 可用连接；超时返回空指针。用完必须调用 Release 归还。
	 */
	FMySQLConnectionPtr Acquire(int32 TimeoutMs = 5000);

	/** 归还连接到池。 */
	void Release(FMySQLConnectionPtr Connection);

	bool IsInitialized() const { return bInitialized; }
	const FMySQLConnectionConfig& GetConfig() const { return Config; }

private:
	FMySQLConnectionPtr CreateConnection(FString& OutError);

private:
	FMySQLConnectionConfig Config;
	TArray<FMySQLConnectionPtr> Idle;      // 空闲连接
	int32 TotalCreated = 0;
	FCriticalSection Mutex;
	FEvent* Available = nullptr;           // 有连接可用的信号
	bool bInitialized = false;
};

typedef TSharedPtr<FMySQLConnectionPool, ESPMode::ThreadSafe> FMySQLConnectionPoolPtr;

/** RAII 作用域连接：构造时借出，析构时自动归还。 */
class UEMYSQL_API FMySQLScopedConnection
{
public:
	explicit FMySQLScopedConnection(const FMySQLConnectionPoolPtr& InPool, int32 TimeoutMs = 5000)
		: Pool(InPool)
	{
		if (Pool.IsValid())
		{
			Conn = Pool->Acquire(TimeoutMs);
		}
	}

	~FMySQLScopedConnection()
	{
		if (Pool.IsValid() && Conn.IsValid())
		{
			Pool->Release(Conn);
		}
	}

	FMySQLScopedConnection(const FMySQLScopedConnection&) = delete;
	FMySQLScopedConnection& operator=(const FMySQLScopedConnection&) = delete;

	bool IsValid() const { return Conn.IsValid(); }
	FMySQLConnection* operator->() const { return Conn.Get(); }
	FMySQLConnection* Get() const { return Conn.Get(); }

private:
	FMySQLConnectionPoolPtr Pool;
	FMySQLConnectionPtr Conn;
};
