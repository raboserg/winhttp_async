// winhttp_async.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <string>
#include "AsyncWinHttp.h"

static PCWSTR signalUrl = L"https://msdn.microsoft.com/ru-ru/library/windows/desktop/ms682396(v=vs.85).aspx";
static const std::wstring iDsUrl = L"http://tetraforex.anthill.by/management/components/signals/handler.php?action=deactivateSignals&id=";


HANDLE hEvent = NULL;

void WinHttp_CallBack(AsyncWinHttp* asyncWinHttp) {
	if (asyncWinHttp->status.Status() == ASYNC_WINHTTP_ERROR) {
		printf("%S", asyncWinHttp->status.Desc().c_str());
	}
	else {
		std::string response;
		asyncWinHttp->GetResponseRaw(response);
		printf("%s", response.c_str());
	}
	SetEvent(hEvent);
}

int _tmain(int argc, _TCHAR* argv[]) {
	hEvent = CreateEvent(NULL, FALSE, FALSE, L"ASYNC_WINHTTP_EVENT");

	AsyncWinHttp http;
	//ASYNC_WINHTTP_CALLBACK dd = AsyncWinHttp_Callback;
	http.Initialize(ResultState(hEvent));
	http.SetTimeout(3 * 60 * 1000);
	http.SendRequest(signalUrl);

	WaitForSingleObject(hEvent, INFINITE);
	CloseHandle(hEvent);

	//fgetc(stdin);
	return 0;
}

