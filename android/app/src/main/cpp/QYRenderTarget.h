/*******************************************************
Copyright (c) 2021 IQIYISMART, Inc. All Rights Reserved.
*******************************************************/
#ifndef _QYRENDERTARGET_H_
#define _QYRENDERTARGET_H_

#include "QYRefCount.h"

namespace QY_GL_EXT
{
	bool InitFunction_MultiView();
	bool IsSupport_MultiView();
}

class QYRenderTarget : public QYRefCount
{
public:
	QYRenderTarget();
	~QYRenderTarget();

	bool Init(bool isSupport_Foveation, GLuint foveationFeatureBits, bool isMultiView, int width, int height, int samples, int colorSizedFormat, bool requireDepth, bool isProtectedContent/* = false*/);
	void Release();

	bool Bind();
	bool UnBind();

	unsigned int GetColorAttachment() const { return m_idColor; }
	
protected:
	bool Init_SingleSample();
	bool Init_MultiSampleEXT();
	void Init_genColorDepth();
	bool Init_MultiView();

protected:
	bool			m_bInit;

	GLsizei         m_nWidth;
	GLsizei			m_nHeight;
	int             m_nSamples;
	bool 			m_bIsProtectedContent;
	bool            m_bRequireDepth;
	bool			m_bIsMultiView;
	GLenum			m_colorSizedFormat;
	GLenum			m_format;
	GLenum			m_type;

	unsigned int 	m_idColor;
	unsigned int 	m_idDepth;
	unsigned int 	m_idFBO;
};


#endif//_QYRENDERTARGET_H_