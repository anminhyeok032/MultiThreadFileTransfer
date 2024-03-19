#include "..\Common.h"
#include "resource.h"

#include <CommCtrl.h>
#include <commdlg.h>
#include <string>

#define SERVERIP   "127.0.0.1"
#define SERVERPORT 9000
#define BUFSIZE    512
#define BUFSIZE_NAME 100

// ��ȭ���� ���ν���
INT_PTR CALLBACK DlgProc(HWND, UINT, WPARAM, LPARAM);

// ����Ʈ ��Ʈ�� ��� �Լ�
void DisplayText(const char* fmt, ...);

// ���� �Լ� ���� ���
void DisplayError(const char* msg);

// ���� ��� ������ �Լ�
DWORD WINAPI ClientMain(LPVOID arg);


SOCKET sock;							// ����
char buf[BUFSIZE + 1];					// ������ �ۼ��� ����

HANDLE hReadEvent, hWriteEvent;			// �̺�Ʈ

HWND hSendButton;						// ������ ��ư

HWND hEdit;								// ����Ʈ ��Ʈ�� �ؽ�Ʈ ����
HINSTANCE hInst;						// �ν��Ͻ� �ڵ�

HWND hProgress;							// ���α׷��� ��

HWND hwnd;								// ���� ����
OPENFILENAME OFN;						// ���� ���� ����ü ����
TCHAR str[BUFSIZE_NAME] = L"";
TCHAR filter[100] = L"��� ����(*.*)\0*.*\0";
TCHAR lpstrFile[MAX_PATH] = { 0 };		// ���� ��θ� ������ ���� �ʱ�ȭ

char* file_name;


int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
	LPSTR lpCmdLine, int nCmdShow)
{
	// ���� �ʱ�ȭ
	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
		return 1;

	// �̺�Ʈ ����
	hReadEvent = CreateEvent(NULL, FALSE, TRUE, NULL);
	hWriteEvent = CreateEvent(NULL, FALSE, FALSE, NULL);

	// ���� ��� ������ ����
	CreateThread(NULL, 0, ClientMain, NULL, 0, NULL);

	// ��ȭ���� ����
	DialogBox(hInstance, MAKEINTRESOURCE(IDD_DIALOG1), NULL, DlgProc);

	// �̺�Ʈ ����
	CloseHandle(hReadEvent);
	CloseHandle(hWriteEvent);

	// ���� ����
	WSACleanup();
	return 0;
}

// ��ȭ���� ���ν���
INT_PTR CALLBACK DlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg) {
	case WM_INITDIALOG:
		// ��ư ����
		hSendButton = GetDlgItem(hDlg, IDOK);

		// �ؽ�Ʈ �ڽ� ����
		hEdit = CreateWindow(_T("edit"), NULL,
			WS_CHILD | WS_VISIBLE | WS_HSCROLL |
			WS_VSCROLL | ES_AUTOHSCROLL |
			ES_AUTOVSCROLL | ES_MULTILINE,
			10, 100, 500, 100, hDlg, (HMENU)100, hInst, NULL);
		DisplayText("GUI ���� Ŭ���̾�Ʈ ���α׷��Դϴ�.\r\n");

		// ���α׷��� �� ����
		hProgress = CreateWindowEx(
			0, PROGRESS_CLASS, NULL,
			WS_CHILD | WS_VISIBLE | PBS_SMOOTH,
			50, 50, 200, 30,
			hDlg, NULL, NULL, NULL);

		// ���α׷��� �� ���� ���� (0 ~ 100)
		SendMessage(hProgress, PBM_SETRANGE, 0, MAKELPARAM(0, 100));

		// �ʱ� �� ���� (0)
		SendMessage(hProgress, PBM_SETPOS, 0, 0);

		// ���ϼ���
		hwnd = CreateWindow(
			L"BUTTON",												// ������ Ŭ���� �̸�
			L"���� ����",											// ��ư �ؽ�Ʈ
			WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,	// ������ ��Ÿ��
			10, 10, 100, 30,										// ��ư ��ġ�� ũ�� (x, y, width, height)
			hDlg,													// �θ� ������ �ڵ�
			(HMENU)ID_FILEOPEN,										// ��ư �ĺ���
			(HINSTANCE)GetWindowLongPtr(hDlg, GWLP_HINSTANCE),		// �ν��Ͻ� �ڵ�
			NULL);													// ������ ��ư�� ���� �߰� ������

		return TRUE;

	case WM_CREATE:
		break;
	
	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDOK:
			EnableWindow(hSendButton, FALSE); // ������ ��ư ��Ȱ��ȭ
			WaitForSingleObject(hReadEvent, INFINITE); // �б� �Ϸ� ���
			SetEvent(hWriteEvent); // ���� �Ϸ� �˸�
			return TRUE;
			
		case ID_FILEOPEN: // �޴� ����
			memset(&OFN, 0, sizeof(OPENFILENAME)); // ����ü �ʱ�ȭ
			OFN.lStructSize = sizeof(OPENFILENAME);
			OFN.hwndOwner = hwnd;
			OFN.lpstrFilter = filter;
			OFN.lpstrFile = lpstrFile;
			OFN.nMaxFile = 256;
			OFN.lpstrInitialDir = L"."; // �ʱ� ���丮
			if (GetOpenFileName(&OFN) != 0) { // ���� ���� �Լ� ȣ��
				wsprintf(str, L"%s", OFN.lpstrFile);

				// �ʿ��� ���� ������ ���
				int bufferSize = WideCharToMultiByte(CP_ACP, 0, str, -1, NULL, 0, NULL, NULL);

				// ��ȯ�� ���ڿ��� ������ ���� ���� �Ҵ�
				char* file_name_tmp = new char[bufferSize];
				WideCharToMultiByte(CP_ACP, 0, str, -1, file_name_tmp, bufferSize, NULL, NULL);
				file_name = file_name_tmp;
				
				DisplayText("���� ��� : %s ", file_name);
				MessageBox(hwnd, str, L"���� ����", MB_OK);
			}
			break;

		case IDCANCEL:
			EndDialog(hDlg, IDCANCEL); // ��ȭ���� �ݱ�
			closesocket(sock); // ���� �ݱ�
			return TRUE;
		}
		return FALSE;
	}
	return FALSE;
}

