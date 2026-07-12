// Copyright NetGame. All Rights Reserved.
#include "UEMySQLConnection.h"
#include "UEMySQLModule.h"

#if WITH_UEMYSQL
// mysql.h 会定义一些与 UE 冲突的宏，用 THIRD_PARTY_INCLUDES_START 包裹隔离
THIRD_PARTY_INCLUDES_START
#include <mysql.h>
THIRD_PARTY_INCLUDES_END
#endif

// ---------- UTF-8 <-> FString 辅助（防乱码核心） ----------
namespace UEMySQLUtil
{
	// FString(TCHAR) -> UTF-8 std 字节串，供 mysql C API 使用
	static FTCHARToUTF8 ToUtf8(const FString& In)
	{
		return FTCHARToUTF8(*In);
	}

	// UTF-8 字节 -> FString。Len 为字节数（可能含中文多字节）
	static FString FromUtf8(const char* Data, unsigned long Len)
	{
		if (!Data || Len == 0)
		{
			return FString();
		}
		// UTF8_TO_TCHAR 需要以 \0 结尾或指定长度，这里用 FUTF8ToTCHAR 指定字节数
		FUTF8ToTCHAR Conv(Data, static_cast<int32>(Len));
		return FString(Conv.Length(), Conv.Get());
	}
}

FMySQLConnection::FMySQLConnection()
{
}

FMySQLConnection::~FMySQLConnection()
{
	Close();
}

bool FMySQLConnection::Connect(const FMySQLConnectionConfig& InConfig, FString& OutError)
{
#if !WITH_UEMYSQL
	OutError = TEXT("当前平台未启用 UEMySQL（仅支持 Win64）。");
	return false;
#else
	Config = InConfig;
	Close();

	UE_LOG(LogUEMySQL, Verbose, TEXT("FMySQLConnection::Connect: Host=%s, Port=%d, User=%s, Database=%s, Charset=%s, Timeout=%ds, bAutoReconnect=%d"),
		*Config.Host, Config.Port, *Config.User,
		Config.Database.IsEmpty() ? TEXT("(无)") : *Config.Database,
		*Config.CharsetName, Config.ConnectTimeoutSeconds, Config.bAutoReconnect ? 1 : 0);

	Connection = mysql_init(nullptr);
	if (!Connection)
	{
		OutError = TEXT("mysql_init 失败（内存不足？）。");
		UE_LOG(LogUEMySQL, Error, TEXT("%s"), *OutError);
		return false;
	}

	// 连接超时
	unsigned int Timeout = static_cast<unsigned int>(FMath::Max(1, Config.ConnectTimeoutSeconds));
	mysql_options(Connection, MYSQL_OPT_CONNECT_TIMEOUT, &Timeout);

	// 自动重连
	my_bool Reconnect = Config.bAutoReconnect ? 1 : 0;
	mysql_options(Connection, MYSQL_OPT_RECONNECT, &Reconnect);

	// 【防乱码 1】连接前设置默认字符集，握手阶段即协商 UTF-8
	{
		FTCHARToUTF8 CharsetUtf8 = UEMySQLUtil::ToUtf8(Config.CharsetName);
		mysql_options(Connection, MYSQL_SET_CHARSET_NAME, CharsetUtf8.Get());
	}

	FTCHARToUTF8 HostU = UEMySQLUtil::ToUtf8(Config.Host);
	FTCHARToUTF8 UserU = UEMySQLUtil::ToUtf8(Config.User);
	FTCHARToUTF8 PassU = UEMySQLUtil::ToUtf8(Config.Password);
	FTCHARToUTF8 DbU   = UEMySQLUtil::ToUtf8(Config.Database);

	UE_LOG(LogUEMySQL, Verbose, TEXT("FMySQLConnection::Connect: 调用 mysql_real_connect..."));
	MYSQL* Ret = mysql_real_connect(
		Connection,
		HostU.Get(),
		UserU.Get(),
		PassU.Get(),
		Config.Database.IsEmpty() ? nullptr : DbU.Get(),
		static_cast<unsigned int>(Config.Port),
		nullptr,
		CLIENT_MULTI_STATEMENTS);

	if (!Ret)
	{
		const unsigned int ErrNo = mysql_errno(Connection);
		FString ErrMsg = UTF8_TO_TCHAR(mysql_error(Connection));
		OutError = FString::Printf(TEXT("连接失败 [%d]: %s"), ErrNo, *ErrMsg);
		UE_LOG(LogUEMySQL, Error, TEXT("FMySQLConnection::Connect: mysql_real_connect 失败 [%d]: %s (Host=%s, Port=%d, User=%s, Database=%s)"),
			ErrNo, *ErrMsg, *Config.Host, Config.Port, *Config.User,
			Config.Database.IsEmpty() ? TEXT("(无)") : *Config.Database);
		mysql_close(Connection);
		Connection = nullptr;
		return false;
	}

	bConnected = true;
	UE_LOG(LogUEMySQL, Verbose, TEXT("FMySQLConnection::Connect: mysql_real_connect 成功,server=%s,client=%s"),
		UTF8_TO_TCHAR(mysql_get_server_info(Connection)),
		UTF8_TO_TCHAR(mysql_get_client_info()));

	// 【防乱码 2】连接后再次显式设置连接字符集（等价于 SET NAMES utf8mb4）
	if (!ApplyCharset())
	{
		const unsigned int ErrNo = mysql_errno(Connection);
		FString ErrMsg = UTF8_TO_TCHAR(mysql_error(Connection));
		OutError = FString::Printf(TEXT("设置字符集失败 [%d]: %s"), ErrNo, *ErrMsg);
		UE_LOG(LogUEMySQL, Error, TEXT("FMySQLConnection::Connect: ApplyCharset(%s) 失败 [%d]: %s"),
			*Config.CharsetName, ErrNo, *ErrMsg);
		Close();
		return false;
	}

	UE_LOG(LogUEMySQL, Log, TEXT("已连接 MySQL %s:%d/%s (charset=%s)"),
		*Config.Host, Config.Port, *Config.Database, *Config.CharsetName);
	return true;
#endif
}

