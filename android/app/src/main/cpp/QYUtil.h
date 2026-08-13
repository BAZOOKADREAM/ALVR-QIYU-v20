/*******************************************************
Copyright (c) 2021 IQIYISMART, Inc. All Rights Reserved.
*******************************************************/
#ifndef _QYUTIL_H_
#define _QYUTIL_H_

#include <android/log.h>

namespace QY
{
	void CheckGlError(const char* file, int line);
	void CheckEglError(const char* file, int line);
};

#define CHECK_GL_ 1
#if CHECK_GL_
#define QY_GL(func) func; QY::CheckGlError(__FILE__, __LINE__);
#define QY_EGL(func) func; QY::CheckEglError(__FILE__, __LINE__);
#else
#define QY_GL(func) func;
#define QY_EGL(func) func;
#endif


#endif//_QYUTIL_H_