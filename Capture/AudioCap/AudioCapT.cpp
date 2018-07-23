#include "stdafx.h"
#include "AudioCapT.h"

#define REFTIMES_PER_SEC  10000000
#define REFTIMES_PER_MILLISEC  10000

#pragma comment(lib, "Avrt.lib")

CAudioCapT::CAudioCapT()
{
	m_dwDataSize = 0;
	m_bInit = false;
	m_hThreadCapture = NULL;
	m_bStop = false;
	::InitializeCriticalSection(&m_cs);
}


CAudioCapT::~CAudioCapT()
{
	Audio_Data * pTmp = NULL;

	for (std::list<Audio_Data *>::iterator it = m_al.begin(); it != m_al.end(); it++)
	{
		pTmp = *it;
		if (pTmp)
		{
			if (pTmp->pData)
				delete[] pTmp->pData;
			delete pTmp;
		}
	}

	m_al.clear();
	::DeleteCriticalSection(&m_cs);
}

void CAudioCapT::SetDeiveType(int nType)
{
	m_nDeviceType = nType;
}


int CAudioCapT::GetDeviceType()
{
	return m_nDeviceType;
}

void CAudioCapT::OnCaptureData(LPBYTE pData, INT iDataLen)
{
	Audio_Data * pItem = NULL;

	if (!pData)
		return;

	pItem = new Audio_Data();
	if (!pItem)
		return;

	pItem->iDataLen = iDataLen;
	pItem->pData = new (std::nothrow) BYTE[iDataLen];

	if (pItem->pData)
	{
		memcpy_s(pItem->pData, iDataLen, pData, iDataLen);
		::EnterCriticalSection(&m_cs);
		m_al.push_back(pItem);
		m_dwDataSize += iDataLen;
		printf(".");
		::LeaveCriticalSection(&m_cs);
	}

	return;
}

Audio_Data * CAudioCapT::GetAudio()
{
	Audio_Data * pAudio = NULL;
	::EnterCriticalSection(&m_cs);
	if (m_al.empty() == false)
	{
		pAudio = m_al.front();
		m_al.pop_front();
	}
	::LeaveCriticalSection(&m_cs);
	return pAudio;
}

IMMDevice * CAudioCapT::GetDefaultDevice(int nType)
{
	IMMDevice *pDevice = nullptr;

	IMMDeviceEnumerator *pMMDeviceEnumerator = nullptr;
	HRESULT hr = CoCreateInstance(
		__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
		__uuidof(IMMDeviceEnumerator),
		(void**)&pMMDeviceEnumerator);
	if (FAILED(hr))
		return nullptr;

	if (nType == CAudioCapT::MICPHONE)
		hr = pMMDeviceEnumerator->GetDefaultAudioEndpoint((EDataFlow)eCapture, eConsole, &pDevice);
	else if (nType == CAudioCapT::SPEAKER)
		hr = pMMDeviceEnumerator->GetDefaultAudioEndpoint((EDataFlow)eRender, eConsole, &pDevice);
	else
		pDevice = nullptr;

	if (pMMDeviceEnumerator)
	{
		pMMDeviceEnumerator->Release();
		pMMDeviceEnumerator = nullptr;
	}

	return pDevice;
}

bool CAudioCapT::Init()
{
	if (m_bInit)
		return true;

	ClearAudioList();

	m_pDevice = GetDefaultDevice(m_nDeviceType);

	if (!m_pDevice)
		return false;

	m_hEventStarted = CreateEvent(nullptr, true, false, nullptr);

	if (m_hEventStarted == NULL)
	{
		return false;
	}

	m_bStop = false;

	m_dwDataSize = 0;

	m_bInit = true;

	return true;
}

DWORD CAudioCapT::GetDataSize()
{
	::EnterCriticalSection(&m_cs);
	DWORD dwSize = m_al.size();
	::LeaveCriticalSection(&m_cs);
	return dwSize;
}

IMMDevice * CAudioCapT::GetDevice()
{
	return m_pDevice;
}

HANDLE CAudioCapT::GetStartEventHandle()
{
	return m_hEventStarted;
}

HANDLE CAudioCapT::GetStopEventHandle()
{
	return m_hEventStop;
}

AudioList * CAudioCapT::GetAudioList()
{
	return &m_al;
}

void CAudioCapT::ClearAudioList()
{
	::EnterCriticalSection(&m_cs);
	for (AudioList::iterator it = m_al.begin(); it != m_al.end(); it++)
	{
		byte * pData = (*it)->pData;
		if (pData)
		{
			delete[] pData;
		}
	}

	m_al.clear();
	::LeaveCriticalSection(&m_cs);
}