bool FMySQLConnection::ApplyCharset()
{
#if WITH_UEMYSQL
	if (!Connection) return false;
	FTCHARToUTF8 CharsetUtf8 = UEMySQLUtil::ToUtf8(Config.CharsetName);
	// mysql_set_character_set 内部会执行 SET NAMES 并更新客户端字符集
	return mysql_set_character_set(Connection, CharsetUtf8.Get()) == 0;
#else
	return false;
#endif
}

void FMySQLConnection::Close()
{
#if WITH_UEMYSQL
	if (Connection)
	{
		mysql_close(Connection);
		Connection = nullptr;
	}
#endif
	bConnected = false;
}

bool FMySQLConnection::IsAlive()
{
#if WITH_UEMYSQL
	if (!Connection || !bConnected)
	{
		return false;
	}
	// mysql_ping 在开启自动重连时会尝试重连
	if (mysql_ping(Connection) != 0)
	{
		bConnected = false;
		return false;
	}
	return true;
#else
	return false;
#endif
}

FString FMySQLConnection::EscapeString(const FString& In) const
{
#if WITH_UEMYSQL
	if (!Connection)
	{
		return In;
	}
	FTCHARToUTF8 SrcU(*In);
	const char* Src = SrcU.Get();
	unsigned long SrcLen = static_cast<unsigned long>(SrcU.Length());

	TArray<char> Buffer;
	Buffer.SetNumUninitialized(SrcLen * 2 + 1);
	unsigned long OutLen = mysql_real_escape_string(Connection, Buffer.GetData(), Src, SrcLen);
	return UEMySQLUtil::FromUtf8(Buffer.GetData(), OutLen);
#else
	return In;
#endif
}

void FMySQLConnection::FillError(FMySQLResult& Result) const
{
#if WITH_UEMYSQL
	if (Connection)
	{
		Result.bSuccess = false;
		Result.ErrorCode = static_cast<int32>(mysql_errno(Connection));
		Result.ErrorMessage = UTF8_TO_TCHAR(mysql_error(Connection));
	}
#endif
}

FMySQLResult FMySQLConnection::Execute(const FString& SQL)
{
	FMySQLResult Result;
#if WITH_UEMYSQL
	if (!Connection)
	{
		Result.ErrorMessage = TEXT("连接未建立。");
		return Result;
	}

	FTCHARToUTF8 SqlU(*SQL);
	if (mysql_real_query(Connection, SqlU.Get(), static_cast<unsigned long>(SqlU.Length())) != 0)
	{
		FillError(Result);
		UE_LOG(LogUEMySQL, Error, TEXT("Execute 失败 [%d]: %s | SQL=%s"),
			Result.ErrorCode, *Result.ErrorMessage, *SQL);
		return Result;
	}

	// 判断是否有结果集（ParseStoreResult 内部会调用 mysql_store_result 并负责释放）
	Result = ParseStoreResult();

	// 多结果集排空：CLIENT_MULTI_STATEMENTS 下存储过程/批量语句可能产生多个结果集，
	// 必须全部消费，否则后续查询会报 "Commands out of sync" 使连接损坏。
	DrainRemainingResults(Result);
#else
	Result.ErrorMessage = TEXT("UEMySQL 未启用。");
#endif
	return Result;
}

