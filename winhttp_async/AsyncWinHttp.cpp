
#include "stdafx.h"
#include "AsyncWinHttp.h"

static const LPCWSTR userAgent = L"Mozilla/5.0 (Windows NT 6.1) AppleWebKit/537.4 (KHTML, like Gecko) Chrome/22.0.1229.79 Safari/537.4";

AsyncWinHttp::AsyncWinHttp()
	:session_(NULL),
	connect_(NULL),
	request_(NULL),
	size_(0),
	totalSize_(0),
	lpBuffer_(NULL),
	callbackAsyncResultState_(NULL),
	response_raw_(NULL) {
	memset(memo_, 0, sizeof(memo_));
}

AsyncWinHttp::~AsyncWinHttp() {
	Cleanup();
	if (response_raw_ != NULL) {
		delete response_raw_;
		response_raw_ = NULL;
	}
}

//bool AsyncWinHttp::Initialize(ASYNC_WINHTTP_CALLBACK WinHttpAsyncResult)
bool AsyncWinHttp::Initialize(std::function<void(AsyncWinHttp*)> callbackAsyncResultState) {
	callbackAsyncResultState_ = callbackAsyncResultState;
	session_ = ::WinHttpOpen(userAgent,
		WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
		WINHTTP_NO_PROXY_NAME,
		WINHTTP_NO_PROXY_BYPASS,
		WINHTTP_FLAG_ASYNC);
	return (session_ != NULL);
}

bool AsyncWinHttp::SendRequest(PCWSTR szURL) {
	WCHAR szHost[256];
	DWORD dwOpenRequestFlag = 0;
	URL_COMPONENTS urlComp;
	bool fRet = false;

	// Initialize URL_COMPONENTS structure.
	ZeroMemory(&urlComp, sizeof(urlComp));
	urlComp.dwStructSize = sizeof(urlComp);

	// Use allocated buffer to store the Host Name.
	urlComp.lpszHostName = szHost;
	urlComp.dwHostNameLength = sizeof(szHost) / sizeof(szHost[0]);

	// Set non-zero lengths to obtain pointer a to the URL Path.
	// NOTE: If we threat this pointer as a null-terminated string,
	//       it gives us access to additional information as well. 
	urlComp.dwUrlPathLength = -1;

	// Crack HTTP scheme.
	urlComp.dwSchemeLength = -1;

	// Set the szMemo string.
	swprintf_s(memo_, L"WinHttpCrackURL");

	// Crack the URL.
	if (!::WinHttpCrackUrl(szURL, 0, 0, &urlComp)) {
		goto cleanup;
	}

	// Set the szMemo string.
	swprintf_s(memo_, L"WinHttpConnect");

	// Open an HTTP session.
	connect_ = ::WinHttpConnect(session_, szHost, urlComp.nPort, 0);
	if (NULL == connect_) {
		goto cleanup;
	}

	// Prepare OpenRequest flag
	dwOpenRequestFlag = (INTERNET_SCHEME_HTTPS == urlComp.nScheme) ? WINHTTP_FLAG_SECURE : 0;

	// Set the szMemo string.
	swprintf_s(memo_, L"WinHttpOpenRequest");

	// Open a "GET" request.
	request_ = ::WinHttpOpenRequest(connect_,
		L"GET", urlComp.lpszUrlPath,
		NULL, WINHTTP_NO_REFERER,
		WINHTTP_DEFAULT_ACCEPT_TYPES,
		dwOpenRequestFlag);

	if (request_ == 0) {
		goto cleanup;
	}

	// Set the szMemo string.
	swprintf_s(memo_, L"WinHttpSetStatusCallback");

	// Install the status callback function.
	WINHTTP_STATUS_CALLBACK pCallback = ::WinHttpSetStatusCallback(request_,
		(WINHTTP_STATUS_CALLBACK)AsyncCallback,
		WINHTTP_CALLBACK_FLAG_ALL_COMPLETIONS |
		WINHTTP_CALLBACK_FLAG_REDIRECT,
		NULL);

	// note: On success WinHttpSetStatusCallback returns the previously defined callback function. Here it should be NULL
	if (pCallback != NULL) {
		goto cleanup;
	}

	// Set the szMemo string.
	swprintf_s(memo_, L"WinHttpSendRequest");

	// Send the request.
	if (!::WinHttpSendRequest(request_,
		WINHTTP_NO_ADDITIONAL_HEADERS, 0,
		WINHTTP_NO_REQUEST_DATA, 0, 0,
		(DWORD_PTR)this)) {
		goto cleanup;
	}
	fRet = true;

cleanup:

	if (fRet == false) {
		WCHAR szError[256];
		// Set the error message.
		swprintf_s(szError, L"%s failed with error %d", memo_, GetLastError());
		status.set(ASYNC_WINHTTP_ERROR, szError);
		// Cleanup handles.
		Cleanup();
	}
	return fRet;
}


