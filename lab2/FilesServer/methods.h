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
	printf("当前用户列表为：\n");
	for (set<int>::iterator it = userlist.begin(); it != userlist.end(); it++) {
		printf("%d ", *it);
	}
	printf("\n");
	LeaveCriticalSection(&cal.n);
	return cal.client_num;
}
typedef struct _SOCKET_OBJ
{
	SOCKET s;						// 套接字句柄
	int nOutstandingOps;			// 记录此套接字上的重叠I/O数量

	LPFN_ACCEPTEX lpfnAcceptEx;		// 扩展函数AcceptEx的指针（仅对监听套接字而言）
} SOCKET_OBJ, *PSOCKET_OBJ;

typedef struct _BUFFER_OBJ
{
	OVERLAPPED ol;			// 重叠结构
	char *buff;				// send/recv/AcceptEx所使用的缓冲区
	int nLen;				// buff的长度
	PSOCKET_OBJ pSocket;	// 此I/O所属的套接字对象

	int nOperation;			// 提交的操作类型
#define OP_ACCEPT	1
#define OP_READ		2
#define OP_WRITE	3

	SOCKET sAccept;			// 用来保存AcceptEx接受的客户套接字（仅对监听套接字而言）
	_BUFFER_OBJ *pNext;
	FILE* fd;
	char *fileName;
	int id;
	int seq;
} BUFFER_OBJ, *PBUFFER_OBJ;

// 套接字对象
/*typedef struct _SOCKET_OBJ
{
SOCKET s;					// 套接字句柄
HANDLE event;				// 与此套接字相关联的事件对象句柄
sockaddr_in addrRemote;		// 客户端地址信息

_SOCKET_OBJ *pNext;			// 指向下一个SOCKET_OBJ对象，为的是连成一个表
} SOCKET_OBJ, *PSOCKET_OBJ;
*/
// 线程对象
typedef struct _THREAD_OBJ
{
	HANDLE events[WSA_MAXIMUM_WAIT_EVENTS];	// 记录当前线程要等待的事件对象的句柄
	int nBufferCount;						// 记录当前线程处理的套接字的数量 <=  WSA_MAXIMUM_WAIT_EVENTS

	PBUFFER_OBJ pBufferHeader;				// 当前线程处理的套接字对象列表，pSockHeader指向表头
	PBUFFER_OBJ pBufferTail;					// pSockTail指向表尾

	CRITICAL_SECTION cs;					// 关键代码段变量，为的是同步对本结构的访问
	_THREAD_OBJ *pNext;	 // 指向下一个THREAD_OBJ对象，为的是连成一个表
	static int total_thread_num;

} THREAD_OBJ, *PTHREAD_OBJ;

// 线程列表
PTHREAD_OBJ g_pThreadList;		// 指向线程对象列表表头
CRITICAL_SECTION g_cs;			// 同步对此全局变量的访问


								// 状态信息
LONG total_thread_num;		// 总共连接数量
LONG g_nTatolConnections;
LONG g_nCurrentConnections;		// 当前连接数量
LPFN_GETACCEPTEXSOCKADDRS m_lpfnGetAcceptExSockaddrs = NULL;



// 申请套接字对象和释放套接字对象的函数
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
	// 从列表中移除BUFFER_OBJ对象
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
	// 释放它占用的内存空间
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

// 申请一个线程对象，初始化它的成员，并将它添加到线程对象列表中
PTHREAD_OBJ GetThreadObj()
{
	PTHREAD_OBJ pThread = (PTHREAD_OBJ)::GlobalAlloc(GPTR, sizeof(THREAD_OBJ));
	if (pThread != NULL)
	{
		::InitializeCriticalSection(&pThread->cs);
		// 创建一个事件对象，用于指示该线程的句柄数组需要重组
		pThread->events[0] = ::WSACreateEvent();

		// 将新申请的线程对象添加到列表中
		::EnterCriticalSection(&g_cs);
		pThread->pNext = g_pThreadList;
		g_pThreadList = pThread;
		//pThread->total_thread_num++;
		total_thread_num++;
		::LeaveCriticalSection(&g_cs);
		printf("启动新的线程!线程数: %d\n", total_thread_num);
	}
	return pThread;
}

