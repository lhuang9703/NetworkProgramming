#pragma once
#pragma warning(disable : 4996)

#include "initsock.h"
#include <Mswsock.h>
#include <stdio.h>
#include <windows.h>
#include<set>
#include<string>
#include<time.h>
using namespace std;

CInitSock theSock;

#define BUFFER_SIZE 1024

DWORD WINAPI ServerThread(LPVOID lpParam);
struct get_user_num {
	int client_num;
	CRITICAL_SECTION n;
	get_user_num() {
		client_num = 0;
		::InitializeCriticalSection(&n);
	}
}cal;

set<int> userlist;

int add() {
	EnterCriticalSection(&cal.n);
	cal.client_num++;
	userlist.insert(cal.client_num);
	printf("��ǰ�û��б�Ϊ��\n");
	for (set<int>::iterator it = userlist.begin(); it != userlist.end(); it++) {
		printf("%d ", *it);
	}
	printf("\n");
	LeaveCriticalSection(&cal.n);
	return cal.client_num;
}
typedef struct _SOCKET_OBJ
{
	SOCKET s;						// �׽��־��
	int nOutstandingOps;			// ��¼���׽����ϵ��ص�I/O����

	LPFN_ACCEPTEX lpfnAcceptEx;		// ��չ����AcceptEx��ָ�루���Լ����׽��ֶ��ԣ�
} SOCKET_OBJ, *PSOCKET_OBJ;

typedef struct _BUFFER_OBJ
{
	OVERLAPPED ol;			// �ص��ṹ
	char *buff;				// send/recv/AcceptEx��ʹ�õĻ�����
	int nLen;				// buff�ĳ���
	PSOCKET_OBJ pSocket;	// ��I/O�������׽��ֶ���

	int nOperation;			// �ύ�Ĳ�������
#define OP_ACCEPT	1
#define OP_READ		2
#define OP_WRITE	3

	SOCKET sAccept;			// ��������AcceptEx���ܵĿͻ��׽��֣����Լ����׽��ֶ��ԣ�
	_BUFFER_OBJ *pNext;
	FILE* fd;
	char *fileName;
	int id;
	int seq;
} BUFFER_OBJ, *PBUFFER_OBJ;

// �׽��ֶ���
/*typedef struct _SOCKET_OBJ
{
SOCKET s;					// �׽��־��
HANDLE event;				// ����׽�����������¼�������
sockaddr_in addrRemote;		// �ͻ��˵�ַ��Ϣ

_SOCKET_OBJ *pNext;			// ָ����һ��SOCKET_OBJ����Ϊ��������һ����
} SOCKET_OBJ, *PSOCKET_OBJ;
*/
// �̶߳���
typedef struct _THREAD_OBJ
{
	HANDLE events[WSA_MAXIMUM_WAIT_EVENTS];	// ��¼��ǰ�߳�Ҫ�ȴ����¼�����ľ��
	int nBufferCount;						// ��¼��ǰ�̴߳�����׽��ֵ����� <=  WSA_MAXIMUM_WAIT_EVENTS

	PBUFFER_OBJ pBufferHeader;				// ��ǰ�̴߳�����׽��ֶ����б�pSockHeaderָ���ͷ
	PBUFFER_OBJ pBufferTail;					// pSockTailָ���β

	CRITICAL_SECTION cs;					// �ؼ�����α�����Ϊ����ͬ���Ա��ṹ�ķ���
	_THREAD_OBJ *pNext;	 // ָ����һ��THREAD_OBJ����Ϊ��������һ����
	static int total_thread_num;

} THREAD_OBJ, *PTHREAD_OBJ;

// �߳��б�
PTHREAD_OBJ g_pThreadList;		// ָ���̶߳����б��ͷ
CRITICAL_SECTION g_cs;			// ͬ���Դ�ȫ�ֱ����ķ���


								// ״̬��Ϣ
