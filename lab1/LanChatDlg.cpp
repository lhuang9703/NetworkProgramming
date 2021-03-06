
// LanChatDlg.cpp: 实现文件
//

#include "stdafx.h"
#include "LanChat.h"
#include "LanChatDlg.h"
#include "afxdialogex.h"
#include "InitSock.h"


#ifdef _DEBUG
#define new DEBUG_NEW
#endif


// 用于应用程序“关于”菜单项的 CAboutDlg 对话框

class CAboutDlg : public CDialogEx
{
public:
	CAboutDlg();

// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_ABOUTBOX };
#endif

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

// 实现
protected:
	DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialogEx(IDD_ABOUTBOX)
{
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialogEx)
END_MESSAGE_MAP()


// CLanChatDlg 对话框


CInitSock initSock;

CLanChatDlg::CLanChatDlg(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_LANCHAT_DIALOG, pParent)
	, ipRecevier(0)
	, lbChatContent(_T(""))
	, editSendContent(_T(""))
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);

}

void CLanChatDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_IPAddress(pDX, IDC_IPADDRESS1, ipRecevier);
	DDX_Text(pDX, IDC_EDIT1 , lbChatContent);
	DDX_Text(pDX, IDC_EDIT2, editSendContent);
}

BEGIN_MESSAGE_MAP(CLanChatDlg, CDialogEx)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_NOTIFY(IPN_FIELDCHANGED, IDC_IPADDRESS1, &CLanChatDlg::OnIpnFieldchangedIpaddress1)
	ON_EN_CHANGE(IDC_EDIT1, &CLanChatDlg::OnEnChangeEdit1)
	ON_BN_CLICKED(IDC_BUTTON1, &CLanChatDlg::OnBnClickedButton1)
	ON_EN_CHANGE(IDC_EDIT2, &CLanChatDlg::OnEnChangeEdit2)
	ON_MESSAGE(WM_RECVDATA, &CLanChatDlg::UpdateEdit1)
END_MESSAGE_MAP()


// CLanChatDlg 消息处理程序

BOOL CLanChatDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// 将“关于...”菜单项添加到系统菜单中。

	// IDM_ABOUTBOX 必须在系统命令范围内。
	ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
	ASSERT(IDM_ABOUTBOX < 0xF000);

	CMenu* pSysMenu = GetSystemMenu(FALSE);
	if (pSysMenu != nullptr)
	{
		BOOL bNameValid;
		CString strAboutMenu;
		bNameValid = strAboutMenu.LoadString(IDS_ABOUTBOX);
		ASSERT(bNameValid);
		if (!strAboutMenu.IsEmpty())
		{
			pSysMenu->AppendMenu(MF_SEPARATOR);
			pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
		}
	}

	// 设置此对话框的图标。  当应用程序主窗口不是对话框时，框架将自动
	//  执行此操作
	SetIcon(m_hIcon, TRUE);			// 设置大图标
	SetIcon(m_hIcon, FALSE);		// 设置小图标

	// TODO: 在此添加额外的初始化代码
	CreateSocket();

	RECVPARAM *pRecvParam = new RECVPARAM;
	pRecvParam->sockparam = m_socket;
	pRecvParam->hwnd = m_hWnd;
	HANDLE hReceiveThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)RecvProc, (LPVOID)pRecvParam, 0, NULL);
	CloseHandle(hReceiveThread);
	//m_pThread = AfxBeginThread((AFX_THREADPROC)RecvProc, this);
	return TRUE;  // 除非将焦点设置到控件，否则返回 TRUE
}

void CLanChatDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
	if ((nID & 0xFFF0) == IDM_ABOUTBOX)
	{
		CAboutDlg dlgAbout;
		dlgAbout.DoModal();
	}
	else
	{
		CDialogEx::OnSysCommand(nID, lParam);
	}
}

// 如果向对话框添加最小化按钮，则需要下面的代码
//  来绘制该图标。  对于使用文档/视图模型的 MFC 应用程序，
//  这将由框架自动完成。

void CLanChatDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // 用于绘制的设备上下文

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// 使图标在工作区矩形中居中
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// 绘制图标
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialogEx::OnPaint();
	}
}

//当用户拖动最小化窗口时系统调用此函数取得光标
//显示。
HCURSOR CLanChatDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}

bool CLanChatDlg::CreateSocket()
{
	m_socket = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (m_socket == INVALID_SOCKET)
	{
		MessageBox(_T("Fail to create socket:m_socket!\n"));
		return FALSE;
	}

	sockaddr_in sin;
	sin.sin_family = AF_INET;
	sin.sin_port = htons(2333);
	sin.sin_addr.S_un.S_addr = INADDR_ANY;

	if (::bind(m_socket, (LPSOCKADDR)&sin, sizeof(sin)) == SOCKET_ERROR)
	{
		MessageBox(_T("Fail to bind socket:m_socket!\n"));
		return FALSE;
	}
	return TRUE;
}


