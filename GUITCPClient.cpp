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

// ��ȭ���� ���ν���
BOOL CALLBACK DlgProc(HWND, UINT, WPARAM, LPARAM);
// ���� ��Ʈ�� ��� �Լ�
void DisplayText(char *fmt, ...);
// ���� ��� �Լ�
void err_quit(char *msg);
void err_display(char *msg);
// ����� ���� ������ ���� �Լ�
int recvn(SOCKET s, char *buf, int len, int flags);
// ���� ��� ������ �Լ�
DWORD WINAPI Main(LPVOID arg);
DWORD WINAPI Receiver(LPVOID arg);

// ����ð� ����Լ�
char* timeToString(struct tm *t);
struct tm *t;
time_t timer;

bool Connect(char * ip, char * port);

SOCKET sock; // ����
char timeBuf[BUFSIZE + 1]; // �ð� ����
char buf[BUFSIZE+1]; // ������ �ۼ��� ����
char sendbuf[BUFSIZE + 1]; // ������ �۽� ����
char name[BUFSIZE + 1]; // �̸� �迭
char ip[20]; // IP �迭
char port[10]; // PORT �迭
char tmpNameBuf[BUFSIZE + 1]; // ����� �̸� �迭

HWND ownerWindow;
HANDLE hReadEvent, hWriteEvent; // �̺�Ʈ
HWND hSendButton; // ������ ��ư
HWND inputTextBox, outputTextBox; // ���� ��Ʈ��
HWND applyButton;
HWND connectButton;
HWND portTextBox;

// �г����� �������� üũ�ϱ� ���� BOOL�� ������
bool nickNameChange = false;


int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	// �̺�Ʈ ����
	hReadEvent = CreateEvent(NULL, FALSE, TRUE, NULL);
	if(hReadEvent == NULL) return 1;
	hWriteEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	if(hWriteEvent == NULL) return 1;

	// ���� ��� ������ ����
	CreateThread(NULL, 0, Main, NULL, 0, NULL);

	// ��ȭ���� ����
	DialogBox(hInstance, MAKEINTRESOURCE(IDD_DIALOG1), NULL, DlgProc);

	// �̺�Ʈ ����
	CloseHandle(hReadEvent);
	CloseHandle(hWriteEvent);

	// closesocket()
	closesocket(sock);

	// ���� ����
	WSACleanup();
	return 0;
}