bool AsyncWinHttp::QueryHeaders() {
	DWORD dwSize = 0;
	LPVOID lpOutBuffer = NULL;
	bool ret = false;

	// Set the state memo.
	swprintf_s(this->memo_, L"WinHttpQueryHeaders");

	// Use HttpQueryInfo to obtain the size of the buffer.
	if (!::WinHttpQueryHeaders(this->request_,
		WINHTTP_QUERY_RAW_HEADERS_CRLF,
		WINHTTP_HEADER_NAME_BY_INDEX, NULL, &size_, WINHTTP_NO_HEADER_INDEX)) {
		// An ERROR_INSUFFICIENT_BUFFER is expected because you
		// are looking for the size of the headers.  If any other
		// error is encountered, display error information.
		DWORD dwErr = GetLastError();
		if (dwErr != ERROR_INSUFFICIENT_BUFFER) {
			WCHAR  szError[256];
			swprintf_s(szError, L"%s: Error %d encountered.", memo_, dwErr);
			status.set(ASYNC_WINHTTP_ERROR, szError);
			return false;
		}
	}

	// Allocate memory for the buffer.
	lpOutBuffer = new WCHAR[dwSize];

	// Use HttpQueryInfo to obtain the header buffer.
	if (::WinHttpQueryHeaders(this->request_,
		WINHTTP_QUERY_RAW_HEADERS_CRLF,
		WINHTTP_HEADER_NAME_BY_INDEX, lpOutBuffer, &dwSize, WINHTTP_NO_HEADER_INDEX))
	{
		responseHeader_ = (LPWSTR)lpOutBuffer;
		ret = true;
	}
	else
		ret = false;

	// Free the allocated memory.
	delete[] lpOutBuffer;
	return ret;
}


void AsyncWinHttp::Cleanup() {
	// Set the memo to indicate a closed handle.
	swprintf_s(memo_, L"Closed");

	if (request_) {
		::WinHttpSetStatusCallback(request_, NULL, NULL, NULL);
		::WinHttpCloseHandle(request_);
		request_ = NULL;
	}

	if (connect_) {
		WinHttpCloseHandle(connect_);
		connect_ = NULL;
	}

	delete[] lpBuffer_;
	lpBuffer_ = NULL;

	if (callbackAsyncResultState_ != NULL) {
		callbackAsyncResultState_(this);
		callbackAsyncResultState_ = NULL;
	}
}

bool AsyncWinHttp::QueryData() {
	// Set the state memo.
	swprintf_s(memo_, L"WinHttpQueryDataAvailable");
	// Check for available data.
	if (::WinHttpQueryDataAvailable(request_, NULL) == FALSE) {
		// If a synchronous error occured, display the error.  Otherwise the query is successful or asynchronous.
		DWORD dwErr = GetLastError();
		WCHAR  szError[256];
		swprintf_s(szError, L"%s: Error %d encountered.", memo_, dwErr);
		status.set(ASYNC_WINHTTP_ERROR, szError);
		return false;
	}
	return true;
}

bool AsyncWinHttp::ReadData() {
	LPSTR lpOutBuffer = new char[size_ + 1];
	ZeroMemory(lpOutBuffer, size_ + 1);
	// Set the state memo.
	swprintf_s(memo_, L"WinHttpReadData");
	// Read the available data.
	if (::WinHttpReadData(request_, (LPVOID)lpOutBuffer, size_, NULL) == FALSE) {
		// If a synchronous error occurred, display the error.  Otherwise the read is successful or asynchronous.
		DWORD dwErr = GetLastError();
		WCHAR  szError[256];
		swprintf_s(szError, L"%s: Error %d encountered.", memo_, dwErr);
		status.set(ASYNC_WINHTTP_ERROR, szError);
		delete[] lpOutBuffer;
		return false;
	}
	return true;
}

