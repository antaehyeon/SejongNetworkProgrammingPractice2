#pragma comment(lib, "ws2_32")
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>
#include <sstream>
#include <time.h>
#include "resource.h"

#define MULTICASTIP "235.7.8.1"
#define REMOTEPORT  9000
#define BUFSIZE     512

// 대화상자 프로시저
BOOL CALLBACK DlgProc(HWND, UINT, WPARAM, LPARAM);
// 편집 컨트롤 출력 함수
void DisplayText(char *fmt, ...);
// 오류 출력 함수
void err_quit(char *msg);
void err_display(char *msg);
// 사용자 정의 데이터 수신 함수
int recvn(SOCKET s, char *buf, int len, int flags);
// 소켓 통신 스레드 함수
DWORD WINAPI Main(LPVOID arg);
DWORD WINAPI Receiver(LPVOID arg);

// 현재시간 출력함수
char* timeToString();
struct tm *t;
time_t timer;

bool Connect(char * ip, char * port);

SOCKET sock; // 소켓
char timeBuf[BUFSIZE + 1]; // 시간 버퍼
char buf[BUFSIZE+1]; // 데이터 송수신 버퍼
char sendbuf[BUFSIZE + 1]; // 데이터 송신 버퍼
char name[BUFSIZE + 1]; // 이름 배열
char ip[20]; // IP 배열
char port[10]; // PORT 배열
char tmpNameBuf[BUFSIZE + 1]; // 변경된 이름 배열
char ownNameBuf[BUFSIZE + 1];

HWND ownerWindow;
HANDLE hReadEvent, hWriteEvent; // 이벤트
HWND hSendButton; // 보내기 버튼
HWND inputTextBox, outputTextBox; // 편집 컨트롤
HWND applyButton;
HWND connectButton;
HWND portTextBox;

// 닉네임이 변경됬는지 체크하기 위한 BOOL형 데이터
bool nickNameChange = false;
// 재 연결을 체크하기 위한 BOOL 형 데이터
bool bReConnect = false;
// RECEIVER MODE SELECTOR
int receiverModeSelector = 0;
const int SEND = 1;
const int APPLY = 2;

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	// 이벤트 생성
	hReadEvent = CreateEvent(NULL, FALSE, TRUE, NULL);
	if(hReadEvent == NULL) return 1;
	hWriteEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	if(hWriteEvent == NULL) return 1;

	// 소켓 통신 스레드 생성
	CreateThread(NULL, 0, Main, NULL, 0, NULL);

	// 대화상자 생성
	DialogBox(hInstance, MAKEINTRESOURCE(IDD_DIALOG1), NULL, DlgProc);

	// 이벤트 제거
	CloseHandle(hReadEvent);
	CloseHandle(hWriteEvent);

	// closesocket()
	closesocket(sock);

	// 윈속 종료
	WSACleanup();
	return 0;
}

