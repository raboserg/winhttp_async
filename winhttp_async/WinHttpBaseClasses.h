#pragma once

#include <windows.h>
#include <winhttp.h>
//#include <WebServices.h>
//#include <memory>
#include <string>

#ifdef _DEBUG
#define ASSERT(X) _ASSERT(X)
#define VERIFY(X) _ASSERT(X)
#define ASSUME(X) _ASSERT(X)
#else
#define ASSERT(X)
#deine VERIFY(X) (X)
#define ASSUME(X) __assume(X)
#endif

const LPCWSTR s_userAgent = L"Mozilla/5.0 (Windows NT 6.1) AppleWebKit/537.4 (KHTML, like Gecko) Chrome/22.0.1229.79 Safari/537.4";

class SimpleBuffer {
public:
	void Initialize(DWORD dwSize){
		m_pszOutBuffer = new char[dwSize + 1];
		//m_pszOutBuffer.reset(new char[dwSize+1]);
	}

	DWORD GetCount(){
		return (DWORD)strlen(m_pszOutBuffer);
	}

	void* GetData(){
		return static_cast<VOID*>(m_pszOutBuffer);
	}

	~SimpleBuffer(){
		delete m_pszOutBuffer;
	}

private:
	LPSTR m_pszOutBuffer;
	//std::auto_ptr<LPSTR> m_pszOutBuffer;
};

class WinHttpHandle {
public:
	WinHttpHandle() : m_handle(0) {}

	~WinHttpHandle(){
		Close();
	}

	bool Attach(HINTERNET handle){
		ASSERT(0 == m_handle);
		m_handle = handle;
		return 0 != m_handle;
	}

	HINTERNET Detach(){
		HANDLE handle = m_handle;
		m_handle = 0;
		return handle;
	}

	void Close(){
		if (m_handle != 0){
			VERIFY(::WinHttpCloseHandle(m_handle));
			m_handle = 0;
		}
	}

	HRESULT SetOption(DWORD option, const void* value, DWORD length) {
		if (!::WinHttpSetOption(m_handle, option, const_cast<void*>(value), length)) {
			return HRESULT_FROM_WIN32(::GetLastError());
		}
		return S_OK;
	}

	HRESULT QueryOption(DWORD option, void* value, DWORD& length) const	{
		if (!::WinHttpQueryOption(m_handle, option, value, &length)){
			return HRESULT_FROM_WIN32(::GetLastError());
		}
		return S_OK;
	}

	HINTERNET m_handle;
};

// Session
class WinHttpSession : public WinHttpHandle {
public:
	HRESULT Initialize(const UINT flagAsync) {
		if (!Attach(::WinHttpOpen(s_userAgent,
			WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
			WINHTTP_NO_PROXY_NAME,
			WINHTTP_NO_PROXY_BYPASS,
			flagAsync))) /*WINHTTP_FLAG_ASYNC*/
		{
			return HRESULT_FROM_WIN32(::GetLastError());
		}
		return S_OK;
	}
};

// Connection
class WinHttpConnection : public WinHttpHandle {
public:
	HRESULT Initialize(PCWSTR pwszUrl, const WinHttpSession& session) {
		SIZE_T cchUrl = ::wcslen(pwszUrl) + 1;
		WCHAR* m_pwszUrl = new WCHAR[cchUrl];
		::wcscpy_s(m_pwszUrl, cchUrl, pwszUrl);

		//URL_COMPONENTS UrlComponents;
		ZeroMemory(&UrlComponents, sizeof(UrlComponents));
		UrlComponents.dwStructSize = sizeof(UrlComponents);

		UrlComponents.dwSchemeLength = (DWORD)-1;
		UrlComponents.dwHostNameLength = (DWORD)-1;
		UrlComponents.dwUrlPathLength = (DWORD)-1;

		if (::WinHttpCrackUrl(m_pwszUrl, (DWORD)::wcslen(m_pwszUrl), 0, &UrlComponents) == NULL) {
			fprintf(stderr, "Requester #%d failed to open; ::WinHttpCrackUrl() failed; error = %d.\n", ::GetCurrentThreadId(), ::GetLastError());
			return HRESULT_FROM_WIN32(::GetLastError());
		}

		WCHAR wCharSave = UrlComponents.lpszHostName[UrlComponents.dwHostNameLength];
		UrlComponents.lpszHostName[UrlComponents.dwHostNameLength] = L'\0';

		//Create connection
		if (!Attach(::WinHttpConnect(session.m_handle, UrlComponents.lpszHostName, UrlComponents.nPort, 0))) { //reserved
			return HRESULT_FROM_WIN32(::GetLastError());
		}

		UrlComponents.lpszHostName[UrlComponents.dwHostNameLength] = wCharSave;
		return S_OK;
	}

	URL_COMPONENTS UrlComponents;
};