// 释放一个线程对象，并将它从线程对象列表中移除
void FreeThreadObj(PTHREAD_OBJ pThread)
{
	// 在线程对象列表中查找pThread所指的对象，如果找到就从中移除
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
			// 此时，p是pThread的前一个，即“p->pNext == pThread”
			p->pNext = pThread->pNext;
		}
	}
	::LeaveCriticalSection(&g_cs);

	// 释放资源
	::CloseHandle(pThread->events[0]);
	::DeleteCriticalSection(&pThread->cs);
	//线程数-1
	total_thread_num--;
	printf("当前的线程数: %d \n", total_thread_num);
	::GlobalFree(pThread);
}



// 向一个线程的套接字列表中插入一个套接字
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

	// 插入成功，说明成功处理了客户的连接请求
	if (bRet)
	{
		::InterlockedIncrement(&g_nTatolConnections);
		::InterlockedIncrement(&g_nCurrentConnections);
	}
	return bRet;
}

// 将一个套接字对象安排给空闲的线程处理
void AssignToFreeThread(PBUFFER_OBJ pBuffer)
{
	pBuffer->pNext = NULL;

	::EnterCriticalSection(&g_cs);
	PTHREAD_OBJ pThread = g_pThreadList;
	// 试图插入到现存线程
	while (pThread != NULL)
	{
		if (InsertBufferObj(pThread, pBuffer))
			break;
		pThread = pThread->pNext;
	}

	// 没有空闲线程，为这个套接字创建新的线程
	if (pThread == NULL)
	{
		pThread = GetThreadObj();
		InsertBufferObj(pThread, pBuffer);
		::CreateThread(NULL, 0, ServerThread, pThread, 0, NULL);
	}
	::LeaveCriticalSection(&g_cs);

	// 指示线程重建句柄数组
	::WSASetEvent(pThread->events[0]);
}