LONG total_thread_num;		// �ܹ���������
LONG g_nTatolConnections;
LONG g_nCurrentConnections;		// ��ǰ��������
LPFN_GETACCEPTEXSOCKADDRS m_lpfnGetAcceptExSockaddrs = NULL;



// �����׽��ֶ�����ͷ��׽��ֶ���ĺ���
PSOCKET_OBJ GetSocketObj(SOCKET s)
{
	PSOCKET_OBJ pSocket = (PSOCKET_OBJ)::GlobalAlloc(GPTR, sizeof(SOCKET_OBJ));
	if (pSocket != NULL)
	{
		pSocket->s = s;
	}
	return pSocket;
}
void FreeSocketObj(PSOCKET_OBJ pSocket)
{
	if (pSocket->s != INVALID_SOCKET)
		::closesocket(pSocket->s);
	::GlobalFree(pSocket);

}

PBUFFER_OBJ GetBufferObj(PSOCKET_OBJ pSocket, ULONG nLen)
{
	PBUFFER_OBJ pBuffer = (PBUFFER_OBJ)::GlobalAlloc(GPTR, sizeof(BUFFER_OBJ));
	if (pBuffer != NULL)
	{
		pBuffer->buff = (char*)::GlobalAlloc(GPTR, nLen);
		pBuffer->ol.hEvent = ::WSACreateEvent();
		pBuffer->pSocket = pSocket;
		pBuffer->sAccept = INVALID_SOCKET;
	}
	return pBuffer;
}

void FreeBufferObj(PBUFFER_OBJ pBuffer, PTHREAD_OBJ pthread)
{
	// ���б����Ƴ�BUFFER_OBJ����
	::EnterCriticalSection(&pthread->cs);
	PBUFFER_OBJ pTest = pthread->pBufferHeader;
	BOOL bFind = FALSE;
	if (pTest == pBuffer)
	{
		pthread->pBufferHeader = pthread->pBufferTail = NULL;
		bFind = TRUE;
	}
	else
	{
		while (pTest != NULL && pTest->pNext != pBuffer)
			pTest = pTest->pNext;
		if (pTest != NULL)
		{
			pTest->pNext = pBuffer->pNext;
			if (pTest->pNext == NULL)
				pthread->pBufferTail = pTest;
			bFind = TRUE;
		}
	}
	// �ͷ���ռ�õ��ڴ�ռ�
	if (bFind)
	{
		pthread->nBufferCount--;
		::CloseHandle(pBuffer->ol.hEvent);
		::GlobalFree(pBuffer->buff);
		::GlobalFree(pBuffer);
	}

	::LeaveCriticalSection(&pthread->cs);
}

PBUFFER_OBJ FindBufferObj(HANDLE hEvent, PTHREAD_OBJ pthread)
{
	PBUFFER_OBJ pBuffer = pthread->pBufferHeader;
	while (pBuffer != NULL)
	{
		if (pBuffer->ol.hEvent == hEvent)
			break;
		pBuffer = pBuffer->pNext;
	}
	return pBuffer;
}

void RebuildArray(PTHREAD_OBJ pthread)
{
	PBUFFER_OBJ pBuffer = pthread->pBufferHeader;
	int i = 1;
	while (pBuffer != NULL)
	{
		pthread->events[i++] = pBuffer->ol.hEvent;
		pBuffer = pBuffer->pNext;
	}
}

// ����һ���̶߳��󣬳�ʼ�����ĳ�Ա����������ӵ��̶߳����б���
PTHREAD_OBJ GetThreadObj()
{
	PTHREAD_OBJ pThread = (PTHREAD_OBJ)::GlobalAlloc(GPTR, sizeof(THREAD_OBJ));
	if (pThread != NULL)
	{
		::InitializeCriticalSection(&pThread->cs);
		// ����һ���¼���������ָʾ���̵߳ľ��������Ҫ����
		pThread->events[0] = ::WSACreateEvent();

		// ����������̶߳�����ӵ��б���
		::EnterCriticalSection(&g_cs);
		pThread->pNext = g_pThreadList;
		g_pThreadList = pThread;
		//pThread->total_thread_num++;
		total_thread_num++;
		::LeaveCriticalSection(&g_cs);
		printf("�����µ��߳�!�߳���: %d\n", total_thread_num);
	}
	return pThread;
}