// 대화상자 프로시저
BOOL CALLBACK DlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch(uMsg){
	case WM_INITDIALOG:
		ownerWindow = GetDlgItem(hDlg, IDD_DIALOG1);
		inputTextBox = GetDlgItem(hDlg, TEXTBOX_CONTENT);
		outputTextBox = GetDlgItem(hDlg, TEXT_CONTENT);
		hSendButton = GetDlgItem(hDlg, BUTTON_SEND);
		applyButton = GetDlgItem(hDlg, BUTTON_APPLY);
		portTextBox = GetDlgItem(hDlg, TEXTBOX_PORT);
		connectButton = GetDlgItem(hDlg, BUTTON_CONNECT);

		SendMessage(inputTextBox, EM_SETLIMITTEXT, BUFSIZE, 0);
		// 적용버튼 비활성화
		EnableWindow(applyButton, FALSE);
		EnableWindow(hSendButton, FALSE);
		return TRUE;
	case WM_COMMAND:
		switch(LOWORD(wParam)){
		// 연결 버튼
		case BUTTON_CONNECT:
			int ipAddress, portAddress;

			GetDlgItemText(hDlg, TEXTBOX_NICKNAME, ownNameBuf, BUFSIZE + 1);
			GetDlgItemText(hDlg, TEXTBOX_NICKNAME, name, 10);
			GetDlgItemText(hDlg, TEXTBOX_IP, ip, 20);
			GetDlgItemText(hDlg, TEXTBOX_PORT, port, 10);

			// 닉네임이 비어있을 경우
			if (strlen(name) == 0) {
				MessageBox(NULL, "닉네임을 입력하세요", "경고", MB_OK);
				break;
			}

			// IP D CLASS 예외처리
			ipAddress = atoi(ip);
			if ((ipAddress < 224) || (ipAddress > 239)) {
				MessageBox(NULL, "IP를 제대로 입력하세요", "경고", MB_OK);
				break;
			}

			// PORT 예외처리
			portAddress = atoi(port);
			if ((portAddress < 1024) || (portAddress > 49151)) {
				MessageBox(NULL, "PORT를 제대로 입력하세요", "경고", MB_OK);
				break;
			}
			EnableWindow(connectButton, FALSE); // 연결버튼 비활성화 : 연결을 더이상 누르지 못하게 함
			EnableWindow(applyButton, TRUE); // 적용버튼 활성화 : 닉네임을 변경할 수 있게끔 함
			SetEvent(hWriteEvent);
			//Connect(ip, port);
			//MessageBoxA(NULL, NULL, "Message", MB_OK | MB_ICONINFORMATION);
			//if (MessageBox(NULL, "YES OR NO?", "Alert Window", MB_YESNO) == IDYES) {
			//	MessageBox(NULL, "YES !", "Yes Man", MB_OK);
			//}
			//else {
			//	MessageBox(NULL, "NO!", "No Man", MB_OK);
			//}

		// 메세지 보내기 버튼
		case BUTTON_SEND:
			receiverModeSelector = SEND;
			EnableWindow(hSendButton, FALSE); // 보내기 버튼 비활성화
			WaitForSingleObject(hReadEvent, INFINITE); // 읽기 완료 기다리기
			// 현재 시간 추출
			strcpy(timeBuf, timeToString());
			// 각 데이터 추출 (이름, 내용)
			GetDlgItemText(hDlg, TEXTBOX_NICKNAME, name, BUFSIZE + 1);
			GetDlgItemText(hDlg, TEXTBOX_CONTENT, sendbuf, BUFSIZE + 1);
			SetEvent(hWriteEvent); // 쓰기 완료 알리기
			SetFocus(inputTextBox);
			SendMessage(inputTextBox, EM_SETSEL, 0, -1);
			return TRUE;

		// 닉네임 변경버튼(적용)
		case BUTTON_APPLY:
			receiverModeSelector = APPLY;
			nickNameChange = true;
			EnableWindow(hSendButton, FALSE);
			EnableWindow(applyButton, FALSE);			
			WaitForSingleObject(hReadEvent, INFINITE);
			// 현재 시간 추출
			timer = time(NULL);
			t = localtime(&timer);
			strcpy(timeBuf, timeToString());
			// 바뀐 닉네임 추출
			GetDlgItemText(hDlg, TEXTBOX_NICKNAME, tmpNameBuf, BUFSIZE + 1);
			SetEvent(hWriteEvent);
			SetFocus(inputTextBox);
			SendMessage(inputTextBox, EM_SETSEL, 0, -1);
			return TRUE;	

		case BUTTON_EXIT:
			EndDialog(hDlg, IDCANCEL);
			return TRUE;
		}
		return FALSE;
	}
	return FALSE;
}

// 편집 컨트롤 출력 함수
void DisplayText(char *fmt, ...)
{
	va_list arg;
	va_start(arg, fmt);

	char cbuf[BUFSIZE+256];
	vsprintf(cbuf, fmt, arg);

	int nLength = GetWindowTextLength(outputTextBox);
	SendMessage(outputTextBox, EM_SETSEL, nLength, nLength);
	SendMessage(outputTextBox, EM_REPLACESEL, FALSE, (LPARAM)cbuf);

	va_end(arg);
}

// 소켓 함수 오류 출력 후 종료
void err_quit(char *msg)
{
	LPVOID lpMsgBuf;
	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_SYSTEM,
		NULL, WSAGetLastError(),
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR)&lpMsgBuf, 0, NULL);
	MessageBox(NULL, (LPCTSTR)lpMsgBuf, msg, MB_ICONERROR);
	LocalFree(lpMsgBuf);
	exit(1);
}