// Request
template <typename T>
class WinHttpRequest : public WinHttpHandle {
public:
	HRESULT Initialize(PCWSTR path, __in_opt PCWSTR verb, const WinHttpConnection& connection) {
		//pszOutBuffer;// = new char[dwSize+1]; 
		m_buffer.Initialize(8 * 1024);

		DWORD dwOpenRequestFlag = (connection.UrlComponents.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : NULL;
		// call WinHttpOpenRequest and WinHttpSetStatusCallback.
		if (!Attach(::WinHttpOpenRequest( connection.m_handle, verb,
			connection.UrlComponents.lpszUrlPath, L"HTTP/1.1",
			WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, 
			dwOpenRequestFlag))) 		//reserved
			return HRESULT_FROM_WIN32(::GetLastError());

		::WinHttpSetStatusCallback(m_handle,
			RequesterStatusCallback,
			WINHTTP_CALLBACK_FLAG_ALL_COMPLETIONS |
			WINHTTP_CALLBACK_FLAG_HANDLES,   // to listen to the HANDLE_CLOSING event
			0);
		return S_OK;
	}

	HRESULT SendRequest(__in_opt PCWSTR headers,
		DWORD headersLength,
		__in_opt const void* optional,
		DWORD optionalLength,
		DWORD totalLength) 
	{
		T* pT = static_cast<T*>(this);
		// call WinHttpSendRequest with pT as the context value.
		if (::WinHttpSendRequest(m_handle,
			WINHTTP_NO_ADDITIONAL_HEADERS, 0,
			WINHTTP_NO_REQUEST_DATA, 0, 0,
			(DWORD_PTR)&pT) == FALSE) 
		{
			fprintf(stderr, "[%d] The Requester #%d failed to fetch %S, WinHttpSendRequest() failed; error = %d.\n", ::GetLastError());
			return HRESULT_FROM_WIN32(::GetLastError());
		}
	}

protected:
	void OnRequestError(LPWINHTTP_ASYNC_RESULT pAsyncResult) {

	}

	void OnHandleClosing(HINTERNET hInternet) {

	}

	static void CALLBACK RequesterStatusCallback(
		HINTERNET handle,
		DWORD_PTR context,
		DWORD code,
		void* info,
		DWORD length) {
		if (context != 0) {
			T* pT = reinterpret_cast<T*>(context);
			HRESULT result = pT->OnCallback(code, info, length);
			if (FAILED(result)) {
				pT->OnResponseComplete(result);
			}
		}
	}

	HRESULT OnCallback(DWORD code, const void* info, DWORD length) {
		// Handle notifications here.
		T* pRequester = static_cast<T*>(this);
		switch (code) {
			case WINHTTP_CALLBACK_STATUS_SENDREQUEST_COMPLETE:
				if (!::WinHttpReceiveResponse(m_handle, 0)) // reserved
					return HRESULT_FROM_WIN32(::GetLastError());
				break;
			case WINHTTP_CALLBACK_STATUS_HEADERS_AVAILABLE: 
			{
				DWORD statusCode = 0;
				DWORD statusCodeSize = sizeof(DWORD);
				if (!::WinHttpQueryHeaders(m_handle,
					WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
					WINHTTP_HEADER_NAME_BY_INDEX,
					&statusCode,
					&statusCodeSize,
					WINHTTP_NO_HEADER_INDEX))
					return HRESULT_FROM_WIN32(::GetLastError());

				if (HTTP_STATUS_OK != statusCode)
					return E_FAIL;

				if (!::WinHttpReadData(m_handle, m_buffer.GetData(), m_buffer.GetCount(), 0)) // async result
					return HRESULT_FROM_WIN32(::GetLastError());
				break;
			}
			case WINHTTP_CALLBACK_STATUS_READ_COMPLETE:
				if (length < 0) {
					pRequester->OnReadComplete(m_buffer.GetData(), length);
					if (!::WinHttpReadData(m_handle, m_buffer.GetData(), m_buffer.GetCount(), 0)) // async result
						return HRESULT_FROM_WIN32(::GetLastError());
				} else
					pRequester->OnResponseComplete(S_OK);
				break;
			case WINHTTP_CALLBACK_STATUS_REQUEST_ERROR:
				pRequester->OnRequestError((LPWINHTTP_ASYNC_RESULT)info);
				break;
			case WINHTTP_CALLBACK_STATUS_HANDLE_CLOSING:
				pRequester->OnHandleClosing(m_handle);
				pRequester->m_handle = NULL;
				break;
			default:
				return S_FALSE;
		}
		return S_OK;
	}
	//LPSTR pszOutBuffer;// = new char[dwSize+1];
	SimpleBuffer m_buffer;
};

class DownloadPageRequest : public WinHttpRequest<DownloadPageRequest> {
public:
	HRESULT Initialize(PCWSTR source, const WinHttpConnection& connection) {
		//m_destination = destination;
		return __super::Initialize(source, L"GET", connection);
	}

	HRESULT OnReadComplete(const void* buffer, DWORD bytesRead) {
		const char* str = static_cast<const char*>(buffer);
		m_destination.append(str);
		return S_OK;
	}

	void OnResponseComplete(HRESULT result)	{
		if (S_OK == result){
			// download succeeded
		}
	}

private:
	std::string m_destination;
};

//
//class DownloadFileRequest : public WinHttpRequest<DownloadFileRequest>
//{
//public:
//	HRESULT Initialize(PCWSTR source, IStream* destination, const WinHttpConnection& connection)
//	{
//		m_destination = destination;
//		return __super::Initialize(source, 0/*GET*/, connection);
//	}
//
//	HRESULT OnReadComplete(const void* buffer, DWORD bytesRead)
//	{
//		return m_destination->Write(buffer, bytesRead, 0); // ignored
//	}
//
//	void OnResponseComplete(HRESULT result)
//	{
//		if (S_OK == result)
//		{
//			// Download succeeded
//		}
//	}
//
//private:
//	CComPtr<IStream> m_destination;
//};