#define CASE_OF(constant) case constant: return (L# constant)

LPCWSTR AsyncWinHttp::GetApiErrorString(DWORD dwResult) {
	// Return the error result as a string so that the name of the function causing the error can be displayed.   
	switch (dwResult) {
		CASE_OF(API_RECEIVE_RESPONSE);
		CASE_OF(API_QUERY_DATA_AVAILABLE);
		CASE_OF(API_READ_DATA);
		CASE_OF(API_WRITE_DATA);
		CASE_OF(API_SEND_REQUEST);
	}
	return L"Unknown function";
}

void AsyncWinHttp::TransferAndDeleteBuffers(LPSTR lpReadBuffer, DWORD dwBytesRead) {
	size_ = dwBytesRead;

	if (!lpBuffer_) {	// If there is no context buffer, start one with the read data.
		lpBuffer_ = lpReadBuffer;
	}
	else {	// Store the previous buffer, and create a new one big enough to hold the old data and the new data.
		LPSTR lpOldBuffer = lpBuffer_;
		lpBuffer_ = new char[totalSize_ + size_ + 1];
		memset(lpBuffer_, 0, totalSize_ + size_ + 1);

		// Copy the old and read buffer into the new context buffer.
		memcpy(lpBuffer_, lpOldBuffer, totalSize_);
		memcpy(lpBuffer_ + totalSize_, lpReadBuffer, size_);

		// Free the memory allocated to the old and read buffers.
		delete[] lpOldBuffer;
		delete[] lpReadBuffer;
	}
	//	printf("%s\n", lpBuffer_);
	// Keep track of the total size.
	totalSize_ += size_;
}

VOID AsyncWinHttp::OnSendRequestComplete(DWORD dwStatusInformationLength) {
	WCHAR szBuffer[256] = { 0 };
	DWORD dwThreadId = ::GetCurrentThreadId();
	swprintf_s(szBuffer, L"%s: SENDREQUEST_COMPLETE (%d)", memo_, dwStatusInformationLength);

	// Prepare the request handle to receive a response.
	if (::WinHttpReceiveResponse(request_, NULL) == FALSE) {
		Cleanup();
	}
}

VOID AsyncWinHttp::OnHeadersAvailable(DWORD dwStatusInformationLength) {
	WCHAR szBuffer[256] = { 0 };
	DWORD dwThreadId = ::GetCurrentThreadId();
	swprintf_s(szBuffer, L"%s: HEADERS_AVAILABLE (%d)", memo_, dwStatusInformationLength);

	QueryHeaders();

	// Initialize the buffer sizes.
	size_ = 0;
	totalSize_ = 0;
	// Begin downloading the resource.
	if (QueryData() == false) {
		Cleanup();
	}
}

VOID AsyncWinHttp::OnDataAvailable(LPVOID lpvStatusInformation) {
	WCHAR szBuffer[256] = { 0 };
	swprintf_s(szBuffer, L"%s: DATA_AVAILABLE (%p)", this->memo_, lpvStatusInformation);

	this->size_ = *((LPDWORD)lpvStatusInformation);

	// If there is no data, the process is complete.
	if (this->size_ == 0) {
		// All of the data has been read.  Display the data.
		if (this->totalSize_) {
			// Convert the final context buffer to wide characters, and display.
			if (this->response_raw_ != NULL) {
				delete this->response_raw_;
				this->response_raw_ = NULL;
			}
			this->response_raw_ = new std::string(this->lpBuffer_, this->totalSize_);
			LPWSTR lpWideBuffer = new WCHAR[this->totalSize_ + 1];

			::MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED,
				this->lpBuffer_,
				this->totalSize_,
				lpWideBuffer,
				this->totalSize_);

			lpWideBuffer[this->totalSize_] = 0;
			this->response_ = lpWideBuffer;

			// Delete the remaining data buffers.
			delete[] lpWideBuffer;
			printf("%s", this->lpBuffer_);
			delete[] this->lpBuffer_;
			this->lpBuffer_ = NULL;
		}

		// Close the request and connect handles for this context.
		this->Cleanup();
	}
	else // Otherwise, read the next block of data.
		if (this->ReadData() == false)
			this->Cleanup();
}