BOOL PostAccept(PBUFFER_OBJ pBuffer)
{
	PSOCKET_OBJ pSocket = pBuffer->pSocket;
	if (pSocket->lpfnAcceptEx != NULL)
	{
		// 设置I/O类型，增加套接字上的重叠I/O计数
		pBuffer->nOperation = OP_ACCEPT;
		pSocket->nOutstandingOps++;

		// 投递此重叠I/O  
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
	// 设置I/O类型，增加套接字上的重叠I/O计数
	pBuffer->nOperation = OP_READ;
	pBuffer->pSocket->nOutstandingOps++;

	// 投递此重叠I/O
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
	// 设置I/O类型，增加套接字上的重叠I/O计数
	pBuffer->nOperation = OP_WRITE;
	pBuffer->pSocket->nOutstandingOps++;

	// 投递此重叠I/O
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
	PSOCKET_OBJ pSocket = pBuffer->pSocket; // 从BUFFER_OBJ对象中提取SOCKET_OBJ对象指针，为的是方便引用
	pSocket->nOutstandingOps--;

	// 获取重叠操作结果
	DWORD dwTrans;
	DWORD dwFlags;
	BOOL bRet = ::WSAGetOverlappedResult(pSocket->s, &pBuffer->ol, &dwTrans, FALSE, &dwFlags);
	if (!bRet)
	{
		// 在此套接字上有错误发生，因此，关闭套接字，移除此缓冲区对象。
		// 如果没有其它抛出的I/O请求了，释放此缓冲区对象，否则，等待此套接字上的其它I/O也完成
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

	// 没有错误发生，处理已完成的I/O
	switch (pBuffer->nOperation)
	{
	case OP_ACCEPT:	// 接收到一个新的连接，并接收到了对方发来的第一个封包
	{
		time_t temp;
		temp = time(NULL);

		// 为新客户创建一个SOCKET_OBJ对象
		PSOCKET_OBJ pClient = GetSocketObj(pBuffer->sAccept);
		// 为发送数据创建一个BUFFER_OBJ对象，这个对象会在套接字出错或者关闭时释放
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
		printf("用户%d连接成功！\n", pSend->id);

		if (pBuffer->buff[0] == 'F')
		{
			char filepath[164] = "C:\\Users\\lhuang\\";
			strcat(filepath, pBuffer->buff + 1);
			pSend->fileName = pBuffer->buff + 1;
			filepath[32 + dwTrans] = '\0';
			printf("用户%d请求文件:%s \n", pSend->id, filepath);
			pSend->fd = fopen(filepath, "rb");
			if (pSend->fd != NULL)
			{
				strcpy(pSend->buff, "FOUND");
				pSend->nLen = strlen(pSend->buff);
				time_t tem = time(NULL);
				printf("文件开始传输,系统时间：%s", ctime(&tem));
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
			printf("用户%d说：\n%s\n", pSend->id, buff);
			strcpy(pSend->buff, "OK");
			pSend->nLen = strlen(pSend->buff);
		}
		
		// 投递此发送I/O（将数据回显给客户）
		if (!PostSend(pSend))
		{
			// 万一出错的话，释放上面刚申请的两个对象
			FreeSocketObj(pSocket);
			FreeBufferObj(pSend, pThread);
			return FALSE;
		}
		// 继续投递接受I/O
		PostAccept(pBuffer);
	}
	break;
	case OP_READ:	// 接收数据完成
	{
		if (dwTrans > 0)
		{
			// 创建一个缓冲区，以发送数据。这里就使用原来的缓冲区

			PBUFFER_OBJ pSend = pBuffer;
			if (pBuffer->buff[0] == 'F')
			{
				char filepath[164] = "C:\\Users\\lhuang\\";
				strcat(filepath, pBuffer->buff + 1);
				pSend->fileName = pBuffer->buff + 1;
				filepath[32 + dwTrans] = '\0';
				printf("用户%d请求文件:%s \n", pSend->id, filepath);
				pSend->fd = fopen(filepath, "rb");
				if (pSend->fd != NULL)
				{
					strcpy(pSend->buff, "FOUND");
					pSend->nLen = strlen(pSend->buff);
					time_t tem = time(NULL);
					printf("文件开始传输,系统时间：%s", ctime(&tem));
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
				printf("用户%d说：\n%s\n", pSend->id, buff);
				strcpy(pSend->buff, "OK");
				pSend->nLen = strlen(pSend->buff);
			}
			// 投递发送I/O（将数据回送给客户）
			PostSend(pSend);
		}
		else	// 套接字关闭
		{

			// 必须先关闭套接字，以便在此套接字上投递的其它I/O也返回
			if (pSocket->s != INVALID_SOCKET)
			{
				::closesocket(pSocket->s);
				pSocket->s = INVALID_SOCKET;
				//printf("用户%d已经断开连接！", pBuffer->id);
			}
			userlist.erase(pBuffer->id);
			if (pSocket->nOutstandingOps == 0)
				FreeSocketObj(pSocket);

			FreeBufferObj(pBuffer, pThread);
			return FALSE;
		}
	}
	break;
	case OP_WRITE:		// 发送数据完成
	{
		if (dwTrans > 0)
		{
			// 继续使用这个缓冲区投递接收数据的请求

			if (pBuffer->fd != NULL)
			{
				pBuffer->seq++;
				memset(pBuffer->buff, 0, BUFFER_SIZE);
				int read_location = fread((&pBuffer->buff[7]), 1, BUFFER_SIZE - 7, pBuffer->fd);
				if (feof(pBuffer->fd))
				{
					time_t temp = time(NULL);
					printf("文件传输完毕,系统时间：%s", ctime(&temp));
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
		else	// 套接字关闭
		{
			// 同样，要先关闭套接字
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
	// 取得本线程对象的指针
	PTHREAD_OBJ pThread = (PTHREAD_OBJ)lpParam;
	while (TRUE)
	{
		//	等待网络事件
		//sockaddr_in sin;
		int nIndex = ::WSAWaitForMultipleEvents(
			pThread->nBufferCount + 1, pThread->events, FALSE, WSA_INFINITE, FALSE);
		nIndex = nIndex - WSA_WAIT_EVENT_0;
		// 查看受信的事件对象
		for (int i = nIndex; i < pThread->nBufferCount + 1; i++)
		{
			nIndex = ::WSAWaitForMultipleEvents(1, &pThread->events[i], TRUE, 1000, FALSE);
			if (nIndex == WSA_WAIT_FAILED || nIndex == WSA_WAIT_TIMEOUT)
			{
				continue;
			}
			else
			{
				if (i == 0)				// events[0]受信，重建数组
				{
					RebuildArray(pThread);
					// 如果没有客户I/O要处理了，则本线程退出
					if (pThread->nBufferCount == 0)
					{
						FreeThreadObj(pThread);
						return 0;
					}
					::WSAResetEvent(pThread->events[0]);
				}
				else					// 处理网络事件
				{
					// 查找对应的套接字对象指针，调用HandleIO处理网络事件
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