// ��ȭ���� ���ν���
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
		// �����ư ��Ȱ��ȭ
		EnableWindow(applyButton, FALSE);
		EnableWindow(hSendButton, FALSE);


		return TRUE;
	case WM_COMMAND:
		switch(LOWORD(wParam)){
		// ���� ��ư
		case BUTTON_CONNECT:
			int ipAddress, portAddress;

			GetDlgItemText(hDlg, TEXTBOX_NICKNAME, name, 10);
			GetDlgItemText(hDlg, TEXTBOX_IP, ip, 20);
			GetDlgItemText(hDlg, TEXTBOX_PORT, port, 10);

			// �г����� ������� ���
			if (strlen(name) == 0) {
				MessageBox(NULL, "�г����� �Է��ϼ���", "���", MB_OK);
				break;
			}

			// IP D CLASS ����ó��
			ipAddress = atoi(ip);
			if ((ipAddress < 224) || (ipAddress > 239)) {
				MessageBox(NULL, "IP�� ����� �Է��ϼ���", "���", MB_OK);
				break;
			}

			// PORT ����ó��
			portAddress = atoi(port);
			if ((portAddress < 1024) || (portAddress > 49151)) {
				MessageBox(NULL, "PORT�� ����� �Է��ϼ���", "���", MB_OK);
				break;
			}
			EnableWindow(connectButton, FALSE); // �����ư ��Ȱ��ȭ : ������ ���̻� ������ ���ϰ� ��
			EnableWindow(applyButton, TRUE); // �����ư Ȱ��ȭ : �г����� ������ �� �ְԲ� ��
			SetEvent(hWriteEvent);
			//Connect(ip, port);
			//MessageBoxA(NULL, NULL, "Message", MB_OK | MB_ICONINFORMATION);
			//if (MessageBox(NULL, "YES OR NO?", "Alert Window", MB_YESNO) == IDYES) {
			//	MessageBox(NULL, "YES !", "Yes Man", MB_OK);
			//}
			//else {
			//	MessageBox(NULL, "NO!", "No Man", MB_OK);
			//}

		// �޼��� ������ ��ư
		case BUTTON_SEND:
			EnableWindow(hSendButton, FALSE); // ������ ��ư ��Ȱ��ȭ
			WaitForSingleObject(hReadEvent, INFINITE); // �б� �Ϸ� ��ٸ���
			// ���� �ð� ����
			timer = time(NULL);
			t = localtime(&timer);
			strcpy(timeBuf, timeToString(t));
			// �� ������ ���� (�̸�, ����)
			GetDlgItemText(hDlg, TEXTBOX_NICKNAME, name, BUFSIZE + 1);
			GetDlgItemText(hDlg, TEXTBOX_CONTENT, sendbuf, BUFSIZE + 1);
			SetEvent(hWriteEvent); // ���� �Ϸ� �˸���
			SetFocus(inputTextBox);
			SendMessage(inputTextBox, EM_SETSEL, 0, -1);
			return TRUE;

		// �г��� �����ư(����)
		case BUTTON_APPLY:
			nickNameChange = true;
			EnableWindow(hSendButton, FALSE);
			EnableWindow(applyButton, FALSE);			
			WaitForSingleObject(hReadEvent, INFINITE);
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

// ���� ��Ʈ�� ��� �Լ�
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

// ���� �Լ� ���� ��� �� ����
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

// ���� �Լ� ���� ���
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

// ����� ���� ������ ���� �Լ�
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

// TCP Ŭ���̾�Ʈ ���� �κ�
DWORD WINAPI Main(LPVOID arg)
{
	int retval;
	bool bConnect = false;

	// ���� �ʱ�ȭ
	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
		return 1;

	// socket()
	SOCKET sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock == INVALID_SOCKET) err_quit("socket()");

	// ��Ƽĳ��Ʈ TTL ����
	int ttl = 2;
	retval = setsockopt(sock, IPPROTO_IP, IP_MULTICAST_TTL, (char *)&ttl, sizeof(ttl));
	if (retval == SOCKET_ERROR) err_quit("setsockopt()");

	// ���� �ּ� ����ü �ʱ�ȭ
	SOCKADDR_IN remoteaddr;
	ZeroMemory(&remoteaddr, sizeof(remoteaddr));
	remoteaddr.sin_family = AF_INET;
	remoteaddr.sin_addr.s_addr = inet_addr(MULTICASTIP);
	remoteaddr.sin_port = htons(REMOTEPORT);

	// ������ ��ſ� ����� ����
	//char sendbuf[BUFSIZE + 1];
	int len;
	HANDLE hThread;

	//���ù� ������ ����
	hThread = CreateThread(NULL, 0, Receiver, (LPVOID)sock, 0, NULL);
	if (hThread == NULL) { closesocket(sock); }
	else { CloseHandle(hThread); }

	while (1) {
		WaitForSingleObject(hWriteEvent, INFINITE); // ���� �Ϸ� ��ٸ���

		// connect()
		if (!bConnect) {
			SOCKADDR_IN multiaddr;
			ZeroMemory(&multiaddr, sizeof(multiaddr));
			multiaddr.sin_family = AF_INET;
			multiaddr.sin_addr.s_addr = inet_addr(MULTICASTIP);
			multiaddr.sin_port = htons(REMOTEPORT);
			retval = connect(sock, (SOCKADDR *)&multiaddr, sizeof(multiaddr));
			if (retval == SOCKET_ERROR) err_quit("connect()");
			else {
				char *tempBuf = "ä�ù濡 �����Ͽ����ϴ�";
				DisplayText("ä�ù濡 �����Ͽ����ϴ� :) \r\n", retval);
				bConnect = true;
			}
			continue;
		}

		// �г��ӿ� ���� �����ư�� ������ ���
		if (nickNameChange) {
			if (strlen(tmpNameBuf) == 0) {
				nickNameChange = false;
				EnableWindow(hSendButton, TRUE); // ������ ��ư Ȱ��ȭ
				SetEvent(hReadEvent); // �б� �Ϸ� �˸���
				continue;
			}

			// ����� �г��� ������
			retval = sendto(sock, tmpNameBuf, strlen(tmpNameBuf), 0, (SOCKADDR *)&remoteaddr, sizeof(remoteaddr));
			if (retval == SOCKET_ERROR) {
				err_display("sendto()");
				continue;
			}
			nickNameChange = false;
			SetEvent(hReadEvent); // �б� �Ϸ� �˸���
			continue;
		}

		// �ð� ���̰� 0�̸� ������ ����
		if (strlen(timeBuf) == 0) {
			EnableWindow(hSendButton, TRUE); // ������ ��ư Ȱ��ȭ
			SetEvent(hReadEvent); // �б� �Ϸ� �˸���
			continue;
		}

	    // ���ڿ� ���̰� 0�̸� ������ ����
		if (strlen(sendbuf) == 0) {
			EnableWindow(hSendButton, TRUE); // ������ ��ư Ȱ��ȭ
			SetEvent(hReadEvent); // �б� �Ϸ� �˸���
			continue;
		}

		// �̸��� ���̰� 0�̸� ������ ����
		if (strlen(name) == 0) {
			EnableWindow(hSendButton, TRUE); // ������ ��ư Ȱ��ȭ
			SetEvent(hReadEvent); // �б� �Ϸ� �˸���
			continue;
		}

		// �ð� ������
		retval = sendto(sock, timeBuf, strlen(timeBuf), 0, (SOCKADDR *)&remoteaddr, sizeof(remoteaddr));
		if (retval == SOCKET_ERROR) {
			err_display("sendto()");
			continue;
		}

		// �г��� ������
		retval = sendto(sock, name, strlen(name), 0, (SOCKADDR *)&remoteaddr, sizeof(remoteaddr));
		if (retval == SOCKET_ERROR) {
			err_display("sendto()");
			continue;
		}

		// ���� ������
		retval = sendto(sock, sendbuf, strlen(sendbuf), 0, (SOCKADDR *)&remoteaddr, sizeof(remoteaddr));
		if (retval == SOCKET_ERROR) {
			err_display("sendto()");
			continue;
		}

		//DisplayText("\n[TCP Ŭ���̾�Ʈ] %d����Ʈ�� ���½��ϴ�.\r\n", retval);

		EnableWindow(hSendButton, TRUE); // ������ ��ư Ȱ��ȭ
		SetEvent(hReadEvent); // �б� �Ϸ� �˸���
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
		DisplayText("ä�ù濡 �����Ͽ����ϴ� :) \r\n", retval); 
		return true;
	}
}

