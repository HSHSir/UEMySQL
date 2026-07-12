// Copyright NetGame. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UEMySQLTypes.h"

// 前置声明，避免在公共头文件暴露 mysql.h（防止污染 UE 全局命名空间）
typedef struct st_mysql MYSQL;

/**
 * 单个 MySQL 连接的 RAII 封装。
 *
 * 线程安全说明：单个 FMySQLConnection 实例【非】线程安全，
 * 同一时刻只能被一个线程使用（由连接池保证独占）。
 *
 * 防乱码：连接建立时通过 mysql_options(MYSQL_SET_CHARSET_NAME) 与
 * mysql_set_character_set 双重设置字符集为 utf8mb4；所有 FString 与
 * C 字符串之间通过 FTCHARToUTF8 / UTF8_TO_TCHAR 转换，全程 UTF-8。
 */
class UEMYSQLCORE_API FMySQLConnection
{
public:
	FMySQLConnection();
	~FMySQLConnection();

	FMySQLConnection(const FMySQLConnection&) = delete;
	FMySQLConnection& operator=(const FMySQLConnection&) = delete;

	/** 建立连接。返回是否成功；失败时 OutError 填充错误信息。 */
	bool Connect(const FMySQLConnectionConfig& InConfig, FString& OutError);

	/** 关闭连接。 */
	void Close();

	/** 是否处于已连接状态（会执行 mysql_ping 检测并按需重连）。 */
	bool IsAlive();

	/**
	 * 直接执行 SQL（不带参数）。适合 DDL 或已自行转义的语句。
	 * 警告：拼接用户输入有 SQL 注入风险，优先使用 ExecutePrepared。
	 */
	FMySQLResult Execute(const FString& SQL);

	/**
	 * 执行预处理语句（防 SQL 注入）。
	 * @param SQL     含 ? 占位符的语句，例如 "SELECT * FROM t WHERE name=? AND age>?"
	 * @param Params  与占位符顺序一致的参数数组
	 */
	FMySQLResult ExecutePrepared(const FString& SQL, const TArray<FMySQLParam>& Params);

	/** 使用 mysql_real_escape_string 转义字符串（拼 SQL 兜底用）。 */
	FString EscapeString(const FString& In) const;

	const FMySQLConnectionConfig& GetConfig() const { return Config; }

private:
	/** 应用字符集设置（防乱码核心） */
	bool ApplyCharset();

	/** 从普通查询（mysql_store_result）解析结果集 */
	FMySQLResult ParseStoreResult();

	/** 填充错误信息到 Result */
	void FillError(FMySQLResult& Result) const;

private:
	MYSQL* Connection = nullptr;
	FMySQLConnectionConfig Config;
	bool bConnected = false;
};

typedef TSharedPtr<FMySQLConnection, ESPMode::ThreadSafe> FMySQLConnectionPtr;