// ����Ʈ ��Ʈ�� ��� �Լ�
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

// ���� �Լ� ���� ���
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

// TCP Ŭ���̾�Ʈ ���� �κ�
DWORD WINAPI ClientMain(LPVOID arg)
{
	int retval;
	char buf_name[BUFSIZE_NAME + 1]; // ���� ���� ������

	// ���� ����
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
		WaitForSingleObject(hWriteEvent, INFINITE); // ���� �Ϸ� ���

		int sendedBytes = 0;
		int totalSendBytes = 0;
		int filesize = 0;

		// ������ ��ſ� ����� ����
		FILE* file = fopen(file_name, "rb");

		// ���� �̸� ����
		std::string file_original_name = std::string(file_name).substr(std::string(file_name).find_last_of("\\") + 1);

		if (file == NULL) {
			DisplayText("������ �����ϴ�. ���α׷� ����\n");
			return 0;
		}

		fseek(file, 0, SEEK_END);
		filesize = ftell(file);
		fseek(file, 0, SEEK_SET);

		// ������ ������ ���
		int len = (int)strlen(file_original_name.c_str());
		strncpy(buf_name, file_original_name.c_str(), len);
		buf_name[len] = '\0';

		//==============================
		// ������ ������(���� �̸� ���� ����)
		//==============================
		retval = send(sock, (char*)&len, sizeof(int), 0);
		if (retval == SOCKET_ERROR) {
			DisplayError("send()");
			return false;
		}

		// ������ ������(���� �̸� ���� ����)
		retval = send(sock, buf_name, len, 0);
		if (retval == SOCKET_ERROR) {
			DisplayError("send()");
			return false;
		}
		DisplayText("[TCP Ŭ���̾�Ʈ] �����̸� : %s�� %d+%d����Ʈ�� "
			"���½��ϴ�.\n", buf_name, (int)sizeof(int), retval);


		//==============================
		// ������ ������(�������� ���� ����)
		//==============================
		DisplayText("**���� ������**\n");
		DisplayText("File size = %d\n", filesize);
		retval = send(sock, (char*)&filesize, sizeof(int), 0);
		if (retval == SOCKET_ERROR) {
			DisplayError("send()");
			return false;
		}
		
		// ������ ������(�������� ���� ����)
		while ((retval = fread(buf, sizeof(char), sizeof(buf), file)) > 0) {
			send(sock, buf, retval, 0);
			totalSendBytes += retval;
			
			// ���α׷��� �� ������Ʈ
			SendMessage(hProgress, PBM_SETPOS, ((double)totalSendBytes / filesize) * 100, 0);
			
			//Sleep(1);
			if (retval == SOCKET_ERROR) {
				DisplayError("send()");
				break;
			}
		}

		DisplayText("[TCP Ŭ���̾�Ʈ] %d+%d����Ʈ�� "
			"���½��ϴ�.\n", (int)sizeof(int), filesize);

		EnableWindow(hSendButton, TRUE); // ������ ��ư Ȱ��ȭ
		SetEvent(hReadEvent); // �б� �Ϸ� �˸�
	}

	return 0;
}