// �ͷ�һ���̶߳��󣬲��������̶߳����б����Ƴ�
void FreeThreadObj(PTHREAD_OBJ pThread)
{
	// ���̶߳����б��в���pThread��ָ�Ķ�������ҵ��ʹ����Ƴ�
	::EnterCriticalSection(&g_cs);
	PTHREAD_OBJ p = g_pThreadList;
	if (p == pThread)
	{
		g_pThreadList = p->pNext;

	}
	else
	{
		while (p != NULL && p->pNext != pThread)
		{
			p = p->pNext;
		}
		if (p != NULL)
		{
			// ��ʱ��p��pThread��ǰһ��������p->pNext == pThread��
			p->pNext = pThread->pNext;
		}
	}
	::LeaveCriticalSection(&g_cs);

	// �ͷ���Դ
	::CloseHandle(pThread->events[0]);
	::DeleteCriticalSection(&pThread->cs);
	//�߳���-1
	total_thread_num--;
	printf("��ǰ���߳���: %d \n", total_thread_num);
	::GlobalFree(pThread);
}



// ��һ���̵߳��׽����б��в���һ���׽���
BOOL InsertBufferObj(PTHREAD_OBJ pThread, PBUFFER_OBJ pBuffer)
{
	BOOL bRet = FALSE;
	::EnterCriticalSection(&pThread->cs);
	if (pThread->nBufferCount < WSA_MAXIMUM_WAIT_EVENTS - 1)
	{
		if (pThread->pBufferHeader == NULL)
		{
			pThread->pBufferHeader = pThread->pBufferTail = pBuffer;
		}
		else
		{
			pThread->pBufferTail->pNext = pBuffer;
			pThread->pBufferTail = pBuffer;
		}
		pThread->events[++pThread->nBufferCount] = pBuffer->ol.hEvent;
		bRet = TRUE;
	}
	::LeaveCriticalSection(&pThread->cs);

	// ����ɹ���˵���ɹ������˿ͻ�����������
	if (bRet)
	{
		::InterlockedIncrement(&g_nTatolConnections);
		::InterlockedIncrement(&g_nCurrentConnections);
	}
	return bRet;
}

// ��һ���׽��ֶ����Ÿ����е��̴߳���
void AssignToFreeThread(PBUFFER_OBJ pBuffer)
{
	pBuffer->pNext = NULL;

	::EnterCriticalSection(&g_cs);
	PTHREAD_OBJ pThread = g_pThreadList;
	// ��ͼ���뵽�ִ��߳�
	while (pThread != NULL)
	{
		if (InsertBufferObj(pThread, pBuffer))
			break;
		pThread = pThread->pNext;
	}

	// û�п����̣߳�Ϊ����׽��ִ����µ��߳�
	if (pThread == NULL)
	{
		pThread = GetThreadObj();
		InsertBufferObj(pThread, pBuffer);
		::CreateThread(NULL, 0, ServerThread, pThread, 0, NULL);
	}
	::LeaveCriticalSection(&g_cs);

	// ָʾ�߳��ؽ��������
	::WSASetEvent(pThread->events[0]);
}