DWORD WINAPI Receiver(LPVOID arg)
{
	int retval;

	// ���� �ʱ�ȭ
	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
		return 1;

	// socket()
	SOCKET sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock == INVALID_SOCKET) err_quit("socket()");

	// SO_REUSEADDR �ɼ� ����
	BOOL optval = TRUE;
	retval = setsockopt(sock, SOL_SOCKET,
		SO_REUSEADDR, (char *)&optval, sizeof(optval));
	if (retval == SOCKET_ERROR) err_quit("setsockopt()");

	// bind()
	SOCKADDR_IN localaddr;
	ZeroMemory(&localaddr, sizeof(localaddr));
	localaddr.sin_family = AF_INET;
	localaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	localaddr.sin_port = htons(REMOTEPORT);
	retval = bind(sock, (SOCKADDR *)&localaddr, sizeof(localaddr));
	if (retval == SOCKET_ERROR) err_quit("bind()");

	// ��Ƽĳ��Ʈ �׷� ����
	struct ip_mreq mreq;
	mreq.imr_multiaddr.s_addr = inet_addr(MULTICASTIP);
	mreq.imr_interface.s_addr = htonl(INADDR_ANY);
	retval = setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP,
		(char *)&mreq, sizeof(mreq));
	if (retval == SOCKET_ERROR) err_quit("setsockopt()");

	// ������ ��ſ� ����� ����
	SOCKADDR_IN peeraddr;
	int addrlen;
	char buf[BUFSIZE + 1];
	char name[10];
	// ��Ƽĳ��Ʈ ������ �ޱ�
	while (1) {
		// ������ �ޱ�
		addrlen = sizeof(peeraddr);

		// TEMP DATA �ޱ�

		// �ð� �ޱ�
		retval = recvfrom(sock, timeBuf, BUFSIZE, 0, (SOCKADDR *)&peeraddr, &addrlen);
		if (retval == SOCKET_ERROR) {
			err_display("recvfrom()");
			continue;
		}
		timeBuf[retval] = '\0';

		// �̸� �ޱ�
		retval = recvfrom(sock, name, 10, 0, (SOCKADDR *)&peeraddr, &addrlen);
		if (retval == SOCKET_ERROR) {
			err_display("recvfrom()");
			continue;
		}
		name[retval] = '\0';

		// ������ �ޱ�
		retval = recvfrom(sock, buf, BUFSIZE, 0, (SOCKADDR *)&peeraddr, &addrlen);
		if (retval == SOCKET_ERROR) {
			err_display("recvfrom()");
			continue;
		}

		// ���� ������ ���
		buf[retval] = '\0';
		//printf("\n[UDP/%s:%d] %s\n", inet_ntoa(peeraddr.sin_addr), ntohs(peeraddr.sin_port), buf);
		//printf("%s : %s\n", name, buf);
		DisplayText("[%s] %s : %s\n", timeBuf, name, buf);
		
	}

	// ��Ƽĳ��Ʈ �׷� Ż��
	retval = setsockopt(sock, IPPROTO_IP, IP_DROP_MEMBERSHIP, (char *)&mreq, sizeof(mreq));
	if (retval == SOCKET_ERROR) err_quit("setsockopt()");

	// closesocket()
	closesocket(sock);

	// ���� ����
	WSACleanup();
	return 0;
}

bool receiverAPPLY() {


	return TRUE;
}

bool receiverSEND() {

	return TRUE;
}

// ����ð� ���� �Լ�
char* timeToString(struct tm *t) {
	static char s[20];

	sprintf(s, "%04d-%02d-%02d %02d:%02d:%02d",
		t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
		t->tm_hour, t->tm_min, t->tm_sec
	);

	return s;
}