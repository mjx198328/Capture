#pragma once
#include <windows.h>
#include <windowsx.h>
#include <list>


typedef struct
{
	unsigned char * m_pData;
	int m_nW;
	int m_nH;
	int m_nDep;
}Screen_Data, * ScreenDataPtr;

class CScreenCapture
{
public:
	CScreenCapture();
	~CScreenCapture();
	bool Start();
	bool Stop();

	HANDLE m_hThreadCapture;

	static UINT __stdcall _CaptureThreadProc(LPVOID param);

	bool Init();

	void ClearList();

	HANDLE m_hEventStarted;

	bool PushBack(unsigned char * pBuf, int nW, int nH, int nDep);

	Screen_Data* GetRGB();

	int GetLeftImageSize();

protected:
	std::list<Screen_Data *> m_CapList;
	bool m_bStop;
	bool m_bInit;
	CRITICAL_SECTION m_cs;

};

