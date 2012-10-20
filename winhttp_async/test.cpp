// winhttp_async.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <string>
#include "AsyncWinHttp.h"

static PCWSTR signalUrl = L"http://tetraforex.anthill.by/management/components/signals/signal.xml";
static const std::wstring iDsUrl = L"http://tetraforex.anthill.by/management/components/signals/handler.php?action=deactivateSignals&id=";


HANDLE hEvent = NULL;

void WinHttp_CallBack(AsyncWinHttp* asyncWinHttp)
{
	if (asyncWinHttp->status.Status() == ASYNC_WINHTTP_ERROR)
	{
		printf("%S", asyncWinHttp->status.Desc().c_str());
	}
	else
	{
		std::string response;
		asyncWinHttp->GetResponseRaw(response);
		printf("%s", response.c_str());
	}
	SetEvent(hEvent);
}

int _tmain(int argc, _TCHAR* argv[])
{
	hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);

	AsyncWinHttp http;
	//ResultState resultState_(hEvent);
	boost::function<void(AsyncWinHttp*)> f(&WinHttp_CallBack);
	//http.Initialize(resultState_);
	http.Initialize(f);
	http.SetTimeout(3 * 60 * 1000);
	http.SendRequest(signalUrl);
	
	WaitForSingleObject(hEvent, INFINITE);
	CloseHandle(hEvent);

	//fgetc(stdin);
	return 0;
}