void FMySQLConnection::DrainRemainingResults(FMySQLResult& Result)
{
#if WITH_UEMYSQL
	if (!Connection) return;

	int NextStatus = 0;
	// mysql_next_result 返回：0=还有结果集；-1=没有更多；>0=出错
	while ((NextStatus = mysql_next_result(Connection)) == 0)
	{
		// 读取并立即释放后续结果集（数据丢弃，仅维持协议同步）
		MYSQL_RES* Extra = mysql_store_result(Connection);
		if (Extra)
		{
			mysql_free_result(Extra);
		}
		// 若 Extra 为空且 field_count>0 说明该结果集出错，但继续推进以避免连接锁死
	}

	if (NextStatus > 0)
	{
		// 排空过程出现错误，覆盖错误信息（首个结果集的成功状态不再可信）
		Result.bSuccess = false;
		Result.ErrorCode = static_cast<int32>(mysql_errno(Connection));
		Result.ErrorMessage = UTF8_TO_TCHAR(mysql_error(Connection));
		UE_LOG(LogUEMySQL, Error, TEXT("Execute 多结果集排空失败 [%d]: %s"),
			Result.ErrorCode, *Result.ErrorMessage);
	}
#else
	(void)Result;
#endif
}

FMySQLResult FMySQLConnection::ParseStoreResult()
{
	FMySQLResult Result;
#if WITH_UEMYSQL
	MYSQL_RES* Res = mysql_store_result(Connection);
	if (!Res)
	{
		if (mysql_field_count(Connection) == 0)
		{
			Result.bSuccess = true;
			Result.AffectedRows = static_cast<int64>(mysql_affected_rows(Connection));
			Result.LastInsertId = static_cast<int64>(mysql_insert_id(Connection));
		}
		else
		{
			FillError(Result);
		}
		return Result;
	}

	unsigned int NumFields = mysql_num_fields(Res);
	MYSQL_FIELD* Fields = mysql_fetch_fields(Res);

	Result.ColumnNames.Reserve(NumFields);
	for (unsigned int i = 0; i < NumFields; ++i)
	{
		Result.ColumnNames.Add(UTF8_TO_TCHAR(Fields[i].name));
	}

	MYSQL_ROW RowData;
	while ((RowData = mysql_fetch_row(Res)) != nullptr)
	{
		unsigned long* Lengths = mysql_fetch_lengths(Res);
		FMySQLRow Row;
		for (unsigned int i = 0; i < NumFields; ++i)
		{
			const FString& ColName = Result.ColumnNames[i];
			if (RowData[i] == nullptr)
			{
				Row.NullFields.Add(ColName);
				Row.Fields.Add(ColName, FString());
			}
			else
			{
				// 【防乱码 3】按字节长度做 UTF-8 -> TCHAR 转换，正确处理中文
				Row.Fields.Add(ColName, UEMySQLUtil::FromUtf8(RowData[i], Lengths[i]));
			}
		}
		Result.Rows.Add(MoveTemp(Row));
	}

	mysql_free_result(Res);
	Result.bSuccess = true;
#endif
	return Result;
}

