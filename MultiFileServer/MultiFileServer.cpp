#include "..\Common.h"

#define SERVERPORT 9000
#define BUFSIZE    1024
#define BUFSIZE_NAME 50
#define LINE_HEIGHT 6

char buf[BUFSIZE + 1]; // ���� ���� ������
CRITICAL_SECTION cs; // �Ӱ� ����



int generateID()
{
	static int s_itemID = 1;
	return s_itemID++; // s_isemID�� ���纻�� �����, ���� s_isemID�� ������Ų ���� ���纻�� ���� ��ȯ�Ѵ�.
}

void gotoxy(int x, int y) {
	COORD pos = { x, y };
	SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), pos);
}

// Ŭ���̾�Ʈ�� ������ ���
DWORD WINAPI ProcessClient(LPVOID arg)
{
	int id = generateID();
	int ProcessNum = id * LINE_HEIGHT;
	if (id != 1)
	{
		ProcessNum += id + (id-2) ;
	}

	
	int retval;
	SOCKET client_sock = (SOCKET)arg;
	struct sockaddr_in clientaddr;
	char addr[INET_ADDRSTRLEN];
	int addrlen;
	char buf[BUFSIZE + 1];

	int filesize; // ���� ���� ������
	char buf_name[BUFSIZE_NAME + 1]; // ���� ���� ������
	int len; // ���� ���� ������

	// Ŭ���̾�Ʈ ���� ���
	addrlen = sizeof(clientaddr);
	getpeername(client_sock, (struct sockaddr*)&clientaddr, &addrlen);
	inet_ntop(AF_INET, &clientaddr.sin_addr, addr, sizeof(addr));


	// Ŭ���̾�Ʈ�� ������ ���
	while (1) {
		//==============================
		// ������ �ޱ�(���� �̸� ���� ����)
		//==============================
		retval = recv(client_sock, (char*)&len, sizeof(int), MSG_WAITALL);
		if (retval == SOCKET_ERROR) {
			err_display("recv()");
			break;
		}
		else if (retval == 0)
			break;

		// ������ �ޱ�(���� �̸� ���� ����)
		retval = recv(client_sock, buf_name, len, MSG_WAITALL);
		if (retval == SOCKET_ERROR) {
			err_display("recv()");
			break;
		}
		else if (retval == 0)
			break;

		// ���� ������ ���
		buf_name[retval] = '\0';

		//-----------------------------------
		// �Ʒ��� �Ӱ� ������ ����� ��� ����ȭ
		EnterCriticalSection(&cs);

		// ��� ��ġ ����
		gotoxy(0, ProcessNum - 2);
		printf("[TCP/%s:%d] ���� �̸� : %s\n", addr, ntohs(clientaddr.sin_port), buf_name);


		FILE* file = fopen(buf_name, "wb");
		if (file == NULL) {
			err_display("fopen()");
			break;
		}

		//=============================
		// ������ �ޱ�(�������� ���� ����)
		//=============================
		retval = recv(client_sock, (char*)&filesize, sizeof(int), MSG_WAITALL);

		gotoxy(0, ProcessNum -1);
		printf("[TCP/%s:%d] ���� ������ : %d\n", addr, ntohs(clientaddr.sin_port), filesize);

		LeaveCriticalSection(&cs);
		//-----------------------------------

		if (retval == SOCKET_ERROR) {
			err_display("recv()");
			break;
		}
		else if (retval == 0)
			break;


		int totalReadBytes = 0;

	

		// ������ �ޱ�(�������� ���� ����)
		while (totalReadBytes < filesize) {
			retval = recv(client_sock, buf, BUFSIZE, 0);
			totalReadBytes += retval;

			//-----------------------------------
			// �Ʒ��� �Ӱ� ������ ����� ��� ����ȭ
			EnterCriticalSection(&cs);

			// ��� ��ġ ����
			gotoxy(0, ProcessNum);
			printf("In progress: %d/%dByte(s) [%.2f%%]\r\n\n", totalReadBytes, filesize, ((double)totalReadBytes / filesize) * 100);
			
			LeaveCriticalSection(&cs);
			//-----------------------------------

			if (retval == SOCKET_ERROR) {
				err_display("recv()");
				break;
			}
			fwrite(buf, sizeof(char), retval, file);
		}

		//-----------------------------------
		EnterCriticalSection(&cs);

		gotoxy(0, ProcessNum + 2);

		// ���� ������ ���
		printf("[TCP/%s:%d] ���� ���� �Ϸ�\n", addr, ntohs(clientaddr.sin_port));

		LeaveCriticalSection(&cs);
		//-----------------------------------

	}

	// ���� �ݱ�
	closesocket(client_sock);

	//-----------------------------------
	// �Ʒ��� �Ӱ� ������ ����� ��� ����ȭ
	EnterCriticalSection(&cs);
	gotoxy(0, ProcessNum + 3);

	printf("[TCP ����] Ŭ���̾�Ʈ ����: IP �ּ�=%s, ��Ʈ ��ȣ=%d\n",
		addr, ntohs(clientaddr.sin_port));

	// �Ӱ� ���� ����
	LeaveCriticalSection(&cs);
	//-----------------------------------

	return 0;
}





int main(int argc, char* argv[])
{
	int retval;

	// ���� �ʱ�ȭ
	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
		return 1;

	// ���� ����
	SOCKET listen_sock = socket(AF_INET, SOCK_STREAM, 0);
	if (listen_sock == INVALID_SOCKET) err_quit("socket()");

	// bind()
	struct sockaddr_in serveraddr;
	memset(&serveraddr, 0, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
	serveraddr.sin_port = htons(SERVERPORT);
	retval = bind(listen_sock, (struct sockaddr*)&serveraddr, sizeof(serveraddr));
	if (retval == SOCKET_ERROR) err_quit("bind()");

	// listen()
	retval = listen(listen_sock, SOMAXCONN);
	if (retval == SOCKET_ERROR) err_quit("listen()");


	// ������ ��ſ� ����� ����
	SOCKET client_sock;
	struct sockaddr_in clientaddr;
	int addrlen;
	HANDLE hThread;

	// ��� ��ġ ����ȭ�� ���� �Ӱ� ���� �ʱ�ȭ
	InitializeCriticalSection(&cs);

	while (1) {
		// accept()
		addrlen = sizeof(clientaddr);
		client_sock = accept(listen_sock, (struct sockaddr*)&clientaddr, &addrlen);
		if (client_sock == INVALID_SOCKET) {
			err_display("accept()");
			break;
		}

		// ������ Ŭ���̾�Ʈ ���� ���
		char addr[INET_ADDRSTRLEN];
		inet_ntop(AF_INET, &clientaddr.sin_addr, addr, sizeof(addr));
		printf("\n\n\n[TCP ����] Ŭ���̾�Ʈ ����: IP �ּ�=%s, ��Ʈ ��ȣ=%d\n",
			addr, ntohs(clientaddr.sin_port));



		// ������ ����
		hThread = CreateThread(NULL, 0, ProcessClient,
			(LPVOID)client_sock, 0, NULL);
		if (hThread == NULL) { closesocket(client_sock); }
		else { CloseHandle(hThread); }


	}

	// �Ӱ� ���� ����
	DeleteCriticalSection(&cs);

	// ���� �ݱ�
	closesocket(listen_sock);

	// ���� ����
	WSACleanup();
	return 0;
}