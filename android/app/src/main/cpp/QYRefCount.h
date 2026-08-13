/*******************************************************
Copyright (c) 2021 IQIYISMART, Inc. All Rights Reserved.
*******************************************************/
#ifndef _QYREFCOUNT_H_
#define _QYREFCOUNT_H_

class QYRefCount
{
public:
	QYRefCount() : m_uRefCount(0) {}
	~QYRefCount() {}
	
	bool Ref()
	{
		if (m_uRefCount != 0)
		{
			return false;
		}
		else
		{
			m_uRefCount++;
			return true;
		}
	}
	bool UnRef()
	{
		if (m_uRefCount == 0)
		{
			return false;
		}
		else
		{
			m_uRefCount--;
			return true;
		}
	}
protected:
	unsigned int m_uRefCount;
};

#endif//_QYREFCOUNT_H_