// 소켓 함수 오류 출력
void err_display(char *msg)
{
	LPVOID lpMsgBuf;
	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_SYSTEM,
		NULL, WSAGetLastError(),
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR)&lpMsgBuf, 0, NULL);
	DisplayText("[%s] %s", msg, (char *)lpMsgBuf);
	LocalFree(lpMsgBuf);
}

// 사용자 정의 데이터 수신 함수
int recvn(SOCKET s, char *buf, int len, int flags)
{
	int received;
	char *ptr = buf;
	int left = len;

	while(left > 0){
		received = recv(s, ptr, left, flags);
		if(received == SOCKET_ERROR)
			return SOCKET_ERROR;
		else if(received == 0)
			break;
		left -= received;
		ptr += received;
	}

	return (len - left);
}

// TCP 클라이언트 시작 부분
DWORD WINAPI Main(LPVOID arg)
{
	int retval;
	bool bConnect = false;

	// 윈속 초기화
	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
		return 1;

	// socket()
	SOCKET sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock == INVALID_SOCKET) err_quit("socket()");

	// 멀티캐스트 TTL 설정
	int ttl = 2;
	retval = setsockopt(sock, IPPROTO_IP, IP_MULTICAST_TTL, (char *)&ttl, sizeof(ttl));
	if (retval == SOCKET_ERROR) err_quit("setsockopt()");

	// 데이터 통신에 사용할 변수
	//char sendbuf[BUFSIZE + 1];
	int len;
	HANDLE hThread;
	SOCKADDR_IN remoteaddr;
	SOCKADDR_IN multiaddr;

	while (1) {
		WaitForSingleObject(hWriteEvent, INFINITE); // 쓰기 완료 기다리기

		if (!bConnect) {
			// 소켓 주소 구조체 초기화
			ZeroMemory(&remoteaddr, sizeof(remoteaddr));
			remoteaddr.sin_family = AF_INET;
			remoteaddr.sin_addr.s_addr = inet_addr(ip);
			remoteaddr.sin_port = htons((u_short)port);

			// 리시버 스레드 생성
			hThread = CreateThread(NULL, 0, Receiver, (LPVOID)sock, 0, NULL);
			if (hThread == NULL) { closesocket(sock); }
			else { CloseHandle(hThread); }

			// Connect
			ZeroMemory(&multiaddr, sizeof(multiaddr));
			multiaddr.sin_family = AF_INET;
			multiaddr.sin_addr.s_addr = inet_addr(ip);
			multiaddr.sin_port = htons((u_short)port);
			retval = connect(sock, (SOCKADDR *)&multiaddr, sizeof(multiaddr));
			if (retval == SOCKET_ERROR) err_quit("connect()");
			else {
				char *tempBuf = "채팅방에 접속하였습니다";
				DisplayText("채팅방에 접속하였습니다 :) \r\n", retval);
				bConnect = true;
			}
			continue;
		}

		// 새로운 주소로 Connect 하는 과정
		if (bReConnect) {
			remoteaddr.sin_addr.s_addr = inet_addr(ip);
			remoteaddr.sin_port = htons((u_short)port);

			multiaddr.sin_addr.s_addr = inet_addr(ip);
			multiaddr.sin_port = htons((u_short)port);
			retval = connect(sock, (SOCKADDR *)&multiaddr, sizeof(multiaddr));
			if (retval == SOCKET_ERROR) err_quit("connect()");
			else {
				DisplayText("새로운 채팅방에 접속하였습니다 :) \r\n");
				bReConnect = false;
			}
		}

		// 닉네임에 대한 적용버튼을 눌렀을 경우
		if (nickNameChange) {
			if (strlen(tmpNameBuf) == 0) {
				nickNameChange = false;
				EnableWindow(hSendButton, TRUE); // 보내기 버튼 활성화
				SetEvent(hReadEvent); // 읽기 완료 알리기
				continue;
			}
			// 시간 보내기
			strcpy(timeBuf, "APPLY");
			retval = sendto(sock, timeBuf, strlen(timeBuf), 0, (SOCKADDR *
				)&remoteaddr, sizeof(remoteaddr));
			if (retval == SOCKET_ERROR) {
				err_display("sendto()");
				continue;
			}

			// 원래 닉네임 보내기
			retval = sendto(sock, ownNameBuf, strlen(ownNameBuf), 0, (SOCKADDR *)&remoteaddr, sizeof(remoteaddr));
			if (retval == SOCKET_ERROR) {
				err_display("sendto()");
				continue;
			}

			// 변경된 닉네임 보내기
			retval = sendto(sock, tmpNameBuf, strlen(tmpNameBuf), 0, (SOCKADDR *)&remoteaddr, sizeof(remoteaddr));
			if (retval == SOCKET_ERROR) {
				err_display("sendto()");
				continue;
			}
			nickNameChange = false;
			EnableWindow(applyButton, TRUE); // 적용 버튼 활성화
			EnableWindow(hSendButton, TRUE); // 보내기 버튼 활성화
			SetEvent(hReadEvent); // 읽기 완료 알리기
			continue;
		}

		// 시간 길이가 0이면 보내지 않음
		if (strlen(timeBuf) == 0) {
			EnableWindow(hSendButton, TRUE); // 보내기 버튼 활성화
			SetEvent(hReadEvent); // 읽기 완료 알리기
			continue;
		}

	    // 문자열 길이가 0이면 보내지 않음
		if (strlen(sendbuf) == 0) {
			EnableWindow(hSendButton, TRUE); // 보내기 버튼 활성화
			SetEvent(hReadEvent); // 읽기 완료 알리기
			continue;
		}

		// 이름의 길이가 0이면 보내지 않음
		if (strlen(name) == 0) {
			EnableWindow(hSendButton, TRUE); // 보내기 버튼 활성화
			SetEvent(hReadEvent); // 읽기 완료 알리기
			continue;
		}

		// 시간 보내기
		retval = sendto(sock, timeBuf, strlen(timeBuf), 0, (SOCKADDR *)&remoteaddr, sizeof(remoteaddr));
		if (retval == SOCKET_ERROR) {
			err_display("sendto()");
			continue;
		}

		// 닉네임 보내기
		retval = sendto(sock, name, strlen(name), 0, (SOCKADDR *)&remoteaddr, sizeof(remoteaddr));
		if (retval == SOCKET_ERROR) {
			err_display("sendto()");
			continue;
		}

		// 내용 보내기
		retval = sendto(sock, sendbuf, strlen(sendbuf), 0, (SOCKADDR *)&remoteaddr, sizeof(remoteaddr));
		if (retval == SOCKET_ERROR) {
			err_display("sendto()");
			continue;
		}

		//DisplayText("\n[TCP 클라이언트] %d바이트를 보냈습니다.\r\n", retval);

		EnableWindow(hSendButton, TRUE); // 보내기 버튼 활성화
		SetEvent(hReadEvent); // 읽기 완료 알리기
	}
}

