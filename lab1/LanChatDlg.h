
// LanChatDlg.h: 头文件
//

#pragma once


// CLanChatDlg 对话框
class CLanChatDlg : public CDialogEx
{
// 构造
public:
	CLanChatDlg(CWnd* pParent = nullptr);	// 标准构造函数

// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_LANCHAT_DIALOG };
#endif

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV 支持


// 实现
protected:
	HICON m_hIcon;

	// 生成的消息映射函数
	virtual BOOL OnInitDialog();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	bool CreateSocket();
	CWinThread *m_pThread;
	static DWORD WINAPI RecvProc(LPVOID lpVoid);
	SOCKET m_socket;
	DECLARE_MESSAGE_MAP()
public:
	afx_msg void OnIpnFieldchangedIpaddress1(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void OnEnChangeEdit1();
	afx_msg void OnBnClickedButton1();
	DWORD ipRecevier;
	CString lbChatContent;
	CString editSendContent;
	afx_msg void OnEnChangeEdit2();
	afx_msg LRESULT UpdateEdit1(WPARAM wParam, LPARAM lParam);
};

struct RECVPARAM
{
	SOCKET sockparam;
	HWND hwnd;
};

#define WM_RECVDATA (WM_USER+100)