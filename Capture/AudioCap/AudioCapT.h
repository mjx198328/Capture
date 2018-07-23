#pragma once
#include <windows.h>
#include <windowsx.h>
#include <mmdeviceapi.h>
#include <Audioclient.h>
#include <process.h>
#include <avrt.h>
#include <list>

typedef struct
{
	INT		iDataLen;
	LPBYTE	pData;
}Audio_Data, * PAudio_Data;

typedef std::list<PAudio_Data> AudioList;

class CAudioCapT
{
public:

	enum {SPEAKER = 1,MICPHONE};

public:
	CAudioCapT();
	~CAudioCapT();
	//开始捕获
	bool Start();
	//停止捕获
	bool Stop();

	void SaveFormat(WAVEFORMATEX * wf);

	void SetDeiveType(int nType);

	int GetDeviceType();
	
	Audio_Data * GetAudio();

	bool Init();

	IMMDevice * GetDevice();

	HANDLE GetStartEventHandle();

	HANDLE GetStopEventHandle();

	WAVEFORMATEX * GetWaveFormat();

	AudioList * GetAudioList();

	void ClearAudioList();

	DWORD GetDataSize();


	bool m_bStop;

protected:
	bool m_bInit;
	HANDLE m_hThreadCapture;
	static UINT __stdcall _CaptureThreadProc(LPVOID param);

	void OnCaptureData(LPBYTE pData, INT iDataLen);

	WAVEFORMATEX m_WaveFormat;

	int m_nDeviceType = 0;

	AudioList m_al;

	CRITICAL_SECTION m_cs;

	IMMDevice * m_pDevice = NULL;

	HANDLE m_hEventStarted = NULL;

	HANDLE m_hEventStop = NULL;

	IMMDevice * GetDefaultDevice(int nType);

	DWORD m_dwDataSize;
};

