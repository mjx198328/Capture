#include "stdafx.h"
#include "ScreenCapture.h"
#include <process.h>
#include <d3d9.h>

#pragma comment (lib,"d3d9.lib")

//截取全屏
void CaptureScreen(void *data, int w, int h, int nDepth)
{
	//1 创建directx3d对象
	static IDirect3D9 * pd3d = NULL;
	if (!pd3d)
	{
		pd3d = Direct3DCreate9(D3D_SDK_VERSION);
	}
	if (!pd3d) return;

	//2 创建显卡的设备对象
	static IDirect3DDevice9 * pDevice = NULL;
	if (!pDevice)
	{
		D3DPRESENT_PARAMETERS pa;
		ZeroMemory(&pa, sizeof(pa));
		pa.Windowed = true;
		pa.Flags = D3DPRESENTFLAG_LOCKABLE_BACKBUFFER;
		pa.SwapEffect = D3DSWAPEFFECT_DISCARD;
		pa.hDeviceWindow = GetDesktopWindow();
		pd3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, 0,
			D3DCREATE_HARDWARE_VERTEXPROCESSING, &pa, &pDevice
			);
	}
	if (!pDevice)return;

	//3创建离屏表面
	static IDirect3DSurface9 * pSurface = NULL;
	if (!pSurface)
	{
		pDevice->CreateOffscreenPlainSurface(w, h, D3DFMT_A8R8G8B8, D3DPOOL_SCRATCH, &pSurface, 0);
	}
	if (!pSurface)return;

	//4 抓屏
	pDevice->GetFrontBufferData(0, pSurface);

	//5 取出数据
	D3DLOCKED_RECT rect;
	ZeroMemory(&rect, sizeof(rect));
	if (pSurface->LockRect(&rect, 0, 0) != S_OK)
	{
		return;
	}
	memcpy(data, rect.pBits, w * h * nDepth);
	pSurface->UnlockRect();
	printf(".");
}

int CScreenCapture::GetLeftImageSize()
{
	int nSize = 0;
	::EnterCriticalSection(&m_cs);
	nSize = m_CapList.size();
	::LeaveCriticalSection(&m_cs);
	return nSize;
}

UINT __stdcall CScreenCapture::_CaptureThreadProc(LPVOID param)
{
	CScreenCapture * pObject = (CScreenCapture *)param;
	if (!pObject)
		return 0;

	HRESULT hr = 0;

	int nW = GetSystemMetrics(SM_CXSCREEN);
	int nH = GetSystemMetrics(SM_CYSCREEN);
	HDC hDC = ::GetDC(GetDesktopWindow());
	int nDep = GetDeviceCaps(hDC,BITSPIXEL);
	::ReleaseDC(GetDesktopWindow(),hDC);

	unsigned char * pBuf = new unsigned char[nW * nH * nDep];

	if (!pBuf)
		return 0;

	CoInitialize(nullptr);

	SetEvent(pObject->m_hEventStarted);

	while (pObject->m_bStop == false)
	{
		//DWORD s = GetTickCount();

		if (pObject->GetLeftImageSize() > 30)
			continue;

		CaptureScreen(pBuf,nW,nH,4);	

		pObject->PushBack(pBuf, nW, nH, 4);
		
		//DWORD e = GetTickCount();;

		//printf("diff:%d	", e - s);
		Sleep(30);
	}

	CoUninitialize();

	if (pBuf)
	{
		delete[] pBuf;
		pBuf = NULL;
	}

	return 0;
}

bool CScreenCapture::PushBack(unsigned char * pBuf, int nW, int nH, int nDep)
{
	if (!pBuf)
		return false;

	Screen_Data * pData = new Screen_Data;
	if (!pData)
		return false;

	pData->m_nDep = nDep;

	pData->m_nH = nH;

	pData->m_nW = nW;

	pData->m_pData = new unsigned char[nW * nH * nDep];

	if (!pData->m_pData)
	{
		delete pData;
		return false;
	}

	memcpy(pData->m_pData, pBuf, nW * nH * 4);

	::EnterCriticalSection(&m_cs);

	m_CapList.push_back(pData);

	::LeaveCriticalSection(&m_cs);

	return true;
}

Screen_Data* CScreenCapture::GetRGB()
{
	Screen_Data * pData = NULL;
	::EnterCriticalSection(&m_cs);
	//printf("%d\n", m_CapList.size());
	if (m_CapList.empty() == false)
	{
		pData = m_CapList.front();
		m_CapList.pop_front();
		printf("-");
	}
	::LeaveCriticalSection(&m_cs);
	return pData;
}

CScreenCapture::CScreenCapture()
{
	m_bInit = false;
	m_hThreadCapture = NULL;
	::InitializeCriticalSection(&m_cs);
}


CScreenCapture::~CScreenCapture()
{
	Screen_Data * pTmp = NULL;

	for (std::list<Screen_Data *>::iterator it = m_CapList.begin(); it != m_CapList.end(); it++)
	{
		pTmp = *it;
		if (pTmp)
		{
			if (pTmp->m_pData)
				delete[] pTmp->m_pData;
			delete pTmp;
		}
	}

	m_CapList.clear();
	::DeleteCriticalSection(&m_cs);
}

bool CScreenCapture::Init()
{
	if (m_bInit)
		return true;

	ClearList();

	m_hEventStarted = CreateEvent(nullptr, true, false, nullptr);

	if (m_hEventStarted == NULL)
	{
		return false;
	}

	m_bStop = false;

	m_bInit = true;

	return true;
}

bool CScreenCapture::Start()
{
	if (!m_bInit)
		Init();

	if (m_hThreadCapture)
		return true;

	m_hThreadCapture = (HANDLE)_beginthreadex(nullptr, 0, _CaptureThreadProc, this, 0, nullptr);

	if (!m_hThreadCapture)
		return false;

	HANDLE ahWaits[2] = { m_hEventStarted, m_hThreadCapture };
	DWORD dwWaitResult = WaitForMultipleObjects(sizeof(ahWaits) / sizeof(ahWaits[0]), ahWaits, false, INFINITE);
	if (WAIT_OBJECT_0 != dwWaitResult)
	{
		if (m_hThreadCapture)
		{
			CloseHandle(m_hThreadCapture);
			m_hThreadCapture = NULL;
		}

		return false;
	}

	return true;
}

bool CScreenCapture::Stop()
{
	if (m_bInit == false)
		return false;

	m_bStop = true;

	WaitForSingleObject(m_hThreadCapture, INFINITE);

	if (m_hEventStarted)
	{
		CloseHandle(m_hEventStarted);
		m_hEventStarted = NULL;
	}

	if (m_hThreadCapture)
	{
		CloseHandle(m_hThreadCapture);
		m_hThreadCapture = NULL;
	}

	m_bInit = false;

	return true;
}

void CScreenCapture::ClearList()
{
	::EnterCriticalSection(&m_cs);
	for (std::list<Screen_Data *>::iterator it = m_CapList.begin(); it != m_CapList.end(); it++)
	{
		byte * pData = (*it)->m_pData;
		if (pData)
		{
			delete[] pData;
		}
		delete (*it);
	}

	m_CapList.clear();
	::LeaveCriticalSection(&m_cs);
}