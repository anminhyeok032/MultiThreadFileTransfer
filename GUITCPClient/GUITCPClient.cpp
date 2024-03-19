#include "..\Common.h"
#include "resource.h"

#include <CommCtrl.h>
#include <commdlg.h>
#include <string>

#define SERVERIP   "127.0.0.1"
#define SERVERPORT 9000
#define BUFSIZE    512
#define BUFSIZE_NAME 100

// 대화상자 프로시저
INT_PTR CALLBACK DlgProc(HWND, UINT, WPARAM, LPARAM);

// 에디트 컨트롤 출력 함수
void DisplayText(const char* fmt, ...);

// 소켓 함수 오류 출력
void DisplayError(const char* msg);

// 소켓 통신 스레드 함수
DWORD WINAPI ClientMain(LPVOID arg);


SOCKET sock;							// 소켓
char buf[BUFSIZE + 1];					// 데이터 송수신 버퍼

HANDLE hReadEvent, hWriteEvent;			// 이벤트

HWND hSendButton;						// 보내기 버튼

HWND hEdit;								// 에디트 컨트롤 텍스트 상자
HINSTANCE hInst;						// 인스턴스 핸들

HWND hProgress;							// 프로그래스 바

HWND hwnd;								// 파일 선택
OPENFILENAME OFN;						// 파일 관련 구조체 선언
TCHAR str[BUFSIZE_NAME] = L"";
TCHAR filter[100] = L"모든 파일(*.*)\0*.*\0";
TCHAR lpstrFile[MAX_PATH] = { 0 };		// 파일 경로를 저장할 변수 초기화

char* file_name;


int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
	LPSTR lpCmdLine, int nCmdShow)
{
	// 윈속 초기화
	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
		return 1;

	// 이벤트 생성
	hReadEvent = CreateEvent(NULL, FALSE, TRUE, NULL);
	hWriteEvent = CreateEvent(NULL, FALSE, FALSE, NULL);

	// 소켓 통신 스레드 생성
	CreateThread(NULL, 0, ClientMain, NULL, 0, NULL);

	// 대화상자 생성
	DialogBox(hInstance, MAKEINTRESOURCE(IDD_DIALOG1), NULL, DlgProc);

	// 이벤트 제거
	CloseHandle(hReadEvent);
	CloseHandle(hWriteEvent);

	// 윈속 종료
	WSACleanup();
	return 0;
}

// 대화상자 프로시저
INT_PTR CALLBACK DlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg) {
	case WM_INITDIALOG:
		// 버튼 생성
		hSendButton = GetDlgItem(hDlg, IDOK);

		// 텍스트 박스 생성
		hEdit = CreateWindow(_T("edit"), NULL,
			WS_CHILD | WS_VISIBLE | WS_HSCROLL |
			WS_VSCROLL | ES_AUTOHSCROLL |
			ES_AUTOVSCROLL | ES_MULTILINE,
			10, 100, 500, 100, hDlg, (HMENU)100, hInst, NULL);
		DisplayText("GUI 응용 클라이언트 프로그램입니다.\r\n");

		// 프로그래스 바 생성
		hProgress = CreateWindowEx(
			0, PROGRESS_CLASS, NULL,
			WS_CHILD | WS_VISIBLE | PBS_SMOOTH,
			50, 50, 200, 30,
			hDlg, NULL, NULL, NULL);

		// 프로그래스 바 범위 설정 (0 ~ 100)
		SendMessage(hProgress, PBM_SETRANGE, 0, MAKELPARAM(0, 100));

		// 초기 값 설정 (0)
		SendMessage(hProgress, PBM_SETPOS, 0, 0);

		// 파일선택
		hwnd = CreateWindow(
			L"BUTTON",												// 윈도우 클래스 이름
			L"파일 열기",											// 버튼 텍스트
			WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,	// 윈도우 스타일
			10, 10, 100, 30,										// 버튼 위치와 크기 (x, y, width, height)
			hDlg,													// 부모 윈도우 핸들
			(HMENU)ID_FILEOPEN,										// 버튼 식별자
			(HINSTANCE)GetWindowLongPtr(hDlg, GWLP_HINSTANCE),		// 인스턴스 핸들
			NULL);													// 생성한 버튼에 대한 추가 데이터

		return TRUE;

	case WM_CREATE:
		break;
	
	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDOK:
			EnableWindow(hSendButton, FALSE); // 보내기 버튼 비활성화
			WaitForSingleObject(hReadEvent, INFINITE); // 읽기 완료 대기
			SetEvent(hWriteEvent); // 쓰기 완료 알림
			return TRUE;
			
		case ID_FILEOPEN: // 메뉴 선택
			memset(&OFN, 0, sizeof(OPENFILENAME)); // 구조체 초기화
			OFN.lStructSize = sizeof(OPENFILENAME);
			OFN.hwndOwner = hwnd;
			OFN.lpstrFilter = filter;
			OFN.lpstrFile = lpstrFile;
			OFN.nMaxFile = 256;
			OFN.lpstrInitialDir = L"."; // 초기 디렉토리
			if (GetOpenFileName(&OFN) != 0) { // 파일 열기 함수 호출
				wsprintf(str, L"%s", OFN.lpstrFile);

				// 필요한 버퍼 사이즈 계산
				int bufferSize = WideCharToMultiByte(CP_ACP, 0, str, -1, NULL, 0, NULL, NULL);

				// 변환된 문자열을 저장할 버퍼 동적 할당
				char* file_name_tmp = new char[bufferSize];
				WideCharToMultiByte(CP_ACP, 0, str, -1, file_name_tmp, bufferSize, NULL, NULL);
				file_name = file_name_tmp;
				
				DisplayText("파일 경로 : %s ", file_name);
				MessageBox(hwnd, str, L"열기 선택", MB_OK);
			}
			break;

		case IDCANCEL:
			EndDialog(hDlg, IDCANCEL); // 대화상자 닫기
			closesocket(sock); // 소켓 닫기
			return TRUE;
		}
		return FALSE;
	}
	return FALSE;
}

