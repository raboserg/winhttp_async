#pragma once

#include <string>
#include <windows.h>
#include <winhttp.h>
#include <boost/function.hpp>

#pragma comment(lib, "winhttp.lib")

#define ASYNC_WINHTTP_OK 0
#define ASYNC_WINHTTP_ERROR 1

class ResultState;

class AsyncWinHttpErrorInfo
{
public:
	AsyncWinHttpErrorInfo() : status_(ASYNC_WINHTTP_OK) {}
	~AsyncWinHttpErrorInfo() {}

	void set(const int& status, const std::wstring& desc)
	{
		status_ = status;
		desc_ = desc;
	}

	int Status() { return status_; }
	std::wstring Desc() { return desc_; }

private:
	int status_;
	std::wstring desc_;
};

//class AsyncWinHttp;

//typedef void (*ASYNC_WINHTTP_CALLBACK)(AsyncWinHttp*);
//boost::function<void()> WinHttpAsyncResult_;

class AsyncWinHttp
{
public:
	AsyncWinHttp();
	virtual ~AsyncWinHttp();

	//bool Initialize(ASYNC_WINHTTP_CALLBACK cb);
	bool Initialize(boost::function<void(AsyncWinHttp*)> WinHttpAsyncResult_);
	//bool Initialize(ResultState resultState);
	bool SendRequest(PCWSTR szURL);
	void GetResponse(std::wstring& str) const;
	void GetResponseHeader(std::wstring& str) const;
	void GetResponseRaw(std::string& str) const;
	bool SetTimeout(DWORD timeout);

	AsyncWinHttpErrorInfo status;

private:
	HINTERNET session_;
	HINTERNET connect_;       // Connection handle
	HINTERNET request_;       // Resource request handle
	DWORD     size_;          // Size of the latest data block
	DWORD     totalSize_;     // Size of the total data
	LPSTR     lpBuffer_;      // Buffer for storing read data
	WCHAR     memo_[256];     // String providing state information
	//ASYNC_WINHTTP_CALLBACK RequesterStatusCallback_;
	boost::function<void(AsyncWinHttp*)> WinHttpAsyncResult_;

	std::wstring response_;
	std::wstring responseHeader_;
	std::string* response_raw_;

	VOID OnRequestError(LPWINHTTP_ASYNC_RESULT);
	VOID OnSendRequestComplete(DWORD lpvStatusInformation);
	VOID OnHeadersAvailable(DWORD dwStatusInformationLength);
	VOID OnReadComplete(LPVOID lpvStatusInformation, DWORD dwStatusInformationLength);
	VOID OnHandleClosing(HINTERNET);
	VOID OnDataAvailable(LPVOID lpvStatusInformation);

	bool QueryHeaders();
	LPCWSTR GetApiErrorString(DWORD dwResult);
	bool ReadData();
	bool QueryData();
	void Cleanup();
	
	void TransferAndDeleteBuffers(LPSTR lpReadBuffer, DWORD dwBytesRead);

	static void __stdcall AsyncCallback(
		HINTERNET hInternet, 
		DWORD_PTR dwContext,
		DWORD dwInternetStatus,
		LPVOID lpvStatusInformation,
		DWORD dwStatusInformationLength);
};


class ResultState {
	HANDLE hEvent_;
public:
	ResultState(HANDLE hEvent):hEvent_(hEvent) {}

	void operator()(AsyncWinHttp* asyncWinHttp) {
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
		SetEvent(hEvent_);
	}
};
