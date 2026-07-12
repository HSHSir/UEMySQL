// Copyright NetGame. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UEMySQLTypes.generated.h"

/**
 * MySQL 连接配置。
 * 注意：CharsetName 默认 "utf8mb4"，用于 mysql_options(MYSQL_SET_CHARSET_NAME)，
 * 从连接建立起就统一 UTF-8，避免中文乱码。
 */
USTRUCT(BlueprintType)
struct FMySQLConnectionConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MySQL")
	FString Host = TEXT("127.0.0.1");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MySQL")
	int32 Port = 3306;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MySQL")
	FString User = TEXT("root");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MySQL")
	FString Password = TEXT("root");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MySQL")
	FString Database = TEXT("");

	/** 连接字符集，默认 utf8mb4（完整 UTF-8，支持 emoji），防止中文乱码。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MySQL")
	FString CharsetName = TEXT("utf8mb4");

	/** 连接超时（秒） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MySQL")
	int32 ConnectTimeoutSeconds = 5;

	/** 断线自动重连 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MySQL")
	bool bAutoReconnect = true;

	/** 连接池大小（连接池模式下生效） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MySQL")
	int32 PoolSize = 4;
};

/** 单行数据：列名 -> 值（值统一以 FString 存储，UTF-8 已转换为 TCHAR）。 */
USTRUCT(BlueprintType)
struct FMySQLRow
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "MySQL")
	TMap<FString, FString> Fields;

	/** 记录 NULL 字段（因为 TMap 无法区分空字符串与 NULL） */
	TSet<FString> NullFields;

	const FString& Get(const FString& Column) const
	{
		static const FString Empty;
		const FString* Found = Fields.Find(Column);
		return Found ? *Found : Empty;
	}

	bool IsNull(const FString& Column) const
	{
		return NullFields.Contains(Column);
	}
};

/** 查询/执行结果。 */
USTRUCT(BlueprintType)
struct FMySQLResult
{
	GENERATED_BODY()

	/** 是否成功 */
	UPROPERTY(BlueprintReadOnly, Category = "MySQL")
	bool bSuccess = false;

	/** 错误码（0 表示无错误，其余为 mysql_errno） */
	UPROPERTY(BlueprintReadOnly, Category = "MySQL")
	int32 ErrorCode = 0;

	/** 错误信息 */
	UPROPERTY(BlueprintReadOnly, Category = "MySQL")
	FString ErrorMessage;

	/** 列名（有序，SELECT 时有效） */
	UPROPERTY(BlueprintReadOnly, Category = "MySQL")
	TArray<FString> ColumnNames;

	/** 结果集行（SELECT 时有效） */
	UPROPERTY(BlueprintReadOnly, Category = "MySQL")
	TArray<FMySQLRow> Rows;

	/** 受影响行数（INSERT/UPDATE/DELETE 时有效） */
	UPROPERTY(BlueprintReadOnly, Category = "MySQL")
	int64 AffectedRows = 0;

	/** 最后插入的自增 ID（INSERT 时有效） */
	UPROPERTY(BlueprintReadOnly, Category = "MySQL")
	int64 LastInsertId = 0;

	int32 NumRows() const { return Rows.Num(); }
};

/** 预处理语句的参数值类型。用于 FMySQLParam 打包。 */
enum class EMySQLParamType : uint8
{
	Null,
	Int64,
	Double,
	String,   // UTF-8 文本
	Blob      // 二进制
};

/** 预处理语句参数（防注入）。使用工厂函数构造。 */
struct FMySQLParam
{
	EMySQLParamType Type = EMySQLParamType::Null;
	int64  IntValue = 0;
	double DoubleValue = 0.0;
	FString StringValue;          // 保存 FString（绑定时再转 UTF-8）
	TArray<uint8> BlobValue;

	static FMySQLParam MakeNull()               { return FMySQLParam(); }
	static FMySQLParam MakeInt(int64 V)         { FMySQLParam P; P.Type = EMySQLParamType::Int64;  P.IntValue = V;    return P; }
	static FMySQLParam MakeDouble(double V)     { FMySQLParam P; P.Type = EMySQLParamType::Double; P.DoubleValue = V; return P; }
	static FMySQLParam MakeString(const FString& V) { FMySQLParam P; P.Type = EMySQLParamType::String; P.StringValue = V; return P; }
	static FMySQLParam MakeBlob(const TArray<uint8>& V) { FMySQLParam P; P.Type = EMySQLParamType::Blob; P.BlobValue = V; return P; }
};