BOOL PostAccept(PBUFFER_OBJ pBuffer)
{
	PSOCKET_OBJ pSocket = pBuffer->pSocket;
	if (pSocket->lpfnAcceptEx != NULL)
	{
		// ����I/O���ͣ������׽����ϵ��ص�I/O����
		pBuffer->nOperation = OP_ACCEPT;
		pSocket->nOutstandingOps++;

		// Ͷ�ݴ��ص�I/O  
		DWORD dwBytes;
		pBuffer->sAccept =
			::WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
		BOOL b = pSocket->lpfnAcceptEx(pSocket->s,
			pBuffer->sAccept,
			pBuffer->buff,
			BUFFER_SIZE - ((sizeof(sockaddr_in) + 16) * 2),
			sizeof(sockaddr_in) + 16,
			sizeof(sockaddr_in) + 16,
			&dwBytes,
			&pBuffer->ol);
		if (!b)
		{
			if (::WSAGetLastError() != WSA_IO_PENDING)
				return FALSE;
		}
		return TRUE;
	}
	return FALSE;
};

BOOL PostRecv(PBUFFER_OBJ pBuffer)
{
	// ����I/O���ͣ������׽����ϵ��ص�I/O����
	pBuffer->nOperation = OP_READ;
	pBuffer->pSocket->nOutstandingOps++;

	// Ͷ�ݴ��ص�I/O
	DWORD dwBytes;
	DWORD dwFlags = 0;
	WSABUF buf;
	buf.buf = pBuffer->buff;
	buf.len = pBuffer->nLen;
	if (::WSARecv(pBuffer->pSocket->s, &buf, 1, &dwBytes, &dwFlags, &pBuffer->ol, NULL) != NO_ERROR)
	{
		if (::WSAGetLastError() != WSA_IO_PENDING)
			return FALSE;
	}
	return TRUE;
}

BOOL PostSend(PBUFFER_OBJ pBuffer)
{
	// ����I/O���ͣ������׽����ϵ��ص�I/O����
	pBuffer->nOperation = OP_WRITE;
	pBuffer->pSocket->nOutstandingOps++;

	// Ͷ�ݴ��ص�I/O
	DWORD dwBytes;
	DWORD dwFlags = 0;
	WSABUF buf;
	buf.buf = pBuffer->buff;
	buf.len = pBuffer->nLen;
	if (::WSASend(pBuffer->pSocket->s,
		&buf, 1, &dwBytes, dwFlags, &pBuffer->ol, NULL) != NO_ERROR)
	{
		if (::WSAGetLastError() != WSA_IO_PENDING)
			return FALSE;
	}
	return TRUE;
}