UINT __stdcall CAudioCapT::_CaptureThreadProc(LPVOID param)
{
	CAudioCapT * pObject = (CAudioCapT *)param;
	if (!pObject)
		return 0;

	HRESULT hr = 0;
	IAudioClient *pAudioClient = nullptr;
	WAVEFORMATEX *pWfx = nullptr;
	IAudioCaptureClient *pAudioCaptureClient = nullptr;
	DWORD nTaskIndex = 0;
	HANDLE hTask = nullptr;
	bool bStarted(false);
	int nDeviceType = 0;
	IMMDevice * pDevice = pObject->GetDevice();
	HANDLE hEventStarted = pObject->GetStartEventHandle();

	REFERENCE_TIME hnsRequestedDuration = REFTIMES_PER_SEC;
	REFERENCE_TIME hnsActualDuration;

	UINT bufferFrameCount = 0;

	if (!pDevice || !hEventStarted)
		return 0;

	CoInitialize(nullptr);

	do
	{
		hr = pDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)&pAudioClient);
		if (FAILED(hr))
			break;

		hr = pAudioClient->GetMixFormat(&pWfx);
		if (FAILED(hr))
			break;

		SetEvent(hEventStarted);

		pObject->SaveFormat(pWfx);

		nDeviceType = pObject->GetDeviceType();

		if (nDeviceType == CAudioCapT::MICPHONE)
			hr = pAudioClient->Initialize(AUDCLNT_SHAREMODE_SHARED, 0, 0, 0, pWfx, 0);
		else if (nDeviceType == CAudioCapT::SPEAKER)
			hr = pAudioClient->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_LOOPBACK, 0, 0, pWfx, 0);
		else
			break;

		if (FAILED(hr))
			break;

		hr = pAudioClient->GetBufferSize(&bufferFrameCount);

		if (FAILED(hr))
			break;

		hr = pAudioClient->GetService(__uuidof(IAudioCaptureClient), (void**)&pAudioCaptureClient);

		if (FAILED(hr))
			break;

		hnsActualDuration = (double)REFTIMES_PER_SEC * bufferFrameCount / pWfx->nSamplesPerSec;

		if (nDeviceType == CAudioCapT::MICPHONE)
			hTask = AvSetMmThreadCharacteristics(_T("Audio"), &nTaskIndex);
		else
			hTask = AvSetMmThreadCharacteristics(_T("Capture"), &nTaskIndex);

		if (!hTask)
			break;

		hr = pAudioClient->Start();
		if (FAILED(hr))
			break;

		bStarted = true;

		DWORD  dwWaitResult;
		UINT32 uiNextPacketSize(0);
		BYTE *pData = nullptr;
		UINT32 uiNumFramesToRead;
		DWORD dwFlags;

		while (pObject->m_bStop == false)
		{
			Sleep(hnsActualDuration / REFTIMES_PER_MILLISEC / 2);

			if (pObject->GetDataSize() > 10)
				continue;

			hr = pAudioCaptureClient->GetNextPacketSize(&uiNextPacketSize);
			if (FAILED(hr))
				break;

			while (uiNextPacketSize != 0)
			{
				hr = pAudioCaptureClient->GetBuffer(
					&pData,
					&uiNumFramesToRead,
					&dwFlags,
					nullptr,
					nullptr);

				if (FAILED(hr))
					break;

				if (dwFlags & AUDCLNT_BUFFERFLAGS_SILENT)
				{
					pData = NULL;
				}
		
				pObject->OnCaptureData(pData, uiNumFramesToRead * pWfx->nBlockAlign);	
	
				pAudioCaptureClient->ReleaseBuffer(uiNumFramesToRead);

				hr = pAudioCaptureClient->GetNextPacketSize(&uiNextPacketSize);

				if (FAILED(hr))
					break;
			}
		}

	} while (0);

	if (hTask)
	{
		AvRevertMmThreadCharacteristics(hTask);
		hTask = nullptr;
	}

	if (pAudioCaptureClient)
	{
		pAudioCaptureClient->Release();
		pAudioCaptureClient = nullptr;
	}

	if (pWfx)
	{
		CoTaskMemFree(pWfx);
		pWfx = nullptr;
	}

	if (pAudioClient)
	{
		if (bStarted)
		{
			pAudioClient->Stop();
		}

		pAudioClient->Release();
		pAudioClient = nullptr;
	}

	CoUninitialize();

	return 0;
}




void CAudioCapT::SaveFormat(WAVEFORMATEX * wf)
{
	if (!wf)
		return;

	memcpy(&m_WaveFormat, wf, sizeof(WAVEFORMATEX));

	return;
}

WAVEFORMATEX * CAudioCapT::GetWaveFormat()
{
	return &m_WaveFormat;
}



bool CAudioCapT::Start()
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

bool CAudioCapT::Stop()
{
	if (m_bInit == false)
		return false;

	m_bStop = true;

	WaitForSingleObject(m_hThreadCapture, INFINITE);

	if (m_pDevice)
	{
		m_pDevice->Release();
		m_pDevice = NULL;
	}
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