bool Connect(char * ip, char * port) {
	int retval;
	SOCKADDR_IN multiaddr;

	ZeroMemory(&multiaddr, sizeof(multiaddr));
	multiaddr.sin_family = AF_INET;
	multiaddr.sin_addr.s_addr = inet_addr(ip);
	multiaddr.sin_port = htons((u_short)port);
	retval = connect(sock, (SOCKADDR *)&multiaddr, sizeof(multiaddr));
	if (retval == SOCKET_ERROR) {
		err_quit("connect()");
		return false;
	}
	else { 
		DisplayText("채팅방에 접속하였습니다 :) \r\n", retval); 
		return true;
	}
}

DWORD WINAPI Receiver(LPVOID arg)
{
	int retval;

	// 윈속 초기화
	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
		return 1;

	// socket()
	SOCKET sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock == INVALID_SOCKET) err_quit("socket()");

	// SO_REUSEADDR 옵션 설정
	BOOL optval = TRUE;
	retval = setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char *)&optval, sizeof(optval));
	if (retval == SOCKET_ERROR) err_quit("setsockopt()");

	// bind()
	SOCKADDR_IN localaddr;
	ZeroMemory(&localaddr, sizeof(localaddr));
	localaddr.sin_family = AF_INET;
	localaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	localaddr.sin_port = htons((u_short)port);
	retval = bind(sock, (SOCKADDR *)&localaddr, sizeof(localaddr));
	if (retval == SOCKET_ERROR) err_quit("bind()");

	// 멀티캐스트 그룹 가입
	struct ip_mreq mreq;
	mreq.imr_multiaddr.s_addr = inet_addr(ip);
	mreq.imr_interface.s_addr = htonl(INADDR_ANY);
	retval = setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *)&mreq, sizeof(mreq));
	if (retval == SOCKET_ERROR) err_quit("setsockopt()");

	// 데이터 통신에 사용할 변수
	SOCKADDR_IN peeraddr;
	int addrlen;
	char buf[BUFSIZE + 1];
	char name[10];
	// 멀티캐스트 데이터 받기
	while (1) {
		// 데이터 받기
		addrlen = sizeof(peeraddr);

		// 시간 받기
		retval = recvfrom(sock, timeBuf, BUFSIZE, 0, (SOCKADDR *)&peeraddr, &addrlen);
		if (retval == SOCKET_ERROR) {
			err_display("recvfrom()");
			continue;
		}
		timeBuf[retval] = '\0';

		if (strcmp(timeBuf, "APPLY") == 0) {
			receiverModeSelector = APPLY;
			strcpy(timeBuf, timeToString());
		}

		// NEW 데이터 받기
		switch (receiverModeSelector) {
			case SEND:
				// 이름 받기
				retval = recvfrom(sock, name, BUFSIZE, 0, (SOCKADDR *)&peeraddr, &addrlen);
				if (retval == SOCKET_ERROR) {
					err_display("recvfrom()");
					continue;
				}
				name[retval] = '\0';

				// 데이터 받기
				retval = recvfrom(sock, buf, BUFSIZE, 0, (SOCKADDR *)&peeraddr, &addrlen);
				if (retval == SOCKET_ERROR) {
					err_display("recvfrom()");
					continue;
				}
				buf[retval] = '\0';

				// 받은 데이터 출력
				//printf("\n[UDP/%s:%d] %s\n", inet_ntoa(peeraddr.sin_addr), ntohs(peeraddr.sin_port), buf);
				DisplayText("[%s | %s] %s : %s\n", timeBuf, inet_ntoa(peeraddr.sin_addr), name, buf);
				break;
			case APPLY:
				// 원래 이름 받기 (임시로)
				retval = recvfrom(sock, name, BUFSIZE, 0, (SOCKADDR *)&peeraddr, &addrlen);
				if (retval == SOCKET_ERROR) {
					err_display("recvfrom()");
					continue;
				}
				name[retval] = '\0';

				// 변경된 이름 받기
				retval = recvfrom(sock, tmpNameBuf, BUFSIZE, 0, (SOCKADDR *)&peeraddr, &addrlen);
				if (retval == SOCKET_ERROR) {
					err_display("recvfrom()");
					continue;
				}
				tmpNameBuf[retval] = '\0';

				DisplayText("[%s] %s 님이 %s으로 닉네임을 변경하셨습니다.\n", timeBuf, name, tmpNameBuf);
				strcpy(ownNameBuf, tmpNameBuf);
				receiverModeSelector = SEND;

				strcpy(ip, "235.7.8.2");

				retval = setsockopt(sock, IPPROTO_IP, IP_DROP_MEMBERSHIP, (char *)&mreq, sizeof(mreq));
				if (retval == SOCKET_ERROR) err_quit("setsockopt()");

				mreq.imr_multiaddr.s_addr = inet_addr(ip);
				mreq.imr_interface.s_addr = htonl(INADDR_ANY);
				retval = setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *)&mreq, sizeof(mreq));
				if (retval == SOCKET_ERROR) err_quit("setsockopt()");
				else {
					bReConnect = true;
					DisplayText("IP가 변경되었습니다\n");
				}
				break;
		}
	}

	// 멀티캐스트 그룹 탈퇴
	retval = setsockopt(sock, IPPROTO_IP, IP_DROP_MEMBERSHIP, (char *)&mreq, sizeof(mreq));
	if (retval == SOCKET_ERROR) err_quit("setsockopt()");

	// closesocket()
	closesocket(sock);

	// 윈속 종료
	WSACleanup();
	return 0;
}

// 현재시간 리턴 함수
char* timeToString() {
	timer = time(NULL);
	t = localtime(&timer);

	static char s[20];

	sprintf(s, "%04d-%02d-%02d %02d:%02d:%02d",
		t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
		t->tm_hour, t->tm_min, t->tm_sec
	);

	return s;
}