BOOL HandleIO(PTHREAD_OBJ pThread, PBUFFER_OBJ pBuffer)
{
	PSOCKET_OBJ pSocket = pBuffer->pSocket; // ��BUFFER_OBJ��������ȡSOCKET_OBJ����ָ�룬Ϊ���Ƿ�������
	pSocket->nOutstandingOps--;

	// ��ȡ�ص��������
	DWORD dwTrans;
	DWORD dwFlags;
	BOOL bRet = ::WSAGetOverlappedResult(pSocket->s, &pBuffer->ol, &dwTrans, FALSE, &dwFlags);
	if (!bRet)
	{
		// �ڴ��׽������д���������ˣ��ر��׽��֣��Ƴ��˻���������
		// ���û�������׳���I/O�����ˣ��ͷŴ˻��������󣬷��򣬵ȴ����׽����ϵ�����I/OҲ���
		if (pSocket->s != INVALID_SOCKET)
		{
			::closesocket(pSocket->s);
			pSocket->s = INVALID_SOCKET;
		}

		if (pSocket->nOutstandingOps == 0)
			FreeSocketObj(pSocket);

		FreeBufferObj(pBuffer, pThread);
		return FALSE;
	}

	// û�д���������������ɵ�I/O
	switch (pBuffer->nOperation)
	{
	case OP_ACCEPT:	// ���յ�һ���µ����ӣ������յ��˶Է������ĵ�һ�����
	{
		time_t temp;
		temp = time(NULL);

		// Ϊ�¿ͻ�����һ��SOCKET_OBJ����
		PSOCKET_OBJ pClient = GetSocketObj(pBuffer->sAccept);
		// Ϊ�������ݴ���һ��BUFFER_OBJ���������������׽��ֳ�����߹ر�ʱ�ͷ�
		PBUFFER_OBJ pSend = GetBufferObj(pClient, BUFFER_SIZE);
		if (pSend == NULL)
		{
			printf(" Too much connections! \n");
			FreeSocketObj(pClient);
			return FALSE;
		}
		AssignToFreeThread(pSend);
		RebuildArray(pThread);
		
		pSend->id = add();
		printf("�û�%d���ӳɹ���\n", pSend->id);

		if (pBuffer->buff[0] == 'F')
		{
			char filepath[164] = "C:\\Users\\lhuang\\";
			strcat(filepath, pBuffer->buff + 1);
			pSend->fileName = pBuffer->buff + 1;
			filepath[32 + dwTrans] = '\0';
			printf("�û�%d�����ļ�:%s \n", pSend->id, filepath);
			pSend->fd = fopen(filepath, "rb");
			if (pSend->fd != NULL)
			{
				strcpy(pSend->buff, "FOUND");
				pSend->nLen = strlen(pSend->buff);
				time_t tem = time(NULL);
				printf("�ļ���ʼ����,ϵͳʱ�䣺%s", ctime(&tem));
			}
			else
			{
				strcpy(pSend->buff, "ERROR");
				pSend->nLen = strlen(pSend->buff);
			}
		}
		else
		{
			char buff[1024] = "";
			strcat(buff, pBuffer->buff + 1);
			printf("�û�%d˵��\n%s\n", pSend->id, buff);
			strcpy(pSend->buff, "OK");
			pSend->nLen = strlen(pSend->buff);
		}
		
		// Ͷ�ݴ˷���I/O�������ݻ��Ը��ͻ���
		if (!PostSend(pSend))
		{
			// ��һ����Ļ����ͷ�������������������
			FreeSocketObj(pSocket);
			FreeBufferObj(pSend, pThread);
			return FALSE;
		}
		// ����Ͷ�ݽ���I/O
		PostAccept(pBuffer);
	}
	break;
	case OP_READ:	// �����������
	{
		if (dwTrans > 0)
		{
			// ����һ�����������Է������ݡ������ʹ��ԭ���Ļ�����

			PBUFFER_OBJ pSend = pBuffer;
			if (pBuffer->buff[0] == 'F')
			{
				char filepath[164] = "C:\\Users\\lhuang\\";
				strcat(filepath, pBuffer->buff + 1);
				pSend->fileName = pBuffer->buff + 1;
				filepath[32 + dwTrans] = '\0';
				printf("�û�%d�����ļ�:%s \n", pSend->id, filepath);
				pSend->fd = fopen(filepath, "rb");
				if (pSend->fd != NULL)
				{
					strcpy(pSend->buff, "FOUND");
					pSend->nLen = strlen(pSend->buff);
					time_t tem = time(NULL);
					printf("�ļ���ʼ����,ϵͳʱ�䣺%s", ctime(&tem));
				}
				else
				{
					strcpy(pSend->buff, "ERROR");
					pSend->nLen = strlen(pSend->buff);
				}
			}
			else
			{
				char buff[1024] = "";
				strcat(buff, pBuffer->buff + 1);
				printf("�û�%d˵��\n%s\n", pSend->id, buff);
				strcpy(pSend->buff, "OK");
				pSend->nLen = strlen(pSend->buff);
			}
			// Ͷ�ݷ���I/O�������ݻ��͸��ͻ���
			PostSend(pSend);
		}
		else	// �׽��ֹر�
		{

			// �����ȹر��׽��֣��Ա��ڴ��׽�����Ͷ�ݵ�����I/OҲ����
			if (pSocket->s != INVALID_SOCKET)
			{
				::closesocket(pSocket->s);
				pSocket->s = INVALID_SOCKET;
				//printf("�û�%d�Ѿ��Ͽ����ӣ�", pBuffer->id);
			}
			userlist.erase(pBuffer->id);
			if (pSocket->nOutstandingOps == 0)
				FreeSocketObj(pSocket);

			FreeBufferObj(pBuffer, pThread);
			return FALSE;
		}
	}
	break;
	case OP_WRITE:		// �����������
	{
		if (dwTrans > 0)
		{
			// ����ʹ�����������Ͷ�ݽ������ݵ�����

			if (pBuffer->fd != NULL)
			{
				pBuffer->seq++;
				memset(pBuffer->buff, 0, BUFFER_SIZE);
				int read_location = fread((&pBuffer->buff[7]), 1, BUFFER_SIZE - 7, pBuffer->fd);
				if (feof(pBuffer->fd))
				{
					time_t temp = time(NULL);
					printf("�ļ��������,ϵͳʱ�䣺%s", ctime(&temp));
					pBuffer->buff[0] = 0;
					pBuffer->nLen = read_location + 7;
					memcpy(&(pBuffer->buff[1]), &(pBuffer->nLen), 2);
					memcpy(&(pBuffer->buff[3]), &(pBuffer->seq), 4);
					fclose(pBuffer->fd);
					pBuffer->fd = NULL;
					pBuffer->seq = 0;
				}
				else
				{
					pBuffer->buff[0] = 1;
					pBuffer->nLen = BUFFER_SIZE;
					memcpy(&(pBuffer->buff[1]), &(pBuffer->nLen), 2);
					memcpy(&(pBuffer->buff[3]), &(pBuffer->seq), 4);
				}
				PostSend(pBuffer);
			}
			else
			{
				pBuffer->nLen = BUFFER_SIZE;
				memset(pBuffer->buff, 0, BUFFER_SIZE);
				PostRecv(pBuffer);
			}
		}
		else	// �׽��ֹر�
		{
			// ͬ����Ҫ�ȹر��׽���
			if (pSocket->s != INVALID_SOCKET)
			{
				::closesocket(pSocket->s);
				pSocket->s = INVALID_SOCKET;
			}
			userlist.erase(pBuffer->id);
			if (pSocket->nOutstandingOps == 0)
				FreeSocketObj(pSocket);

			FreeBufferObj(pBuffer, pThread);
			return FALSE;
		}
	}
	break;
	}
	return TRUE;
}