VOID AsyncWinHttp::OnReadComplete(LPVOID lpvStatusInformation, DWORD dwStatusInformationLength) {
	wchar_t szBuffer[256] = { 0 };
	DWORD dwThreadId = ::GetCurrentThreadId();
	swprintf_s(szBuffer, L"%s: READ_COMPLETE (%d)", this->memo_, dwStatusInformationLength);

	// Copy the data and delete the buffers.
	if (dwStatusInformationLength != 0) {
		this->TransferAndDeleteBuffers((LPSTR)lpvStatusInformation, dwStatusInformationLength);
		// Check for more data.
		if (this->QueryData() == false) {
			this->Cleanup();
		}
	}
}


VOID AsyncWinHttp::OnRequestError(LPWINHTTP_ASYNC_RESULT pAsyncResult) {
	wchar_t szBuffer[256] = { 0 };
	swprintf_s(szBuffer, L"%s: REQUEST_ERROR - error %d, result %s", this->memo_, pAsyncResult->dwError, this->GetApiErrorString((DWORD)pAsyncResult->dwResult));
	this->status.set(ASYNC_WINHTTP_ERROR, szBuffer);
	this->Cleanup();
}

void __stdcall AsyncWinHttp::AsyncCallback(
	HINTERNET hInternet,
	DWORD_PTR dwContext,
	DWORD dwInternetStatus,
	LPVOID lpvStatusInformation,
	DWORD dwStatusInformationLength) {

	AsyncWinHttp * pRequester = (AsyncWinHttp*)dwContext;
	wchar_t szBuffer[256] = { 0 };

	if (pRequester == NULL)	return;

	switch (dwInternetStatus) {
	case WINHTTP_CALLBACK_STATUS_SENDREQUEST_COMPLETE:
		pRequester->OnSendRequestComplete(dwStatusInformationLength);
		break;
	case WINHTTP_CALLBACK_STATUS_HEADERS_AVAILABLE:
		pRequester->OnHeadersAvailable(dwStatusInformationLength);
		break;
	case WINHTTP_CALLBACK_STATUS_DATA_AVAILABLE:
		pRequester->OnDataAvailable(lpvStatusInformation);
		break;
	case WINHTTP_CALLBACK_STATUS_READ_COMPLETE:
		pRequester->OnReadComplete(lpvStatusInformation, dwStatusInformationLength);
		break;
	case WINHTTP_CALLBACK_STATUS_REDIRECT:
		swprintf_s(szBuffer, L"%s: REDIRECT (%d)", pRequester->memo_, dwStatusInformationLength);
		break;
	case WINHTTP_CALLBACK_STATUS_CLOSE_COMPLETE:
		//pRequester->OnHandleClosing(hInternet);
		break;
	case WINHTTP_CALLBACK_STATUS_REQUEST_ERROR:
		pRequester->OnRequestError((WINHTTP_ASYNC_RESULT *)lpvStatusInformation);
		break;
	default:
		swprintf_s(szBuffer, L"%s: Unknown/unhandled callback - status %d given", pRequester->memo_, dwInternetStatus);
		break;
	}
}

void AsyncWinHttp::GetResponse(std::wstring& str) const {
	str = response_;
}

void AsyncWinHttp::GetResponseHeader(std::wstring& str) const {
	str = responseHeader_;
}

void AsyncWinHttp::GetResponseRaw(std::string& str) const {
	if (response_raw_ == NULL) {
		str = "";
		return;
	}
	str = *response_raw_;
}

bool AsyncWinHttp::SetTimeout(DWORD timeout) {
	if (session_ == NULL)
		return false;
	BOOL httpResult = ::WinHttpSetOption(session_,
		WINHTTP_OPTION_CONNECT_TIMEOUT,
		&timeout,
		sizeof(timeout));
	return (httpResult == TRUE);
}