// 에디트 컨트롤 출력 함수
void DisplayText(const char* fmt, ...)
{
	va_list arg;
	va_start(arg, fmt);
	char cbuf[BUFSIZE * 2];
	vsprintf(cbuf, fmt, arg);
	va_end(arg);

	int nLength = GetWindowTextLength(hEdit);
	SendMessage(hEdit, EM_SETSEL, nLength, nLength);
	SendMessageA(hEdit, EM_REPLACESEL, FALSE, (LPARAM)cbuf);
}

// 소켓 함수 오류 출력
void DisplayError(const char* msg)
{
	LPVOID lpMsgBuf;
	FormatMessageA(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
		NULL, WSAGetLastError(),
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(char*)&lpMsgBuf, 0, NULL);
	DisplayText("[%s] %s\r\n", msg, (char*)lpMsgBuf);
	LocalFree(lpMsgBuf);
}

// TCP 클라이언트 시작 부분
DWORD WINAPI ClientMain(LPVOID arg)
{
	int retval;
	char buf_name[BUFSIZE_NAME + 1]; // 가변 길이 데이터

	// 소켓 생성
	sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock == INVALID_SOCKET) err_quit("socket()");

	// connect()
	struct sockaddr_in serveraddr;
	memset(&serveraddr, 0, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.s_addr = inet_addr(SERVERIP);
	serveraddr.sin_port = htons(SERVERPORT);
	retval = connect(sock, (struct sockaddr*)&serveraddr, sizeof(serveraddr));
	if (retval == SOCKET_ERROR) err_quit("connect()");

	while (1) {
		WaitForSingleObject(hWriteEvent, INFINITE); // 쓰기 완료 대기

		int sendedBytes = 0;
		int totalSendBytes = 0;
		int filesize = 0;

		// 데이터 통신에 사용할 변수
		FILE* file = fopen(file_name, "rb");

		// 파일 이름 추출
		std::string file_original_name = std::string(file_name).substr(std::string(file_name).find_last_of("\\") + 1);

		if (file == NULL) {
			DisplayText("파일이 없습니다. 프로그램 종료\n");
			return 0;
		}

		fseek(file, 0, SEEK_END);
		filesize = ftell(file);
		fseek(file, 0, SEEK_SET);

		// 서버와 데이터 통신
		int len = (int)strlen(file_original_name.c_str());
		strncpy(buf_name, file_original_name.c_str(), len);
		buf_name[len] = '\0';

		//==============================
		// 데이터 보내기(파일 이름 고정 길이)
		//==============================
		retval = send(sock, (char*)&len, sizeof(int), 0);
		if (retval == SOCKET_ERROR) {
			DisplayError("send()");
			return false;
		}

		// 데이터 보내기(파일 이름 가변 길이)
		retval = send(sock, buf_name, len, 0);
		if (retval == SOCKET_ERROR) {
			DisplayError("send()");
			return false;
		}
		DisplayText("[TCP 클라이언트] 파일이름 : %s를 %d+%d바이트를 "
			"보냈습니다.\n", buf_name, (int)sizeof(int), retval);


		//==============================
		// 데이터 보내기(영상파일 고정 길이)
		//==============================
		DisplayText("**파일 보내기**\n");
		DisplayText("File size = %d\n", filesize);
		retval = send(sock, (char*)&filesize, sizeof(int), 0);
		if (retval == SOCKET_ERROR) {
			DisplayError("send()");
			return false;
		}
		
		// 데이터 보내기(영상파일 가변 길이)
		while ((retval = fread(buf, sizeof(char), sizeof(buf), file)) > 0) {
			send(sock, buf, retval, 0);
			totalSendBytes += retval;
			
			// 프로그래스 바 업데이트
			SendMessage(hProgress, PBM_SETPOS, ((double)totalSendBytes / filesize) * 100, 0);
			
			//Sleep(1);
			if (retval == SOCKET_ERROR) {
				DisplayError("send()");
				break;
			}
		}

		DisplayText("[TCP 클라이언트] %d+%d바이트를 "
			"보냈습니다.\n", (int)sizeof(int), filesize);

		EnableWindow(hSendButton, TRUE); // 보내기 버튼 활성화
		SetEvent(hReadEvent); // 읽기 완료 알림
	}

	return 0;
}