DWORD WINAPI ServerThread(LPVOID lpParam)
{
	// ȡ�ñ��̶߳����ָ��
	PTHREAD_OBJ pThread = (PTHREAD_OBJ)lpParam;
	while (TRUE)
	{
		//	�ȴ������¼�
		//sockaddr_in sin;
		int nIndex = ::WSAWaitForMultipleEvents(
			pThread->nBufferCount + 1, pThread->events, FALSE, WSA_INFINITE, FALSE);
		nIndex = nIndex - WSA_WAIT_EVENT_0;
		// �鿴���ŵ��¼�����
		for (int i = nIndex; i < pThread->nBufferCount + 1; i++)
		{
			nIndex = ::WSAWaitForMultipleEvents(1, &pThread->events[i], TRUE, 1000, FALSE);
			if (nIndex == WSA_WAIT_FAILED || nIndex == WSA_WAIT_TIMEOUT)
			{
				continue;
			}
			else
			{
				if (i == 0)				// events[0]���ţ��ؽ�����
				{
					RebuildArray(pThread);
					// ���û�пͻ�I/OҪ�����ˣ����߳��˳�
					if (pThread->nBufferCount == 0)
					{
						FreeThreadObj(pThread);
						return 0;
					}
					::WSAResetEvent(pThread->events[0]);
				}
				else					// ���������¼�
				{
					// ���Ҷ�Ӧ���׽��ֶ���ָ�룬����HandleIO���������¼�
					PBUFFER_OBJ pBuffer = FindBufferObj(pThread->events[i], pThread);
					if (pBuffer != NULL)
					{
						if (!HandleIO(pThread, pBuffer))
							RebuildArray(pThread);
						else {

						}
					}
				}
			}
		}
	}
	return 0;
}