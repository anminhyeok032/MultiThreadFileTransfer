#include "..\Common.h"

#define SERVERPORT 9000
#define BUFSIZE    1024
#define BUFSIZE_NAME 50
#define LINE_HEIGHT 6

char buf[BUFSIZE + 1]; // 가변 길이 데이터
CRITICAL_SECTION cs; // 임계 섹션



int generateID()
{
	static int s_itemID = 1;
	return s_itemID++; // s_isemID의 복사본을 만들고, 실제 s_isemID를 증가시킨 다음 복사본의 값을 반환한다.
}

void gotoxy(int x, int y) {
	COORD pos = { x, y };
	SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), pos);
}

// 클라이언트와 데이터 통신
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

	int filesize; // 고정 길이 데이터
	char buf_name[BUFSIZE_NAME + 1]; // 가변 길이 데이터
	int len; // 고정 길이 데이터

	// 클라이언트 정보 얻기
	addrlen = sizeof(clientaddr);
	getpeername(client_sock, (struct sockaddr*)&clientaddr, &addrlen);
	inet_ntop(AF_INET, &clientaddr.sin_addr, addr, sizeof(addr));


	// 클라이언트와 데이터 통신
	while (1) {
		//==============================
		// 데이터 받기(파일 이름 고정 길이)
		//==============================
		retval = recv(client_sock, (char*)&len, sizeof(int), MSG_WAITALL);
		if (retval == SOCKET_ERROR) {
			err_display("recv()");
			break;
		}
		else if (retval == 0)
			break;

		// 데이터 받기(파일 이름 가변 길이)
		retval = recv(client_sock, buf_name, len, MSG_WAITALL);
		if (retval == SOCKET_ERROR) {
			err_display("recv()");
			break;
		}
		else if (retval == 0)
			break;

		// 받은 데이터 출력
		buf_name[retval] = '\0';

		//-----------------------------------
		// 아래의 임계 섹션을 사용해 출력 동기화
		EnterCriticalSection(&cs);

		// 출력 위치 지정
		gotoxy(0, ProcessNum - 2);
		printf("[TCP/%s:%d] 파일 이름 : %s\n", addr, ntohs(clientaddr.sin_port), buf_name);


		FILE* file = fopen(buf_name, "wb");
		if (file == NULL) {
			err_display("fopen()");
			break;
		}

		//=============================
		// 데이터 받기(영상파일 고정 길이)
		//=============================
		retval = recv(client_sock, (char*)&filesize, sizeof(int), MSG_WAITALL);

		gotoxy(0, ProcessNum -1);
		printf("[TCP/%s:%d] 파일 사이즈 : %d\n", addr, ntohs(clientaddr.sin_port), filesize);

		LeaveCriticalSection(&cs);
		//-----------------------------------

		if (retval == SOCKET_ERROR) {
			err_display("recv()");
			break;
		}
		else if (retval == 0)
			break;


		int totalReadBytes = 0;

	

		// 데이터 받기(영상파일 가변 길이)
		while (totalReadBytes < filesize) {
			retval = recv(client_sock, buf, BUFSIZE, 0);
			totalReadBytes += retval;

			//-----------------------------------
			// 아래의 임계 섹션을 사용해 출력 동기화
			EnterCriticalSection(&cs);

			// 출력 위치 지정
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

		// 받은 데이터 출력
		printf("[TCP/%s:%d] 파일 전송 완료\n", addr, ntohs(clientaddr.sin_port));

		LeaveCriticalSection(&cs);
		//-----------------------------------

	}

	// 소켓 닫기
	closesocket(client_sock);

	//-----------------------------------
	// 아래의 임계 섹션을 사용해 출력 동기화
	EnterCriticalSection(&cs);
	gotoxy(0, ProcessNum + 3);

	printf("[TCP 서버] 클라이언트 종료: IP 주소=%s, 포트 번호=%d\n",
		addr, ntohs(clientaddr.sin_port));

	// 임계 섹션 삭제
	LeaveCriticalSection(&cs);
	//-----------------------------------

	return 0;
}





int main(int argc, char* argv[])
{
	int retval;

	// 윈속 초기화
	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
		return 1;

	// 소켓 생성
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


	// 데이터 통신에 사용할 변수
	SOCKET client_sock;
	struct sockaddr_in clientaddr;
	int addrlen;
	HANDLE hThread;

	// 출력 위치 동기화를 위한 임계 구역 초기화
	InitializeCriticalSection(&cs);

	while (1) {
		// accept()
		addrlen = sizeof(clientaddr);
		client_sock = accept(listen_sock, (struct sockaddr*)&clientaddr, &addrlen);
		if (client_sock == INVALID_SOCKET) {
			err_display("accept()");
			break;
		}

		// 접속한 클라이언트 정보 출력
		char addr[INET_ADDRSTRLEN];
		inet_ntop(AF_INET, &clientaddr.sin_addr, addr, sizeof(addr));
		printf("\n\n\n[TCP 서버] 클라이언트 접속: IP 주소=%s, 포트 번호=%d\n",
			addr, ntohs(clientaddr.sin_port));



		// 스레드 생성
		hThread = CreateThread(NULL, 0, ProcessClient,
			(LPVOID)client_sock, 0, NULL);
		if (hThread == NULL) { closesocket(client_sock); }
		else { CloseHandle(hThread); }


	}

	// 임계 구역 해제
	DeleteCriticalSection(&cs);

	// 소켓 닫기
	closesocket(listen_sock);

	// 윈속 종료
	WSACleanup();
	return 0;
}