// ---------------- 预处理语句 ----------------
FMySQLResult FMySQLConnection::ExecutePrepared(const FString& SQL, const TArray<FMySQLParam>& Params)
{
	FMySQLResult Result;
#if WITH_UEMYSQL
	if (!Connection)
	{
		Result.ErrorMessage = TEXT("连接未建立。");
		return Result;
	}

	MYSQL_STMT* Stmt = mysql_stmt_init(Connection);
	if (!Stmt)
	{
		FillError(Result);
		return Result;
	}

	// 确保析构时释放 stmt
	struct FStmtGuard
	{
		MYSQL_STMT* S;
		~FStmtGuard() { if (S) mysql_stmt_close(S); }
	} Guard{ Stmt };

	FTCHARToUTF8 SqlU(*SQL);
	if (mysql_stmt_prepare(Stmt, SqlU.Get(), static_cast<unsigned long>(SqlU.Length())) != 0)
	{
		Result.ErrorCode = static_cast<int32>(mysql_stmt_errno(Stmt));
		Result.ErrorMessage = UTF8_TO_TCHAR(mysql_stmt_error(Stmt));
		UE_LOG(LogUEMySQL, Error, TEXT("Prepare 失败 [%d]: %s | SQL=%s"), Result.ErrorCode, *Result.ErrorMessage, *SQL);
		return Result;
	}

	unsigned long ParamCount = mysql_stmt_param_count(Stmt);
	if (ParamCount != static_cast<unsigned long>(Params.Num()))
	{
		Result.ErrorMessage = FString::Printf(TEXT("参数数量不匹配：SQL 需要 %lu 个，实际提供 %d 个。"), ParamCount, Params.Num());
		return Result;
	}

	// 绑定参数。需要保持 UTF-8 缓冲与长度变量在 execute 期间存活
	TArray<MYSQL_BIND> Binds;
	Binds.SetNumZeroed(Params.Num());
	TArray<TArray<uint8>> Utf8Buffers; // 保存字符串/blob 的字节
	Utf8Buffers.SetNum(Params.Num());
	TArray<unsigned long> Lengths;
	Lengths.SetNumZeroed(Params.Num());
	TArray<my_bool> IsNullFlags;
	IsNullFlags.SetNumZeroed(Params.Num());

	for (int32 i = 0; i < Params.Num(); ++i)
	{
		const FMySQLParam& P = Params[i];
		MYSQL_BIND& B = Binds[i];
		switch (P.Type)
		{
		case EMySQLParamType::Null:
			B.buffer_type = MYSQL_TYPE_NULL;
			IsNullFlags[i] = 1;
			B.is_null = &IsNullFlags[i];
			break;
		case EMySQLParamType::Int64:
			B.buffer_type = MYSQL_TYPE_LONGLONG;
			B.buffer = (void*)&Params[i].IntValue;
			B.is_unsigned = 0;
			break;
		case EMySQLParamType::Double:
			B.buffer_type = MYSQL_TYPE_DOUBLE;
			B.buffer = (void*)&Params[i].DoubleValue;
			break;
		case EMySQLParamType::String:
		{
			// 转 UTF-8 存入 buffer（防乱码）
			FTCHARToUTF8 Conv(*P.StringValue);
			Utf8Buffers[i].Append((const uint8*)Conv.Get(), Conv.Length());
			Lengths[i] = static_cast<unsigned long>(Utf8Buffers[i].Num());
			B.buffer_type = MYSQL_TYPE_STRING;
			B.buffer = Utf8Buffers[i].GetData();
			B.buffer_length = Lengths[i];
			B.length = &Lengths[i];
			break;
		}
		case EMySQLParamType::Blob:
			Utf8Buffers[i] = P.BlobValue;
			Lengths[i] = static_cast<unsigned long>(Utf8Buffers[i].Num());
			B.buffer_type = MYSQL_TYPE_BLOB;
			B.buffer = Utf8Buffers[i].GetData();
			B.buffer_length = Lengths[i];
			B.length = &Lengths[i];
			break;
		}
	}

	if (Params.Num() > 0 && mysql_stmt_bind_param(Stmt, Binds.GetData()) != 0)
	{
		Result.ErrorCode = static_cast<int32>(mysql_stmt_errno(Stmt));
		Result.ErrorMessage = UTF8_TO_TCHAR(mysql_stmt_error(Stmt));
		return Result;
	}

	if (mysql_stmt_execute(Stmt) != 0)
	{
		Result.ErrorCode = static_cast<int32>(mysql_stmt_errno(Stmt));
		Result.ErrorMessage = UTF8_TO_TCHAR(mysql_stmt_error(Stmt));
		UE_LOG(LogUEMySQL, Error, TEXT("Execute(prepared) 失败 [%d]: %s | SQL=%s"), Result.ErrorCode, *Result.ErrorMessage, *SQL);
		return Result;
	}

	// 判断是否有结果集
	MYSQL_RES* MetaRes = mysql_stmt_result_metadata(Stmt);
	if (!MetaRes)
	{
		// 无结果集：INSERT/UPDATE/DELETE
		Result.bSuccess = true;
		Result.AffectedRows = static_cast<int64>(mysql_stmt_affected_rows(Stmt));
		Result.LastInsertId = static_cast<int64>(mysql_stmt_insert_id(Stmt));
		return Result;
	}

	// 有结果集：读取列信息
	unsigned int NumFields = mysql_num_fields(MetaRes);
	MYSQL_FIELD* FieldPtrs = mysql_fetch_fields(MetaRes);
	Result.ColumnNames.Reserve(NumFields);
	for (unsigned int i = 0; i < NumFields; ++i)
	{
		Result.ColumnNames.Add(UTF8_TO_TCHAR(FieldPtrs[i].name));
	}

	// 为每列准备结果绑定缓冲
	TArray<MYSQL_BIND> ResBinds;
	ResBinds.SetNumZeroed(NumFields);
	TArray<TArray<char>> ResBuffers;
	ResBuffers.SetNum(NumFields);
	TArray<unsigned long> ResLengths;
	ResLengths.SetNumZeroed(NumFields);
	TArray<my_bool> ResIsNull;
	ResIsNull.SetNumZeroed(NumFields);
	TArray<my_bool> ResError;
	ResError.SetNumZeroed(NumFields);

	for (unsigned int i = 0; i < NumFields; ++i)
	{
		// 初始给一段缓冲，不够时按实际长度重取
		unsigned long InitLen = FieldPtrs[i].length > 0 && FieldPtrs[i].length < 65535 ? FieldPtrs[i].length + 1 : 256;
		ResBuffers[i].SetNumZeroed(static_cast<int32>(InitLen));
		ResBinds[i].buffer_type = MYSQL_TYPE_STRING; // 统一以字符串取回，便于 UTF-8 处理
		ResBinds[i].buffer = ResBuffers[i].GetData();
		ResBinds[i].buffer_length = InitLen;
		ResBinds[i].length = &ResLengths[i];
		ResBinds[i].is_null = &ResIsNull[i];
		ResBinds[i].error = &ResError[i];
	}

	if (mysql_stmt_bind_result(Stmt, ResBinds.GetData()) != 0)
	{
		Result.ErrorCode = static_cast<int32>(mysql_stmt_errno(Stmt));
		Result.ErrorMessage = UTF8_TO_TCHAR(mysql_stmt_error(Stmt));
		mysql_free_result(MetaRes);
		return Result;
	}

	while (true)
	{
		int FetchRet = mysql_stmt_fetch(Stmt);
		if (FetchRet == MYSQL_NO_DATA) break;
		if (FetchRet == 1)
		{
			Result.ErrorCode = static_cast<int32>(mysql_stmt_errno(Stmt));
			Result.ErrorMessage = UTF8_TO_TCHAR(mysql_stmt_error(Stmt));
			mysql_free_result(MetaRes);
			return Result;
		}
		// FetchRet 为 0（成功）或 MYSQL_DATA_TRUNCATED（需按实际长度重取）
		FMySQLRow Row;
		for (unsigned int i = 0; i < NumFields; ++i)
		{
			const FString& ColName = Result.ColumnNames[i];
			if (ResIsNull[i])
			{
				Row.NullFields.Add(ColName);
				Row.Fields.Add(ColName, FString());
				continue;
			}

			unsigned long ActualLen = ResLengths[i];
			if (ActualLen > (unsigned long)ResBuffers[i].Num())
			{
				// 缓冲不够，扩容后用 mysql_stmt_fetch_column 单独取该列
				ResBuffers[i].SetNumZeroed(static_cast<int32>(ActualLen) + 1);
				MYSQL_BIND ColBind;
				FMemory::Memzero(&ColBind, sizeof(ColBind));
				ColBind.buffer_type = MYSQL_TYPE_STRING;
				ColBind.buffer = ResBuffers[i].GetData();
				ColBind.buffer_length = ActualLen;
				ColBind.length = &ResLengths[i];
				ColBind.is_null = &ResIsNull[i];
				mysql_stmt_fetch_column(Stmt, &ColBind, i, 0);
			}
			Row.Fields.Add(ColName, UEMySQLUtil::FromUtf8(ResBuffers[i].GetData(), ActualLen));
		}
		Result.Rows.Add(MoveTemp(Row));
	}

	mysql_free_result(MetaRes);
	Result.bSuccess = true;
#else
	Result.ErrorMessage = TEXT("UEMySQL 未启用。");
#endif
	return Result;
}
