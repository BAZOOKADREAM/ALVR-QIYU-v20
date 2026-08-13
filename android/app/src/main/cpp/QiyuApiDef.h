/*******************************************************
Copyright (c) 2021 IQIYISMART, Inc. All Rights Reserved.
*******************************************************/
#ifndef _QIYUAPIDEF_H_
#define _QIYUAPIDEF_H_


static const char* PResultCode_Success = "S0000";

struct PResult_Init
{
	const char* code = "";
	bool IsSuccess() const { return (0 == strcmp(code, PResultCode_Success)); }
};
typedef void(*PCallback_Init)(const PResult_Init&);

struct PResult_AccountInfo
{
	const char* uid = "";
	const char* name = "";
	const char* icon = "";
	const char* code = "";
	bool IsSuccess() const { return (0 == strcmp(code, PResultCode_Success)); }
};
typedef void(*PCallback_GetAccountInfo)(const PResult_AccountInfo&);

struct PResult_DeepLink
{
	const char* appID = "";
	const char* key = "";
	const char* value = "";
	const char* code = "";
	bool IsSuccess() const { return (0 == strcmp(code, PResultCode_Success)); }
};
typedef void(*PCallback_GetDeepLink)(const PResult_DeepLink&);


#endif//_QIYUAPIDEF_H_