DWORD WINAPI CLanChatDlg::RecvProc(LPVOID lpVoid)
{
	// 从传进来的参数中获得套接字和线程id
	SOCKET sock = ((RECVPARAM*)lpVoid)->sockparam;
	HWND hwnd = ((RECVPARAM*)lpVoid)->hwnd;
	delete lpVoid;

	// 定义接收消息的buffer变量recvBuff、历史消息的buffer变量allBuff
	char recvBuff[1024];
	char allBuff[2048];
	sockaddr_in addr;
	int nLen = sizeof(addr);

	// 循环接收m_socket在2333端口收到的消息，并通过发送自定义消息WM_RECVDATA更新聊天框内容
	while (TRUE)
	{
		int nRecv = ::recvfrom(sock, recvBuff, 1024, 0, (sockaddr*)&addr, &nLen);
		if (nRecv > 0)
		{
			sprintf(allBuff, "%s:\r\n%s", inet_ntoa(addr.sin_addr), recvBuff);
			::PostMessage(hwnd, WM_RECVDATA, 0, (LPARAM)allBuff);		
		}
		else if (nRecv == SOCKET_ERROR)
			break;
	}
	return 0;
}


void CLanChatDlg::OnIpnFieldchangedIpaddress1(NMHDR *pNMHDR, LRESULT *pResult)
{
	LPNMIPADDRESS pIPAddr = reinterpret_cast<LPNMIPADDRESS>(pNMHDR);
	// TODO: 在此添加控件通知处理程序代码
	*pResult = 0;
}


void CLanChatDlg::OnEnChangeEdit1()
{
	// TODO:  如果该控件是 RICHEDIT 控件，它将不
	// 发送此通知，除非重写 CDialogEx::OnInitDialog()
	// 函数并调用 CRichEditCtrl().SetEventMask()，
	// 同时将 ENM_CHANGE 标志“或”运算到掩码中。

	// TODO:  在此添加控件通知处理程序代码
}


void CLanChatDlg::OnBnClickedButton1()
{
	// TODO: 在此添加控件通知处理程序代码
	// 从IP地址框中获得IP地址
	DWORD IP;
	((CIPAddressCtrl*)GetDlgItem(IDC_IPADDRESS1))->GetAddress(IP);

	// 定义消息发往的套接字，主要包含对方IP地址和端口号
	sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(4567);
	addr.sin_addr.S_un.S_addr = htonl(IP);

	// 从输入框中获取用户输入，并从CString类型转为char*l类型
	CString strSend;
	GetDlgItemText(IDC_EDIT2, strSend);
	char str[2048];
	memset(str, 0, sizeof(str));
	strcpy(str, (LPSTR)strSend.GetBuffer(strSend.GetLength()));

	// 调用sendto函数通过套接字m_socket发送消息，返回值n为发送的字节数（等于-1代表发送失败）
	int lenSend = strSend.GetLength() + 1;
	int n = ::sendto(m_socket, str, lenSend, 0, (sockaddr*)&addr, sizeof(addr));
	if (n < 0)
	{
		MessageBox(_T("Fail to send data!\n"));
	}

	// 清空输入框
	CString strAfter((char*)"");
	SetDlgItemText(IDC_EDIT2, strAfter);

	// 将发送的消息在自己的聊天框中输出来
	CString strEdit1;
	GetDlgItemText(IDC_EDIT1, strEdit1);
	char s2[10] = "\r\n\r\n";
	CString s3(s2);
	strEdit1 = s3 + strEdit1;
	char s[100] = "我:\r\n";
	CString s1(s);
	strSend = s1 + strSend;
	strSend += strEdit1;
	SetDlgItemText(IDC_EDIT1, strSend);

	return;
}


void CLanChatDlg::OnEnChangeEdit2()
{
	// TODO:  如果该控件是 RICHEDIT 控件，它将不
	// 发送此通知，除非重写 CDialogEx::OnInitDialog()
	// 函数并调用 CRichEditCtrl().SetEventMask()，
	// 同时将 ENM_CHANGE 标志“或”运算到掩码中。

	// TODO:  在此添加控件通知处理程序代码
	//CString str((char*)lParam);
}


LRESULT CLanChatDlg::UpdateEdit1(WPARAM wParam, LPARAM lParam)
{
	CString str((char*)lParam);
	CString strAll;

	GetDlgItemText(IDC_EDIT1, strAll);
	str += "\r\n\r\n";
	strAll = str + strAll;
	SetDlgItemText(IDC_EDIT1, strAll);

	return 0;
}
