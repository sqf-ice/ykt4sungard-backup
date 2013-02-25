#include "stdafx.h"
#include "ksnl.h"
#include <exception>
#include ".\ksworkflow.h"
#include "KSInputDialog.h"
#include "KSNLDlg.h"
#include "KSCardManager.h"
#include "DRTPHelper.h"
#include "ksutil.h"

#define KS_BANK_TX_CODE			900077
#define TRADE_NODEBT			1			// ����δ����
#define TRADE_DEBT				2			// ����
#define KS_TRANS				260001		// ˮ��ת��, �ֻ���ֵ
#define KS_WRITE_CARD_FAILED	260002		// д��ʧ��

//#define KS_GETREFNO			850000		// ��̨ȡ���ײο���
#define KS_WATERACC				850005		// ˮ��ת�˺�̨����
#define KS_WATERTRANS			850006		// ���졢����ˮ��Ǯ����ת

using namespace std;
//////////////////////////////////////////////////////////////////////////
// global function
CString ParseDateString(const CString & ordDate)
{
	if(ordDate.GetLength() < 8)
	{
		return "";
	}
	CString result;
	result.Format("%s-%s-%s",ordDate.Mid(0,4),ordDate.Mid(4,2),ordDate.Mid(6,2));
	return result;
}
CString ParseTimeString(const CString & ordTime)
{
	if(ordTime.GetLength() < 6)
	{
		return "";
	}
	CString result;
	result.Format("%s:%s:%s",ordTime.Mid(0,2),ordTime.Mid(2,2),ordTime.Mid(4,2));
	return result;
}

// Write water packet cardtype to log file!
int WriteLogFile(char BeginCardType, char EndCardType, double BeginBalance, double EndBalance)
{
	CStdioFile sfLogFile;
	CString strAppPath = "";
	CString strWriteFile = "";
	char cAppPath[512] = "";
	int nFilePathLength = 0;
	BOOL bResult;
	static int s_nCount = 0;
	nFilePathLength = GetFullPathName("LogFile.txt", 512, cAppPath, NULL);
	if ('\0' == cAppPath[0])
	{
		return -1;				// �ļ���·�����ȹ���	 
	}
	strAppPath.Format("%s", cAppPath);
	bResult = sfLogFile.Open(strAppPath, CFile::modeWrite | CFile::modeCreate 
			 | CFile::modeNoTruncate | CFile::typeText);
	if (!bResult)
	{
		return -2;				// �ļ����ܴ�
	}
	CTime CurrentTime = CTime::GetCurrentTime();
	strWriteFile.Format("[%04d-%02d-%02d %02d:%02d:%02d] д������:%d -- ��������:%d -- д�����:%4.4f -- �������:%4.4f\n", 
						CurrentTime.GetYear(),CurrentTime.GetMonth(), CurrentTime.GetDay(), CurrentTime.GetHour(), 
						CurrentTime.GetMinute(), CurrentTime.GetSecond(), BeginCardType, EndCardType, BeginBalance, EndBalance);
	sfLogFile.SeekToEnd();
	sfLogFile.WriteString(strWriteFile);
	sfLogFile.Close();
	return 0;
}
//////////////////////////////////////////////////////////////////////////
IMPLEMENT_DYNCREATE (CKSWorkflowThread,CWinThread)
CKSWorkflowThread::CKSWorkflowThread()
:CWinThread(),m_workflow(NULL)
{
	this->m_bAutoDelete = TRUE;
}
CKSWorkflowThread::~CKSWorkflowThread()
{
	
}
int CKSWorkflowThread::Run()
{
	m_workResult = m_workflow->DoWork();
	m_workflow->m_isTerminated = TRUE;
	return CWinThread::Run();
}
BOOL CKSWorkflowThread::InitInstance()
{
	return TRUE;
}
int CKSWorkflowThread::ExitInstance()
{
	return 0;
}
BEGIN_MESSAGE_MAP(CKSWorkflowThread, CWinThread)
	//{{AFX_MSG_MAP(CNewCardThread)
	// NOTE - the ClassWizard will add and remove mapping macros here.
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

//////////////////////////////////////////////////////////////////////////
// CKSWorkflow
CKSWorkflow::CKSWorkflow(CDialog * dlg):
m_key(""),m_funcText(""),m_posX(0),m_posY(0),m_payCode(0),m_isTerminated(FALSE)
,m_stopKey(FALSE),m_lastKeyCode(-1),m_workThread(NULL),m_mainDlg(dlg),m_mutex(NULL)
{
	/*
	int s = rand();
	CString mutexStr;
	mutexStr.Format("ks-workflow-mutex-%s-%.06d",(LPCTSTR)m_key,s);
	m_mutex = CreateEvent(NULL,TRUE,TRUE,mutexStr);
	if(!m_mutex || GetLastError() == ERROR_ALREADY_EXISTS)
	{
		throw exception("����������ʧ��");
	}
	*/
}

CKSWorkflow::~CKSWorkflow(void)
{
	if(m_mutex)
	{
		CloseHandle(m_mutex);
		m_mutex = FALSE;
	}
	if(m_workThread)
	{
		m_workThread->ExitInstance();
		delete m_workThread;
	}
}

int CKSWorkflow::OnFormKeyDown(UINT nChar, UINT nFlags)
{
	SetKeyCode(nChar);
	return 1;
}

int CKSWorkflow::Work()
{
	m_isTerminated = FALSE;
	int ret = 0;
	/*
	//m_workThread = new CKSWorkflowThread();
	m_workThread = (CKSWorkflowThread*)AfxBeginThread(RUNTIME_CLASS(CKSWorkflowThread)
		,THREAD_PRIORITY_NORMAL,0,CREATE_SUSPENDED);
	m_workThread->m_workflow = this;
	m_workThread->ResumeThread();
	*/
	ret = DoWork();
	m_isTerminated = TRUE;
	return ret;
}
DWORD CKSWorkflow::GetLastKeyCode()
{
	if(WaitForSingleObject(m_mutex,1000) == WAIT_TIMEOUT)
		return -1;
	UINT flag;
	flag = PM_REMOVE |PM_NOYIELD;
	MSG msg;
	m_lastKeyCode = -1;
	
	if(::PeekMessage(&msg,AfxGetMainWnd()->GetSafeHwnd(),WM_KEYFIRST,WM_KEYLAST,flag))
	{
		if(WM_KEYDOWN == msg.message || WM_SYSKEYDOWN == msg.message)
		{
			if(!m_stopKey)
			{
				m_lastKeyCode = msg.wParam;
			}
		}
	}
	return m_lastKeyCode;
}

int CKSWorkflow::D2I(double value)
{
	double smaller = floor(value);
	double bigger = ceil(value);
	return (int)(((bigger - value) > (value - smaller)) ? smaller : bigger);
} 

//////////////////////////////////////////////////////////////////////////
// ������ı�д
//////////////////////////////////////////////////////////////////////////
CKSSubsidyWorkFlow::CKSSubsidyWorkFlow(CDialog * dlg):
	CKSWorkflow(dlg)
{
	// do nothing
	m_key = "subsidy";
}
CKSSubsidyWorkFlow::~CKSSubsidyWorkFlow()
{
	// 
}
int CKSSubsidyWorkFlow::DoWork()
{
	
	CString passwd;
	CString tipMsg("");
	CDRTPHelper * drtp = NULL;
	CDRTPHelper * response = NULL;
	KS_CARD_INFO cardinfo;
	int ret;
	int iCount = 0;
	int hasMore = 0;
	CKSNLDlg* dlg = (CKSNLDlg*)GetMainWnd();  // Get a active main window of the application
	memset(&cardinfo,0,sizeof cardinfo);
	SetStopKey(false);

	
	dlg->ShowTipMessage("���У԰��...",0);
	CKSCardManager card(dlg->GetConfig(),
		(LPSTR)(LPCTSTR)dlg->GetConfig()->m_workKey,this);
	
	if(card.OpenCOM()!=0)
	{
		dlg->ShowTipMessage(card.GetErrMsg());
		return RET_WF_ERROR;
	}
	
	SetStopKey(false);
	ret = ReadCard(&card,&cardinfo);                    
	if(ret)
	{
		if(RET_WF_ERROR == ret)
		{
			dlg->ShowTipMessage(card.GetErrMsg(), 1);
		}
		goto L_END;                                    // ����ʧ��
	}
	dlg->SetLimtText(1);
	if(dlg->InputPassword("������У԰������",passwd,10) != 0)
	{
		ret = RET_WF_TIMEOUT;
		goto L_END;
	}
	dlg->ShowTipMessage("���״����У��벻Ҫ�ƶ�У԰��",1);
	if(card.TestCardExists(5))
	{
//		dlg->ShowTipMessage("δ�ſ�",5);			  
		ret = RET_WF_ERROR;
		goto L_END;
	}
	
	ret = RET_WF_ERROR;
	drtp = dlg->GetConfig()->CreateDrtpHelper();
	response = dlg->GetConfig()->CreateDrtpHelper();
	while(true)
	{
		hasMore = 0;
		SetStopKey(true);
		drtp->Reset();
		if(drtp->Connect())
		{
			dlg->ShowTipMessage(drtp->GetErrMsg().c_str());
			goto L_END;
		}
		drtp->SetRequestHeader(KS_BANK_TX_CODE,1);
		drtp->AddField(F_SCLOSE_EMP,"240104");				// ������
		drtp->AddField(F_LVOL0,cardinfo.cardid);			// ���׿���
		drtp->AddField(F_SEMP_PWD,(LPSTR)(LPCTSTR)passwd);	// ����
		drtp->AddField(F_DAMT0,cardinfo.balance);			//�뿨ֵ
		drtp->AddField(F_LVOL1,cardinfo.tx_cnt);			//�ۼƽ��״���
		drtp->AddField(F_SNAME,(LPSTR)(LPCTSTR)dlg->GetConfig()->m_systemNo);// ϵͳ���					//�豸�ɣ�
		drtp->AddField(F_SCUST_NO,"system");				//����Ա
		dlg->ShowTipMessage("���ڴ������ף��벻Ҫ�ƶ�У԰��...",1);
		SetStopKey(false);
		if(drtp->SendRequest(3000))
		{
			dlg->ClearMessageLine();
			dlg->AddMessageLine(drtp->GetErrMsg().c_str());
			dlg->AddMessageLine("��[�˳�]����",dlg->GetMaxLineNo()+2,-6);
			dlg->DisplayMessage(3);
			ret = RET_WF_TIMEOUT;
			goto L_END;
		}
		if(drtp->HasMoreRecord())
		{
			iCount++;
			ST_PACK * pData = drtp->GetNextRecord();
			tipMsg.Format("%s ��� %.02f Ԫ��\n����д�����벻Ҫ�ƶ�У԰��..."
						  ,pData->semail,pData->damt1);
			hasMore = pData->lvol2;
			dlg->ShowTipMessage(tipMsg,0);
			ret = DoSubsidy(&card,&cardinfo,pData);                       /////////////////////
			if(RET_WF_ERROR == ret)
			{
				SetStopKey(true);
				response->Reset();
				response->Connect();
				response->SetRequestHeader(900077,1);
				response->AddField(F_SCLOSE_EMP,"240107");
				int serialno = atoi(pData->sserial1);
				response->AddField(F_LVOL1,serialno);
				if(response->SendRequest(3000))
				{
					dlg->ShowTipMessage(response->GetReturnMsg().c_str(),3);
				}
				else
				{
					SetStopKey(false);
					dlg->ClearMessageLine();
					tipMsg = "д��ʧ���뵽�������Ĵ���!";
					dlg->AddMessageLine(tipMsg);
					dlg->AddMessageLine("��[�˳�]����",dlg->GetMaxLineNo()+2,-6);
					dlg->DisplayMessage(1);
				}
				ret = RET_WF_ERROR;
				goto L_END;
			}
			else if(RET_WF_TIMEOUT == ret)
			{
				goto L_END;
			}
			cardinfo.balance = pData->damt2;
			cardinfo.tx_cnt++;
			dlg->ClearMessageLine();
			if(hasMore)
			{
				tipMsg.Format("���� %s ",pData->semail);
				dlg->AddMessageLine(tipMsg);
				tipMsg.Format("��� %0.2f Ԫ����ȡ",pData->damt1);
				dlg->AddMessageLine(tipMsg);
				tipMsg.Format("�����в���δ��ȡ");
				dlg->AddMessageLine(tipMsg,dlg->GetMaxLineNo()+2);
				tipMsg.Format("��[ȷ��]������ȡ����[�˳�]����");
				dlg->AddMessageLine(tipMsg,dlg->GetMaxLineNo()+1,-6);
				// dlg->DisplayMessage(10);
				if(dlg->Confirm(10) != IDOK)
				{
					ret = RET_WF_TIMEOUT;
					goto L_END;
				}
			}
			else
			{
				tipMsg.Format("���� %s ",pData->semail);
				dlg->AddMessageLine(tipMsg);
				tipMsg.Format("��� %0.2f Ԫ����ȡ",pData->damt1);
				dlg->AddMessageLine(tipMsg);
				dlg->AddMessageLine("��[�˳�]����",dlg->GetMaxLineNo()+2,-6);
				dlg->DisplayMessage(3);
				break;
			}
		}
		else
		{
			//û�в���
			dlg->ClearMessageLine();
			ret = drtp->GetReturnCode();
			if(ret != ERR_NOT_LOGIN)
				ret = RET_WF_TIMEOUT;
			dlg->AddMessageLine(drtp->GetReturnMsg().c_str());
			dlg->AddMessageLine("��[�˳�]����",dlg->GetMaxLineNo()+2,-6);
			dlg->DisplayMessage(3);
			
			goto L_END;
		}
	}
	dlg->ShowTipMessage("������ɣ�",1);
	ret = RET_WF_SUCCESSED;
L_END:
	if(drtp)
		delete drtp;
	if(response)
		delete response;
	card.CloseCOM();
	//CDRTPHelper::UninitDrtp();
	SetStopKey(false);
	return ret;
}

CString CKSSubsidyWorkFlow::GetWorkflowID()
{
	return m_key;
}

int CKSSubsidyWorkFlow::OnFormKeyDown(UINT nChar, UINT nFlags)
{
	if(IsTerminate())
	{
		return 0;
	}
	return CKSWorkflow::OnFormKeyDown(nChar,nFlags);
}

int CKSSubsidyWorkFlow::ReadCard(CKSCardManager* manager,KS_CARD_INFO *cardinfo)
{
	int ret = manager->ReadCardInfo(cardinfo,
		CKSCardManager::ciCardId|CKSCardManager::ciBalance,20);
	if(ret)
	{
		ret = manager->GetErrNo();
		if(ERR_USER_CANCEL == ret)
		{
			return RET_WF_TIMEOUT;
		}
		return RET_WF_ERROR;
	}
	return RET_WF_SUCCESSED;
}

int CKSSubsidyWorkFlow::DoSubsidy(CKSCardManager* manager,
								  KS_CARD_INFO *cardinfo,const ST_PACK *rec)
{
	CKSNLDlg* dlg = (CKSNLDlg*)GetMainWnd();
	int ret;
	int retries = 3;
	while(--retries>=0)
	{
		// ��⿨
		if(manager->TestCardExists(5))
		{
			continue;
		}
		// д��, ������������ĺ���, ��Ҫǿתint����
		ret = manager->SetMoney(cardinfo->cardid,D2I(rec->damt2 * 100));
		if(!ret)
		{
			return RET_WF_SUCCESSED;
		}
		if(manager->GetErrNo() == ERR_USER_CANCEL)
		{
			return RET_WF_TIMEOUT;
		}
		dlg->ClearMessageLine();
		dlg->AddMessageLine("д��ʧ�ܣ������·�У԰����");
		dlg->AddMessageLine("��[�˳�]����",dlg->GetMaxLineNo()+2,-6);
		dlg->DisplayMessage(1);
	}
	return RET_WF_ERROR;
}

//////////////////////////////////////////////////////////////////////////
// �˳�
//////////////////////////////////////////////////////////////////////////
CKSExitWorkFlow::CKSExitWorkFlow(CDialog *dlg)
:CKSWorkflow(dlg)
{
	m_key = "exit";
}
CKSExitWorkFlow::~CKSExitWorkFlow()
{
	// 
}
int CKSExitWorkFlow::DoWork()
{
	return RET_WF_EXIT_APP;
}
CString CKSExitWorkFlow::GetWorkflowID()
{
	return m_key;
}
int CKSExitWorkFlow::OnFormKeyDown(UINT nChar, UINT nFlags)
{
	return 0;
}

//////////////////////////////////////////////////////////////////////////
// ��ѯ����
//////////////////////////////////////////////////////////////////////////
CKSQuerySubsidyWorkflow::CKSQuerySubsidyWorkflow(CDialog * dlg)
:CKSWorkflow(dlg)
{
	//
	m_key = _T("qrysubsidy");
}
CKSQuerySubsidyWorkflow::~CKSQuerySubsidyWorkflow()
{
	// 
}
CString CKSQuerySubsidyWorkflow::GetWorkflowID()
{
	return m_key;
}
int CKSQuerySubsidyWorkflow::DoWork()
{
	CDRTPHelper * drtp = NULL;
	CKSNLDlg* dlg = (CKSNLDlg*)GetMainWnd();
	KS_CARD_INFO cardinfo;
	int ret = RET_WF_ERROR;
	int recCount;
	CString passwd;
	CString cardidstr;
	memset(&cardinfo,0,sizeof(cardinfo));

	dlg->ClearAllColumns();
	dlg->AddListColumns("ժҪ",200);
	dlg->AddListColumns("���׿���",70);
	dlg->AddListColumns("��ȡ����",90);
	dlg->AddListColumns("��ȡʱ��",70);
	dlg->AddListColumns("���",75);
	dlg->AddListColumns("״̬",80);
	

	dlg->ShowTipMessage("���У԰��...",0);
	CKSCardManager card(dlg->GetConfig(),
		(LPSTR)(LPCTSTR)dlg->GetConfig()->m_workKey,this);

	if(card.OpenCOM()!=0)
	{
		dlg->ShowTipMessage(card.GetErrMsg());
		return RET_WF_ERROR;
	}
	SetStopKey(false);
	ret = card.ReadCardInfo(&cardinfo,CKSCardManager::ciCardId,5);
	if(ret)
	{
		dlg->ShowTipMessage(card.GetErrMsg(), 1);
		goto L_END;
	}
	dlg->SetLimtText(1);
	if(dlg->InputPassword("������У԰������",passwd,10) != 0)
	{
		ret = RET_WF_TIMEOUT;
		goto L_END;
	}
	dlg->ShowTipMessage("���״�����,��ȴ�...",1);
	cardidstr.Format("%d",cardinfo.cardid);//33396);
	drtp = dlg->GetConfig()->CreateDrtpHelper();
	drtp->SetRequestHeader(KS_BANK_TX_CODE,1);
	drtp->AddField(F_SCLOSE_EMP,"240103");	// ������
	drtp->AddField(F_SSERIAL0,(LPSTR)(LPCTSTR)cardidstr);	// ���׿���
	drtp->AddField(F_SEMP_PWD,(LPSTR)(LPCTSTR)passwd);	// ����
	drtp->AddField(F_SORDER2,(LPSTR)(LPCTSTR)dlg->GetConfig()->m_systemNo);// ϵͳ��� �豸�ɣ�
	drtp->AddField(F_SCUST_NO,"system");			//����Ա
	if(drtp->Connect())
	{
		dlg->ShowTipMessage(drtp->GetErrMsg().c_str());
		goto L_END;
	}
	if(!drtp->SendRequest(3000))
	{
		recCount = 0;
		while(drtp->HasMoreRecord())
		{
			ST_PACK * pData = drtp->GetNextRecord();
			CString money;
			money.Format("%.2fԪ",pData->damt0);
			CString status = (pData->sstatus1[0] == '2')? "δ��ȡ":"����ȡ";
			CString cardno;
			cardno.Format("%d",pData->lvol3);
			CString txdate = ParseDateString(pData->sdate1);
			CString txtime = ParseTimeString(pData->stime1);
			dlg->AddToListInfo(pData->semail,cardno,txdate,txtime,
				(LPCTSTR)money,(LPCTSTR)status);
			recCount++;
			// ֻ��ʾ10��
			if(recCount > 10) break;
		}
		if(recCount > 0)
			dlg->ShowListInfo(25);
		else
			dlg->ShowTipMessage("û�в�����ȡ�ļ�¼!", 2);
		ret = RET_WF_SUCCESSED;
	}
	else
	{
		dlg->ClearMessageLine();
		dlg->AddMessageLine(drtp->GetErrMsg().c_str());
		dlg->AddMessageLine("��[�˳�]����",dlg->GetMaxLineNo()+2,-6);
		dlg->DisplayMessage(3);
		ret = RET_WF_TIMEOUT;
	}
	
L_END:
	if(drtp)
		delete drtp;
	card.CloseCOM();
	SetStopKey(false);
	return ret;
}

//////////////////////////////////////////////////////////////////////////
// ��ѯˮ�����
//////////////////////////////////////////////////////////////////////////
CKSQueryWaterVolumnWorkFlow::CKSQueryWaterVolumnWorkFlow(CDialog *dlg) : CKSWorkflow(dlg)
{
	m_key = _T("qrywatervol");
}

CKSQueryWaterVolumnWorkFlow::~CKSQueryWaterVolumnWorkFlow()
{

}

CString CKSQueryWaterVolumnWorkFlow::GetWorkflowID()
{
	return m_key;
}

int CKSQueryWaterVolumnWorkFlow::DoWork()
{
	CString water_remain = "";
	CDRTPHelper *drtp = NULL;
	CKSNLDlg *dlg = (CKSNLDlg*)GetMainWnd();
	WATER_PACK_INFO water;
	memset(&water, 0, sizeof(water));
	int ret = RET_WF_ERROR;
	
	CKSCardManager card(dlg->GetConfig(),
		(LPSTR)(LPCTSTR)dlg->GetConfig()->m_workKey,this);

	if (card.OpenCOM() != 0)
	{
		dlg->ShowTipMessage(card.GetErrMsg());
		return RET_WF_ERROR; 
	}
	
	dlg->ShowTipMessage("���У԰��...", 0);
	if (card.TestCardExists(5))
	{
		ret = RET_WF_ERROR;
		goto L_END;
	}	

	SetStopKey(false);
	
	ret = SMT_ReadWaterPackInfo(&water);
	if (ret)
	{
		dlg->ShowTipMessage("��ȡˮ��Ǯ����Ϣʧ��", 1);
		goto L_END;
	}

	water_remain.Format("ˮ��Ǯ�����:%0.2lfԪ", water.balance);
	dlg->AddMessageLine(water_remain);
	dlg->AddMessageLine("��[�˳�]����", dlg->GetMaxLineNo() + 3, 0);
	dlg->DisplayMessage(3);

	ret = RET_WF_SUCCESSED;

L_END:
	if (drtp)
		delete drtp;
	card.CloseCOM();
	SetStopKey(false);
	return ret;
}

//////////////////////////////////////////////////////////////////////////
// CKSQueryTransferAccoutsWorkFlow (��ѯת����ϸ)
CKSQueryTransferAccoutsWorkFlow::CKSQueryTransferAccoutsWorkFlow(CDialog *dlg) : CKSWorkflow(dlg)
{
	m_key = _T("qryTrfAct");
}

CKSQueryTransferAccoutsWorkFlow::~CKSQueryTransferAccoutsWorkFlow()
{

}

CString CKSQueryTransferAccoutsWorkFlow::GetWorkflowID()
{
	return m_key;
}

//////////////////////////////////////////////////////////////////////////
// ���ܱ��: 240201
// ��������: ��ѯת����ϸ
// ��������: ��ѯ�û���ʷ��ˮ��ϸ�����10��
// ��������: ������(sclose_emp), ���׿���(lvol0), ����(semp_pwd)
// �������: ��ˮ��ֵ��ˮ��(lvo10), �ͻ���(lvo11), ���׿���(lvo12)
//			 ��ˮ����(sdate0, 8�ַ�), ��ˮʱ��(stime0, 6�ַ�), ����ˮ��(lvol3)
//////////////////////////////////////////////////////////////////////////
int CKSQueryTransferAccoutsWorkFlow::DoWork()
{
	CDRTPHelper *drtp = NULL;
	CKSNLDlg *dlg = (CKSNLDlg*)GetMainWnd();
	KS_CARD_INFO cardInfo;
	int ret = RET_WF_ERROR;
	int retCount;
	CString passwd;
	CString cardidstr;
	memset(&cardInfo, 0, sizeof(cardInfo));

	dlg->ClearAllColumns();
	dlg->AddListColumns("��ˮ��ֵ��ˮ��", 120);
	dlg->AddListColumns("�ͻ���", 80);
	dlg->AddListColumns("���׿���", 70);
	dlg->AddListColumns("��ˮ����", 90);
	dlg->AddListColumns("��ˮʱ��", 70);
	dlg->AddListColumns("����ˮ��(��)", 120);

	dlg->ShowTipMessage("���У԰��...", 0);
	CKSCardManager card(dlg->GetConfig(), (LPSTR)(LPCTSTR)dlg->GetConfig()->m_workKey, this);

	if (card.OpenCOM() != 0)
	{
		dlg->ShowTipMessage(card.GetErrMsg());
		return RET_WF_ERROR;
	}
	SetStopKey(false);
	ret = card.ReadCardInfo(&cardInfo, CKSCardManager::ciCardId, 5);
	if (ret)
	{
		dlg->ShowTipMessage(card.GetErrMsg(), 1);
		goto L_END;
	}
	dlg->SetLimtText(1);
	if (dlg->InputPassword("������У԰������", passwd, 10) != 0)
	{
		ret = RET_WF_ERROR;
		goto L_END;
	}
	dlg->ShowTipMessage("���״�����, ��ȴ�", 1);
	cardidstr.Format("%d", cardInfo.cardid);
	drtp = dlg->GetConfig()->CreateDrtpHelper();
	drtp->SetRequestHeader(KS_BANK_TX_CODE, 1);
	drtp->AddField(F_SCLOSE_EMP, "240201");
	drtp->AddField(F_LVOL0, (LPSTR)(LPCTSTR)cardidstr);
	drtp->AddField(F_SEMP_PWD, (LPSTR)(LPCTSTR)passwd);
	drtp->AddField(F_SNAME,(LPSTR)(LPCTSTR)dlg->GetConfig()->m_systemNo);
	if (drtp->Connect())
	{
		dlg->ShowTipMessage(drtp->GetErrMsg().c_str());
		goto L_END;
	}
	if (drtp->SendRequest(3000))
	{
		dlg->ClearMessageLine();
		dlg->AddMessageLine(drtp->GetErrMsg().c_str());
		dlg->AddMessageLine("��[�˳�]����", dlg->GetMaxLineNo() + 2, -6);
		dlg->DisplayMessage(2);
		ret = RET_WF_TIMEOUT;
		goto L_END;
	}
	else if(drtp->GetReturnCode())
	{
		dlg->ShowTipMessage(drtp->GetReturnMsg().c_str());
		goto L_END;
	}
	else
	{
		retCount = 0;
		while (drtp->HasMoreRecord())
		{
			ST_PACK *pData = drtp->GetNextRecord();
			CString cardWaterNo;                  // ��ˮ��ֵ��ˮ��
			cardWaterNo.Format("%d", pData->lvol0);
			CString userNo;						  // �ͻ���
			userNo.Format("%d", pData->lvol1);
			CString cardNo;					      // ���׿���
			cardNo.Format("%d", pData->lvol2);
			CString txtdate = ParseDateString(pData->sdate0);
			CString txtime = ParseTimeString(pData->stime0);
			CString waterVolumn;
			waterVolumn.Format("%d", D2I(pData->lvol3/1000));
			dlg->AddToListInfo(cardWaterNo, userNo, cardNo, txtdate, txtime, waterVolumn);
			retCount++;
			// ֻ��ʾ10������
			if (retCount > 10)
			{
				break;
			}
		}
		if (retCount > 0)
		{
			dlg->ShowListInfo(25);
		}
		else
		{
			dlg->ShowTipMessage("û��ת�ʵļ�¼!", 2);
		}
		ret = RET_WF_SUCCESSED;
	}
	/*
	else
	{
		dlg->ClearMessageLine();
		dlg->AddMessageLine(drtp->GetErrMsg().c_str());
		dlg->AddMessageLine("��[�˳�]����", dlg->GetMaxLineNo() + 2, -6);
		ret = RET_WF_TIMEOUT;
	}
	*/
L_END:
	if(drtp)
		delete drtp;
	card.CloseCOM();
	SetStopKey(false);
	return ret;
}

//////////////////////////////////////////////////////////////////////////
// CKSWaterPacketTransferWorkFlow (ˮ��Ǯ��ת��)
CKSWaterPacketTransferWorkFlow::CKSWaterPacketTransferWorkFlow(CDialog *dlg) : CKSWorkflow(dlg)
{
	m_key = _T("waterpacketrf");
}

CKSWaterPacketTransferWorkFlow::~CKSWaterPacketTransferWorkFlow()
{

}

CString CKSWaterPacketTransferWorkFlow::GetWorkflowID()
{
	return m_key;
}

int CKSWaterPacketTransferWorkFlow::ReadCard(CKSCardManager* manager,KS_CARD_INFO *cardinfo)
{
	int ret = manager->ReadCardInfo(cardinfo,
		CKSCardManager::ciCardId | CKSCardManager::ciBalance, 20);
	if(ret)
	{
		ret = manager->GetErrNo();
		if(ERR_USER_CANCEL == ret)
		{
			return RET_WF_TIMEOUT;
		}
		return RET_WF_ERROR;
	}
	return RET_WF_SUCCESSED;
}

//////////////////////////////////////////////////////////////////////////
// ���ܱ��: 260001
// ��������: ˮ��Ǯ��ת��
// ��������: �û�ͨ��Ȧ����Ӵ�Ǯ���й�ˮ, Ȼ��д�뵽ˮ��Ǯ����
// ҵ���߼�: 1.	����û���״̬����鿨���
//			 2. ����δ���˽���, ��һ��δ������ˮ
//			 3.	�ӿ��д�Ǯ���۳����, �ɹ���4, ������д��ʧ�ܽ���
//           4.	�����̨����, ��Ǯ����ˮ����, ����СǮ�����
//           5.	СǮ��д��, �ɹ���6, ������д��ʧ�ܽ���
//           6.	Ȧ�����չʾ�ۿ���Ϣ
// ��ع���: ���д��ʧ�ܷ���ת��ʧ�ܽ���, ������240111
// ��������: 
//			 
// �������: 
//			 
//////////////////////////////////////////////////////////////////////////
int CKSWaterPacketTransferWorkFlow::DoWork()
{
	CString password = "";
	CString trade_money = "";
	CString tipMsg = "";
	CDRTPHelper *drtp = NULL;
	CDRTPHelper *response = NULL;
	KS_CARD_INFO cardInfo;
	int ret = 0;
	int iCount = 0;
	int trs_serial_no = 0;
	int water_serial_no = 0;
	double water_balance_remain = 0.0;
	WATER_PACK_INFO water;
	CKSNLDlg *dlg = (CKSNLDlg*)GetMainWnd();
	memset(&cardInfo, 0, sizeof(cardInfo));
	memset(&water, 0, sizeof(water));
	SetStopKey(false);
	dlg->ShowTipMessage("���У԰��...", 0);
	CKSCardManager card(dlg->GetConfig(),
		(LPSTR)(LPCTSTR)dlg->GetConfig()->m_workKey,this);
	if (card.OpenCOM() != 0)
	{
		dlg->ShowTipMessage(card.GetErrMsg());
		return RET_WF_ERROR;
	}
	// ��ȡ����Ϣ
	ret = ReadCard(&card, &cardInfo);
	if (ret)
	{
		if (RET_WF_ERROR == ret)
		{
			dlg->ShowTipMessage(card.GetErrMsg(), 1);					// �޸Ĺ�
		}
		goto L_END;
	}

	ret = SMT_ReadWaterPackInfo(&water);
	if (ret)
	{
		dlg->ShowTipMessage("��ȡˮ��Ǯ����Ϣʧ��", 1);
		goto L_END;
	}

	dlg->SetLimtText(1);
	if (dlg->InputPassword("������У԰������", password, 10) != 0)
	{
		ret = RET_WF_TIMEOUT;
		goto L_END;
	}

	dlg->ShowTipMessage("���״����У� �벻Ҫ�ƶ�У԰��", 1);
	if (card.TestCardExists(5))
	{		
		ret = RET_WF_ERROR;
		goto L_END;
	}
/*
	dlg->SetLimtText(0);
	if (dlg->InputQuery("������ת�˽��", "", trade_money, 10) != 0)
	{
		ret = RET_WF_TIMEOUT;
		goto L_END;
	}
*/

	dlg->SetLimtText(3);
	tipMsg.Format("ת�˽��5 10 20 (Ԫ)");
	if (dlg->InputQuery(tipMsg, "", trade_money, 10) != 0)
	{
		ret = RET_WF_TIMEOUT;
		goto L_END;
	}
	
	if (trade_money.Compare("5") != 0	&&
		trade_money.Compare("10") != 0	&&
		trade_money.Compare("20") != 0
		)
	{
		dlg->ShowTipMessage("����ת�˽���, �����²���", 5);
		ret = RET_WF_ERROR;
		goto L_END;		
	}


	dlg->ShowTipMessage("���״�����, �벻Ҫ�ƶ�У԰��", 1);
	if (card.TestCardExists(5))
	{
		ret = RET_WF_ERROR;
		goto L_END;
	}	
	ret = RET_WF_ERROR;
	drtp = dlg->GetConfig()->CreateDrtpHelper();
	response = dlg->GetConfig()->CreateDrtpHelper();
	while (true)
	{
		SetStopKey(true);
		drtp->Reset();
		if (drtp->Connect())
		{
			dlg->ShowTipMessage(drtp->GetErrMsg().c_str());
			goto L_END;
		}
		// ����drtp�������
		drtp->SetRequestHeader(KS_TRANS, 1);
//		drtp->AddField(F_SCLOSE_EMP, "240202");
		drtp->AddField(F_LCERT_CODE, 240201);							// ˮ��ת��
		drtp->AddField(F_LVOL0, cardInfo.cardid);
		drtp->AddField(F_LVOL1, cardInfo.tx_cnt);
		drtp->AddField(F_LVOL2, PURSE_NO1);
		drtp->AddField(F_DAMT0, cardInfo.balance);
		drtp->AddField(F_DAMT1, (LPSTR)(LPCTSTR)trade_money);	
		drtp->AddField(F_SEMP_PWD, (LPSTR)(LPCTSTR)password);
		drtp->AddField(F_SCUST_NO, "system");
		drtp->AddField(F_LVOL4, atoi((LPSTR)(LPCTSTR)dlg->GetConfig()->m_systemNo));		
		drtp->AddField(F_LVOL9, TRADE_NODEBT);														
		SetStopKey(false);
		// ��������
		if (drtp->SendRequest(5000))
		{
			dlg->ClearMessageLine();
			dlg->AddMessageLine(drtp->GetErrMsg().c_str());
			dlg->AddMessageLine("��[�˳�]����", dlg->GetMaxLineNo() + 2, 0);
			dlg->DisplayMessage(3);
			ret = RET_WF_TIMEOUT;
			goto L_END;
		}
		// ��ȡ���ؼ�¼
		if (drtp->HasMoreRecord())
		{
			ST_PACK *pData = drtp->GetNextRecord();
			tipMsg.Format("����д��, �벻Ҫ�ƶ�У԰��...", 1);
			dlg->ShowTipMessage(tipMsg, 0);
			// д��(��Ǯ���м�Ǯ), �����и�����ֵ
			ret = DoTransferValue(&card, &cardInfo, pData);
			if (RET_WF_ERROR == ret)
			{
				SetStopKey(false);
				dlg->ClearMessageLine();
				tipMsg = "д��ʧ��������ת��";
				dlg->AddMessageLine(tipMsg);
				dlg->AddMessageLine("��[�˳�]����", dlg->GetMaxLineNo() + 2, 0);
				dlg->DisplayMessage(1);
				ret = RET_WF_ERROR;
				goto L_END;
			}
			// ��ʱ
			if (RET_WF_TIMEOUT == ret)
			{
				goto L_END;
			}
		
			trs_serial_no = pData->lvol0;
			water_serial_no = pData->lvol1;

			// �����Ǯ�����˽���
			drtp->Reset();
			if (drtp->Connect())
			{
				dlg->ShowTipMessage(drtp->GetErrMsg().c_str());
				goto L_END;
			}	
			// ����drtp�������	
			drtp->SetRequestHeader(KS_TRANS, 1);
//			drtp->AddField(F_SCLOSE_EMP, "240202");
			drtp->AddField(F_LCERT_CODE, 240201);							// ˮ��ת������
			drtp->AddField(F_LVOL0, cardInfo.cardid);
			drtp->AddField(F_LVOL4, atoi((LPSTR)(LPCTSTR)dlg->GetConfig()->m_systemNo));
			drtp->AddField(F_SEMP_PWD, (LPSTR)(LPCTSTR)password);
			drtp->AddField(F_LVOL6, trs_serial_no);
			drtp->AddField(F_LVOL7, water_serial_no);
			drtp->AddField(F_LVOL9, TRADE_DEBT);
			SetStopKey(false);
			if (drtp->SendRequest(3000))
			{
				dlg->ClearMessageLine();
				dlg->AddMessageLine(drtp->GetErrMsg().c_str());
				dlg->AddMessageLine("��[�˳�]����", dlg->GetMaxLineNo() + 2, 0);
				dlg->DisplayMessage(3);
				ret = RET_WF_TIMEOUT;
				goto L_END;
			}
			
			if (drtp->HasMoreRecord())
			{
				pData = drtp->GetNextRecord();
				water.price1 = pData->damt10;			// ˮ����1
				water.price2 = pData->damt11;			// ˮ����2
				water.price3 = pData->damt12;			// ˮ����3
				cardInfo.balance = pData->damt0;		// ����ֵ
			}
			else
			{
				dlg->ClearMessageLine();
				ret = drtp->GetReturnCode();
				if (ret != ERR_NOT_LOGIN)
				{
					ret = RET_WF_TIMEOUT;
				}
				dlg->AddMessageLine(drtp->GetReturnMsg().c_str());
				dlg->AddMessageLine("��[�˳�]����", dlg->GetMaxLineNo() + 2, 0);
				dlg->DisplayMessage(3);
				goto L_END;	
			}
			
			if (RET_WF_ERROR == ReadWaterCard(&water_balance_remain))
			{
				tipMsg = "��ȡˮ��Ǯ��ʧ��";
				dlg->AddMessageLine(tipMsg);
				dlg->AddMessageLine("��[�˳�]����", dlg->GetMaxLineNo() + 2, 0);
				dlg->DisplayMessage(1);
				ret = RET_WF_ERROR;
				goto L_END;
			}
			
			water.balance = (water_balance_remain += pData->damt1);

			ret = DoTransferWater(&card, &water);
			if (RET_WF_ERROR == ret)
			{
				SetStopKey(true);
				response->Reset();
				response->Connect();
				response->SetRequestHeader(KS_WRITE_CARD_FAILED, 1);		// д��ʧ�ܴ���
				response->AddField(F_SCLOSE_EMP, 240111);
				response->AddField(F_LVOL1, water_serial_no);				// ��ֵ��ˮ��
				if (response->SendRequest(3000))
				{
					dlg->ShowTipMessage(response->GetReturnMsg().c_str(), 3);
				}
				else
				{
					SetStopKey(false);
					dlg->ClearMessageLine();
					tipMsg = "д��ʧ���뵽�������Ĵ���!";
					dlg->AddMessageLine(tipMsg);
					dlg->AddMessageLine("��[�˳�]����", dlg->GetMaxLineNo() + 2, 0);
					dlg->DisplayMessage(1);
				}
				ret = RET_WF_ERROR;
				goto L_END;
			}

			if (RET_WF_TIMEOUT == ret)
			{
				goto L_END;
			}
			// д���ɹ�
			// modify by TC 2008-10-09
			// ��Ϊ��Ҫ�ӳ���Ƭ��Ч�ڣ���Ϊ����
			ChangeValidDate();
			SetStopKey(false);
			dlg->ClearMessageLine();
			tipMsg.Format("ת�˽��:%0.2fԪ", atof((LPSTR)(LPCTSTR)trade_money));
			dlg->AddMessageLine(tipMsg, dlg->GetMaxLineNo() + 1, 0);
			tipMsg.Format("�����:%0.2fԪ", cardInfo.balance);
			dlg->AddMessageLine(tipMsg, dlg->GetMaxLineNo() + 1, 0);
			tipMsg.Format("ˮ��Ǯ�����%0.2fԪ", water_balance_remain);
			dlg->AddMessageLine(tipMsg, dlg->GetMaxLineNo() + 1, 0);
			dlg->AddMessageLine("�밴[�˳�]����",dlg->GetMaxLineNo() + 3, 0);
			dlg->DisplayMessage(10);
			break;
		}
		else
		{
			dlg->ClearMessageLine();
			ret = drtp->GetReturnCode();
			if (ret != ERR_NOT_LOGIN)
			{
				ret = RET_WF_TIMEOUT;
			}
			dlg->AddMessageLine(drtp->GetReturnMsg().c_str());
			dlg->AddMessageLine("��[�˳�]����", dlg->GetMaxLineNo() + 2, 0);
			dlg->DisplayMessage(3);
			goto L_END;
		}
	}

	dlg->ShowTipMessage("�������!", 1);
	ret = RET_WF_SUCCESSED;
L_END:
	if(drtp)
		delete drtp;
	if(response)
		delete response;
	card.CloseCOM();
	SetStopKey(false);
	return ret;
}

// ��Ǯ������
int CKSWaterPacketTransferWorkFlow::DoTransferValue(CKSCardManager* manager, KS_CARD_INFO *cardinfo, const ST_PACK *rec)
{
	CKSNLDlg *dlg = (CKSNLDlg*)GetMainWnd();
	int ret;
	int retries = 3;
	while (--retries >= 0)
	{
		if (manager->TestCardExists(5))
		{
			continue;
		}
		// д��
		ret = manager->SetMoney(cardinfo->cardid, D2I(rec->damt0 * 100));
		if (!ret)
		{
			return RET_WF_SUCCESSED;
		}
		if (manager->GetErrNo() == ERR_USER_CANCEL)
		{
			return RET_WF_TIMEOUT;
		}
		dlg->ClearMessageLine();
		dlg->AddMessageLine("д��ʧ��, �����·�У԰��!");
		dlg->AddMessageLine("��[�˳�]����", dlg->GetMaxLineNo() + 2, -6);
		dlg->DisplayMessage(1);
	}
	return RET_WF_ERROR;
}

// СǮ������
int CKSWaterPacketTransferWorkFlow::DoTransferWater(CKSCardManager* manager, WATER_PACK_INFO *water)
{
	CKSNLDlg *dlg = (CKSNLDlg*)GetMainWnd();
	int ret;
	int retries = 3;
	while (--retries >= 0)
	{
		if (manager->TestCardExists(5))
		{
			continue;
		}
		ret = SMT_WriteWaterPackInfo(water);
		if (!ret)
		{
			return	RET_WF_SUCCESSED;
		}
		if (manager->GetErrNo() == ERR_USER_CANCEL)
		{
			return RET_WF_TIMEOUT;	
		}
		dlg->ClearMessageLine();
		dlg->AddMessageLine("д��ʧ��, �����·�У԰��!");
		dlg->AddMessageLine("��[�˳�]����", dlg->GetMaxLineNo() + 2, -6);
		dlg->DisplayMessage(1);
	}
	return RET_WF_ERROR;
}

// ��ȡСǮ�����
int CKSWaterPacketTransferWorkFlow::ReadWaterCard(double *balance)
{
	int ret = 0;
	WATER_PACK_INFO water;
	memset(&water, 0, sizeof(water));
	if (ret = SMT_ReadWaterPackInfo(&water))
		return RET_WF_ERROR;
	
	*balance = water.balance;
	return RET_WF_SUCCESSED;
}

int CKSWaterPacketTransferWorkFlow::ChangeValidDate()
{
	//
	int ret,i;
	unsigned char dead_line[3];
	char date_str[9];
	char end_date[] = "120901";
	char temp[5] = "";
	memset(dead_line,0,sizeof dead_line);
	ret = SMT_ReadDeadLineDate(dead_line);
	for(i = 0;i < 3;++i)
		sprintf(date_str+i*2,"%02d",(unsigned char)dead_line[i]);
	// ��Ч��С�� 20091230
	if(strncmp(date_str,"091230",6)<=0)
	{
		// ������Ч�ڵ� 20120901
		for(i = 0;i < 3;++i)
		{
			memcpy(temp,end_date+i*2,2);
			dead_line[i] = atoi(temp);
		}
		ret = SMT_ChangeDeadLineDate(dead_line);
		return ret;
	}
	return 0;
}
//////////////////////////////////////////////////////////////////////////
// ���ܱ��: 240203
// ��������: �������ɶ����ˮ�ؿ�
// ��������: �û�ͨ��Ȧ����Ӵ�Ǯ���й�ˮ, Ȼ��д�뵽ˮ��Ǯ����
// ҵ���߼�: 1.	�û���������
//			 2. �û�����ˮ����
//           3.	���̨������, ���Ϳ��Ķ���ˮ����
// ��������: ������(sclose_emp), ���׿���(lvol0), ����(semp_pwd), ˮ����(lvol1)
//////////////////////////////////////////////////////////////////////////
CKSInitCardWorkFlow::CKSInitCardWorkFlow(CDialog *dlg) : CKSWorkflow(dlg)
{
	m_key = _T("InitCard");
}

CKSInitCardWorkFlow::~CKSInitCardWorkFlow()
{

}

CString CKSInitCardWorkFlow::GetWorkflowID()
{
	return m_key;
}

int CKSInitCardWorkFlow::DoWork()
{
	return 0;
	/*
	CDRTPHelper *drtp = NULL;
	CKSNLDlg *dlg = (CKSNLDlg*)GetMainWnd();
	KS_CARD_INFO cardInfo;
	WATER_PACK_INFO water, tempWater;
	CString inputWaterTermId;
	int ret = RET_WF_ERROR;
//	int retCount;
	CString password;
	memset(&cardInfo, 0, sizeof(cardInfo));
	memset(&water, 0, sizeof(water));
	memset(&tempWater, 0, sizeof(tempWater));
	SetStopKey(false);
	dlg->ShowTipMessage("���У԰��...", 0);
	CKSCardManager card(dlg->GetConfig(), 
		(LPSTR)(LPCTSTR)dlg->GetConfig()->m_workKey,this);
	if (card.OpenCOM() != 0)
	{
		dlg->ShowTipMessage(card.GetErrMsg());
		return RET_WF_ERROR;
	}
	SetStopKey(false);
	ret = card.ReadCardInfo(&cardInfo, CKSCardManager::ciCardId, 5);
	if (ret)
	{
		dlg->ShowTipMessage(card.GetErrMsg(), 1);
		goto L_END;
	}
	// ����ǵ�һ��û�����ɶ��㿨�Ŀ�.
	if (SMT_ReadWaterPackInfo(&tempWater) == 0)
	{
		water.remain = tempWater.remain;
	}
	dlg->SetLimtText(1);
	if (dlg->InputPassword("������У԰����", password, 10) != 0)
	{
		ret = RET_WF_TIMEOUT;
		goto L_END;
	}
	dlg->ShowTipMessage("���״�����, �벻Ҫ�ƶ�У԰��", 1);
	if (card.TestCardExists(5))
	{
	//	dlg->ShowTipMessage("δ�ſ�", 5);		
		ret = RET_WF_ERROR;
		goto L_END;
	}
	dlg->SetLimtText(1);
	if (dlg->InputQuery("������ˮ����", "", inputWaterTermId, 10) != 0)
	{
		ret = RET_WF_TIMEOUT;
		goto L_END;
	}
	dlg->ShowTipMessage("���״�����, �벻Ҫ�ƶ�У԰��", 1);
	SetStopKey(false);
	if (card.TestCardExists(5))
	{
  	//	dlg->ShowTipMessage("δ�ſ�", 5);       
		ret = RET_WF_ERROR;
		goto L_END;
	}
	water.cardtype = WATER_FIXED_CARD;
	water.termid = atoi((LPCTSTR)inputWaterTermId);
	// �¼����һ�仰, ��д����ʣ����
	water.remain = tempWater.remain;
	ret = RET_WF_ERROR;
	drtp = dlg->GetConfig()->CreateDrtpHelper();
	drtp->SetRequestHeader(KS_BANK_TX_CODE, 1);
	drtp->AddField(F_SCLOSE_EMP, "240203");
	drtp->AddField(F_LVOL0, cardInfo.cardid);
	drtp->AddField(F_SEMP_PWD, (LPSTR)(LPCTSTR)password);
	drtp->AddField(F_LVOL1, water.termid);
	if (drtp->Connect())
	{
		dlg->ShowTipMessage(drtp->GetErrMsg().c_str());
		goto L_END;
	}
	if (drtp->SendRequest(3000))
	{
		dlg->ClearMessageLine();
		dlg->AddMessageLine(drtp->GetErrMsg().c_str());
		dlg->AddMessageLine("��[�˳�]����", dlg->GetMaxLineNo() + 2, -6);
		dlg->DisplayMessage(2);
		ret = RET_WF_TIMEOUT;
		goto L_END;
	}
	//////////////////////////////////////////////////////////////////////////
	// �¼���ļ�ⷵ�ص�ֵ�Ƿ���ȷ
	if(drtp->GetReturnCode())
	{
		dlg->ShowTipMessage(drtp->GetReturnMsg().c_str());
		ret = RET_WF_ERROR;
		goto L_END;
	}
	//////////////////////////////////////////////////////////////////////////
	if (card.TestCardExists(5))
	{
	//	dlg->ShowTipMessage("δ�ſ�", 5);			
		ret = RET_WF_ERROR;
		goto L_END;
	}
	if (SMT_InitWaterInfo(&water) != 0)
	{
		dlg->ShowTipMessage("д���㿨ʧ��, �����²���");
		goto L_END;
	}
	dlg->ShowTipMessage("���㿨����ɹ�!", 1);
	ret = RET_WF_SUCCESSED;
L_END:
	if(drtp)
		delete drtp;
	card.CloseCOM();
	SetStopKey(false);
	return ret;
	*/
}

//////////////////////////////////////////////////////////////////////////
// ���㿨��ȡ��
//////////////////////////////////////////////////////////////////////////
CKSCancelTermCardWorkFlow::CKSCancelTermCardWorkFlow(CDialog *dlg) : CKSWorkflow(dlg)
{
	m_key = _T("cancel");
}

CKSCancelTermCardWorkFlow::~CKSCancelTermCardWorkFlow()
{

}

CString CKSCancelTermCardWorkFlow::GetWorkflowID()
{
	return m_key;
}
//////////////////////////////////////////////////////////////////////////
// ���ܱ��: 240204
// ��������: ���㿨ȡ��
// ��������: ȡ������ˮ���İ�
// ��������: ���״���(sclose_emp, 240204), ���׿���(lvol0), ����(semp_pwd)
//////////////////////////////////////////////////////////////////////////
int CKSCancelTermCardWorkFlow::DoWork()
{
	return 0;
	/*
	CDRTPHelper *drtp = NULL;
	CKSNLDlg *dlg = (CKSNLDlg*)GetMainWnd();
	WATER_PACK_INFO water;
	KS_CARD_INFO cardInfo;
//	int retCount;
	CString password;
	int ret = RET_WF_ERROR;
	memset(&water, 0, sizeof(water));
	memset(&cardInfo, 0, sizeof(cardInfo));
	SetStopKey(false);
	dlg->ShowTipMessage("���У԰��...", 0);
	CKSCardManager card(dlg->GetConfig(), 
		(LPSTR)(LPCTSTR)dlg->GetConfig()->m_workKey, this);
	
	if (card.OpenCOM() != 0)
	{
		dlg->ShowTipMessage(card.GetErrMsg());
		return RET_WF_ERROR;
	}
	SetStopKey(false);
	ret = card.ReadCardInfo(&cardInfo, CKSCardManager::ciCardId, 5);
	if (ret)
	{
		dlg->ShowTipMessage(card.GetErrMsg(), 1);
		goto L_END;		
	}
	
	if (SMT_ReadWaterPackInfo(&water) != 0)
	{
		dlg->ShowTipMessage("����ʧ��, �����²���!");
		goto L_END;
	}
	water.cardtype = WATER_COMMON_CARD;
	dlg->SetLimtText(1);
	if (dlg->InputPassword("����������", password, 10) != 0)
	{
		ret = RET_WF_TIMEOUT;
		goto L_END;
	}
	dlg->ShowTipMessage("���״�����, ��ȴ�...", 1);
	drtp = dlg->GetConfig()->CreateDrtpHelper();
	drtp->SetRequestHeader(KS_BANK_TX_CODE, 1);
	drtp->AddField(F_SCLOSE_EMP, "240204");
	drtp->AddField(F_LVOL0, cardInfo.cardid);
	drtp->AddField(F_SEMP_PWD, (LPSTR)(LPCTSTR)password);
	if (drtp->Connect())
	{
		dlg->ShowTipMessage(drtp->GetErrMsg().c_str());
		goto L_END;		
	}
	if (drtp->SendRequest(3000))
	{
		dlg->ClearMessageLine();
		dlg->AddMessageLine(drtp->GetErrMsg().c_str());
		dlg->AddMessageLine("��[�˳�]����", dlg->GetMaxLineNo() + 2, -6);
		dlg->DisplayMessage(2);
		ret = RET_WF_TIMEOUT;
		goto L_END;
	}
	//////////////////////////////////////////////////////////////////////////
	// �¼���ļ�ⷵ�ص�ֵ�Ƿ���ȷ
	if(drtp->GetReturnCode())
	{
		dlg->ShowTipMessage(drtp->GetReturnMsg().c_str());
		ret = RET_WF_ERROR;
		goto L_END;
	}
	//////////////////////////////////////////////////////////////////////////
	if (card.TestCardExists(5))
	{
	//	dlg->ShowTipMessage("δ�ſ�", 5);				
		ret = RET_WF_ERROR;
		goto L_END;
	}
	if (SMT_WriteWaterPackInfo(&water) != 0)
	{
		dlg->ShowTipMessage("д���㿨ʧ��, �����²���");
		goto L_END;
	}
	dlg->ShowTipMessage("���㿨ȡ���ɹ�!", 2);	
	return RET_WF_SUCCESSED;
L_END:
	if (drtp)
		delete drtp;
	card.CloseCOM();
	return ret;
	*/
}

//////////////////////////////////////////////////////////////////////////
// ���ת��
//////////////////////////////////////////////////////////////////////////
CKSElePacketTransferWorkFLow::CKSElePacketTransferWorkFLow(CDialog *dlg) : CKSWorkflow(dlg)
{
	m_key = "ElePackTrf";
}

CKSElePacketTransferWorkFLow::~CKSElePacketTransferWorkFLow()
{
	//	
}

CString CKSElePacketTransferWorkFLow::GetWorkflowID()
{
	return m_key;
}

int CKSElePacketTransferWorkFLow::DoWork()
{
	CString password;
	CString tipMsg("");
	CString eleValue;
	CString mateRoomNo;
	char cMateRoom = NULL;
	CDRTPHelper *drtp = NULL;
	CDRTPHelper *response = NULL;      // ������ˮ
	KS_CARD_INFO cardinfo;
	int ret;
	int iCount = 0;
	int hasMore = 0;
	CKSNLDlg *dlg = (CKSNLDlg*)GetMainWnd();
	memset(&cardinfo, 0, sizeof(cardinfo));
	SetStopKey(false);	
	dlg->ShowTipMessage("���У԰��...", 0);
	CKSCardManager card(dlg->GetConfig(), 
		(LPSTR)(LPCTSTR)dlg->GetConfig()->m_workKey, this);
	if (card.OpenCOM() != 0)
	{
		dlg->ShowTipMessage(card.GetErrMsg());
		return RET_WF_ERROR;
	}
	SetStopKey(false);
	ret = ReadCard(&card, &cardinfo);
	if (ret == RET_WF_ERROR)
	{
		dlg->ShowTipMessage(card.GetErrMsg(), 1);
		goto L_END;	
	}
	dlg->SetLimtText(1);
	if (dlg->InputPassword("������У԰������", password, 10) != 0)
	{
		ret = RET_WF_TIMEOUT;
		goto L_END;
	}
	dlg->ShowTipMessage("���״����У� �벻Ҫ�ƶ�У԰��", 1);
	if (card.TestCardExists(5))
	{
	//	dlg->ShowTipMessage("δ�ſ�", 5);			
		ret = RET_WF_ERROR;
		goto L_END;
	}
	dlg->SetLimtText(1);
	if (dlg->InputQuery("�����������", "", mateRoomNo, 10) != 0)
	{
		ret = RET_WF_TIMEOUT;
		goto L_END;
	}
	//////////////////////////////////////////////////////////////////////////
	// �ж�������Ƿ���Ϲ淶
	cMateRoom = mateRoomNo.GetAt(0);
	if (mateRoomNo.GetLength() != 6 || (cMateRoom != '9' && cMateRoom != '8'))
	{
		dlg->ShowTipMessage("���������Ų���, �����²���", 5);
		ret = RET_WF_ERROR;
		goto L_END;		
	}
	//////////////////////////////////////////////////////////////////////////
	dlg->ShowTipMessage("���״����У��벻Ҫ�ƶ�У԰��", 1);
	if (card.TestCardExists(5))
	{
	//	dlg->ShowTipMessage("δ�ſ�", 5);				
		ret = RET_WF_ERROR;
		goto L_END;
	}

	ret = RET_WF_ERROR;
	drtp = dlg->GetConfig()->CreateDrtpHelper();
	response = dlg->GetConfig()->CreateDrtpHelper();
	while (true)
	{
		hasMore = 0;
		SetStopKey(true);
		drtp->Reset();
		if (drtp->Connect())
		{
			dlg->ShowTipMessage(drtp->GetErrMsg().c_str());
			goto L_END;
		}
		drtp->SetRequestHeader(KS_BANK_TX_CODE, 1);
		drtp->AddField(F_SCLOSE_EMP, "240208");
		drtp->AddField(F_SNAME, (LPSTR)(LPCSTR)dlg->GetConfig()->m_systemNo); // ϵͳ���
		drtp->AddField(F_SNAME2, (LPSTR)(LPCSTR)mateRoomNo); // �����
		dlg->ShowTipMessage("���ڶ�ȡ������Ϣ, ��ȴ�...", 1);		
		SetStopKey(false);
		if (drtp->SendRequest(10000))
		{
			dlg->ClearMessageLine();
			dlg->AddMessageLine(drtp->GetErrMsg().c_str());
			dlg->AddMessageLine("��[�˳�]����", dlg->GetMaxLineNo() + 2, -6);
			dlg->DisplayMessage(2);
			goto L_END;		
		}
		if(drtp->GetReturnCode())
		{
			dlg->ShowTipMessage(drtp->GetReturnMsg().c_str());
			ret = RET_WF_ERROR;
			goto L_END;
		}
		if(drtp->HasMoreRecord())
		{
			ST_PACK *pData = drtp->GetNextRecord();
			//AfxMessageBox(pData->vsmess);
			//dlg->ShowTipMessage(pData->vsmess);
			dlg->AddMessageLine(pData->vsmess);
			dlg->AddMessageLine("��[ȷ��]��һ��, ��[�˳�]����", dlg->GetMaxLineNo() + 2, -6);
			if(dlg->Confirm(10) != IDOK)
			{
				ret = RET_WF_TIMEOUT;
				goto L_END;
			}
		}
		else
		{
			ret= RET_WF_ERROR;
			goto L_END;
		}
//////////////////////////////////////////////////////////////////////////
		if (dlg->InputQuery("�����빺����", "", eleValue, 10) != 0)
		{
			ret = RET_WF_TIMEOUT;
			goto L_END;
		}
		dlg->ShowTipMessage("���״����У� �벻Ҫ�ƶ�У԰��", 1);
		if (card.TestCardExists(5))
		{
		//	dlg->ShowTipMessage("δ�ſ�", 5);			
			 ret = RET_WF_ERROR;
			goto L_END;
		}
		drtp->Reset();
		drtp->SetRequestHeader(KS_BANK_TX_CODE, 1);
		drtp->AddField(F_SCLOSE_EMP, "240206");
		drtp->AddField(F_LVOL0, cardinfo.cardid);
		drtp->AddField(F_SEMP_PWD, (LPSTR)(LPCTSTR)password);
		drtp->AddField(F_DAMT0, cardinfo.balance);							  // �뿨ֵ
		drtp->AddField(F_LVOL3, cardinfo.tx_cnt);							  // �ۼƽ��״���
		drtp->AddField(F_DAMT1, (LPSTR)(LPCTSTR)eleValue);					  // ������
		drtp->AddField(F_SNAME, (LPSTR)(LPCSTR)dlg->GetConfig()->m_systemNo); // ϵͳ���
		drtp->AddField(F_SNAME2, (LPSTR)(LPCSTR)mateRoomNo);
		if (drtp->Connect())
		{
			dlg->ShowTipMessage(drtp->GetErrMsg().c_str());
			goto L_END;
		}
		if (drtp->SendRequest(3000))
		{
			dlg->ClearMessageLine();
			dlg->AddMessageLine(drtp->GetErrMsg().c_str());
			dlg->AddMessageLine("��[�˳�]����", dlg->GetMaxLineNo() + 2, -6);
			dlg->DisplayMessage(2);
			goto L_END;
		}
		if (drtp->HasMoreRecord())
		{
			iCount++;
			ST_PACK *pData = drtp->GetNextRecord();
			tipMsg.Format("����д��, �벻Ҫ�ƶ�У԰��...");
			dlg->ShowTipMessage(tipMsg, 0);
			ret = DoTransfer(&card, &cardinfo, pData);
			if (ret == RET_WF_ERROR)
			{
				SetStopKey(true);
				response->Reset();
				response->Connect();
				response->SetRequestHeader(KS_BANK_TX_CODE, 1);
				response->AddField(F_SCLOSE_EMP, "240206");
				int serialNo = atoi(pData->sserial1);
				response->AddField(F_LVOL1, serialNo);
				if (response->SendRequest(3000))
				{
					dlg->ShowTipMessage(response->GetReturnMsg().c_str(),3);
				}
				else
				{
					SetStopKey(false);
					dlg->ClearMessageLine();
					tipMsg = "д��ʧ���뵽�������Ĵ���!";
					dlg->AddMessageLine(tipMsg);
					dlg->AddMessageLine("��[�˳�]����",dlg->GetMaxLineNo()+2,-6);
					dlg->DisplayMessage(1);
				}
				ret = RET_WF_ERROR;
				goto L_END;
			}
			
			if (ret == RET_WF_TIMEOUT)
			{
				goto L_END;
			}
			cardinfo.balance = pData->damt0;
			cardinfo.tx_cnt++;
			dlg->ClearMessageLine();
			SetStopKey(false);						/*�����ӵ�*/
			tipMsg.Format("������Ľ��Ϊ%sԪ", (LPSTR)(LPCSTR)eleValue);
			dlg->AddMessageLine(tipMsg);
			tipMsg.Format("������ĵ���Ϊ%.2lf��",pData->damt1);
			dlg->AddMessageLine(tipMsg);
// 			tipMsg.Format("��[�˳�]����");
// 			dlg->AddMessageLine(tipMsg,dlg->GetMaxLineNo()+1,-6);
			dlg->DisplayMessage(3);				  
			break;
		}
		else
		{
			dlg->ClearMessageLine();
			ret = drtp->GetReturnCode();
			if(ret != ERR_NOT_LOGIN)
				ret = RET_WF_TIMEOUT;
			dlg->AddMessageLine(drtp->GetReturnMsg().c_str());
			dlg->AddMessageLine("��[�˳�]����",dlg->GetMaxLineNo()+2,-6);
			dlg->DisplayMessage(3);
			goto L_END;		
		}
	}
	dlg->ShowTipMessage("�������!", 1);
	ret = RET_WF_SUCCESSED;
L_END:
	if (drtp) {delete drtp;}
	if (response) {delete response;}
	card.CloseCOM();
	SetStopKey(false);
	return ret;
}

int CKSElePacketTransferWorkFLow::ReadCard(CKSCardManager *manager, KS_CARD_INFO *cardinfo)
{
	int ret = manager->ReadCardInfo(cardinfo, CKSCardManager::ciCardId | CKSCardManager::ciBalance, 20);
	if (ret)
	{
		ret = manager->GetErrNo();
		if (ret == ERR_USER_CANCEL)
		{
			return RET_WF_TIMEOUT;
		}
		return RET_WF_ERROR;
	}
	return RET_WF_SUCCESSED;
}

int CKSElePacketTransferWorkFLow::DoTransfer(CKSCardManager *manager,
							KS_CARD_INFO *cardinfo, const ST_PACK *rec)
{
	CKSNLDlg *dlg = (CKSNLDlg*)GetMainWnd();
	int ret;
	int retries = 3;
	while (--retries >= 0)
	{
		// ��⿨
		if (manager->TestCardExists(5))
		{
			continue;
		}
		ret = manager->SetMoney(cardinfo->cardid, (int)rec->damt0 * 100); // ����ֵ
		if (!ret)
		{
			return RET_WF_SUCCESSED;
		}
		if (manager->GetErrNo() == ERR_USER_CANCEL)
		{
			return RET_WF_TIMEOUT;
		}
		dlg->ClearMessageLine();
		dlg->AddMessageLine("д��ʧ�ܣ����ط�У԰��");
		dlg->AddMessageLine("��[�˳�]����",dlg->GetMaxLineNo()+2,-6);
		dlg->DisplayMessage(1);
	}
	return RET_WF_ERROR;
}

//////////////////////////////////////////////////////////////////////////
// ��ѯ���ת����ϸ
//////////////////////////////////////////////////////////////////////////
CKSQueryEleTransferAccoutsWorkFlow::CKSQueryEleTransferAccoutsWorkFlow(CDialog *dlg) : CKSWorkflow(dlg)
{
	m_key = "ElePackAccTrf";
}

CKSQueryEleTransferAccoutsWorkFlow::~CKSQueryEleTransferAccoutsWorkFlow()
{
	//
}

CString CKSQueryEleTransferAccoutsWorkFlow::GetWorkflowID()
{
	return m_key;
}

int CKSQueryEleTransferAccoutsWorkFlow::DoWork()
{
	CDRTPHelper *drtp = NULL;
	CKSNLDlg *dlg = (CKSNLDlg*)GetMainWnd();
	KS_CARD_INFO cardInfo;
	int ret = RET_WF_ERROR;
	int retCount;
	CString passwd;
	CString cardidstr;
	CString moneyValue;
	memset(&cardInfo, 0, sizeof(cardInfo));

	dlg->ClearAllColumns();
	dlg->AddListColumns("��س�ֵ��ˮ��", 120);
	dlg->AddListColumns("�ͻ���", 80);
	dlg->AddListColumns("���׿���", 70);
	dlg->AddListColumns("��������", 90);
	dlg->AddListColumns("����ʱ��", 70);
	dlg->AddListColumns("���", 70);
	dlg->AddListColumns("������ (��)", 80);
	dlg->AddListColumns("�����", 70);
	dlg->ShowTipMessage("���У԰��...", 0);
	CKSCardManager card(dlg->GetConfig(), (LPSTR)(LPCTSTR)dlg->GetConfig()->m_workKey, this);

	if (card.OpenCOM() != 0)
	{
		dlg->ShowTipMessage(card.GetErrMsg());
		return RET_WF_ERROR;
	}
	SetStopKey(false);
	ret = card.ReadCardInfo(&cardInfo, CKSCardManager::ciCardId, 5);
	if (ret)
	{
		dlg->ShowTipMessage(card.GetErrMsg(), 1);
		goto L_END;
	}
	dlg->SetLimtText(1);
	if (dlg->InputPassword("������У԰������", passwd, 10) != 0)
	{
		ret = RET_WF_ERROR;
		goto L_END;
	}
	dlg->ShowTipMessage("���״�����, ��ȴ�", 1);
	cardidstr.Format("%d", cardInfo.cardid);
	drtp = dlg->GetConfig()->CreateDrtpHelper();
	drtp->SetRequestHeader(KS_BANK_TX_CODE, 1);
	drtp->AddField(F_SCLOSE_EMP, "240207");
	drtp->AddField(F_LVOL0, (LPSTR)(LPCTSTR)cardidstr);
	drtp->AddField(F_SEMP_PWD, (LPSTR)(LPCTSTR)passwd);
	if (drtp->Connect())
	{
		dlg->ShowTipMessage(drtp->GetErrMsg().c_str());
		goto L_END;
	}
	if (drtp->SendRequest(3000))
	{
		dlg->ClearMessageLine();
		dlg->AddMessageLine(drtp->GetErrMsg().c_str());
		dlg->AddMessageLine("��[�˳�]����", dlg->GetMaxLineNo() + 2, -6);
		dlg->DisplayMessage(2);
		ret = RET_WF_TIMEOUT;
		goto L_END;
	}
	else if(drtp->GetReturnCode())
	{
		dlg->ShowTipMessage(drtp->GetReturnMsg().c_str());
		goto L_END;
	}
	else
	{
		retCount = 0;
		while (drtp->HasMoreRecord())
		{
			ST_PACK *pData = drtp->GetNextRecord();
			CString cardEleNo;                  // ��س�ֵ��ˮ��
			cardEleNo.Format("%d", pData->lvol0);
			CString userNo;						  // �ͻ���
			userNo.Format("%d", pData->lvol1);
			CString cardNo;					      // ���׿���
			cardNo.Format("%d", pData->lvol2);
			CString txtdate = ParseDateString(pData->sdate0);	// ��������
			CString txtime = ParseTimeString(pData->stime0);    // ����ʱ��
			moneyValue.Format("%0.2f", pData->damt0);
			CString eleVolumn; // ������
			eleVolumn.Format("%0.2f", pData->damt1);
			CString mateRoomNo;
			mateRoomNo.Format("%s", pData->sname);
			dlg->AddToListInfo(cardEleNo, userNo, cardNo, txtdate, txtime, moneyValue, eleVolumn, mateRoomNo);
			retCount++;
			// ֻ��ʾ10������
			if (retCount > 10)
			{
				break;
			}
		}
		if (retCount > 0)
		{
			dlg->ShowListInfo(25);
		}
		else
		{
			dlg->ShowTipMessage("û��ת�ʵļ�¼!");
		}
		ret = RET_WF_SUCCESSED;
	}
L_END:
	if(drtp)
		delete drtp;
	card.CloseCOM();
	SetStopKey(false);
	return ret;
}

//////////////////////////////////////////////////////////////////////////
// ���¿���Ϣ
//////////////////////////////////////////////////////////////////////////
CKModifyCardInfoWorkflow::CKModifyCardInfoWorkflow(CDialog *dlg) : CKSWorkflow(dlg)
{
	m_key = "ModifyCardInfo";
}

CKModifyCardInfoWorkflow::~CKModifyCardInfoWorkflow(void)
{
	//
}

int CKModifyCardInfoWorkflow::DoWork()
{
	CString passwd;
	CString tipMsg("");
	CString tipCardInfoMsg("");
	CDRTPHelper * drtp = NULL;
	CDRTPHelper * response = NULL;
	KS_CARD_INFO cardinfo;
	TPublishCard updateCardInfo;
	//////////////////////////////////////////////////////////////////////////
	CString strDealLineDate = "";
	CString strName = "";
	CString strLibraryNo = "";
	CString strIdentifyNo = "";
	CString strSexNo = "";
	CString strCardNo = "";
	CString strShowCardNo = "";
	CString strCertificateNo = "";
	CString strDepartmentNo = "";
	CString strCardType = "";
	//////////////////////////////////////////////////////////////////////////
	int ret;
//	int iCount = 0;
	int hasMore = 0;
	CKSNLDlg* dlg = (CKSNLDlg*)GetMainWnd();  // Get a active main window of the application
	memset(&cardinfo, 0, sizeof cardinfo);
	memset(&updateCardInfo, 0, sizeof(TPublishCard));
	dlg->ShowTipMessage("���У԰��...",0);
	CKSCardManager card(dlg->GetConfig(),
		(LPSTR)(LPCTSTR)dlg->GetConfig()->m_workKey,this);
	if(card.OpenCOM()!=0)
	{
		dlg->ShowTipMessage(card.GetErrMsg());
		return RET_WF_ERROR;
	}
	
	SetStopKey(false);
	ret = ReadCard(&card, &cardinfo);                    
	if(ret)
	{
		if(RET_WF_ERROR == ret)
		{
			dlg->ShowTipMessage(card.GetErrMsg(), 1);
		}
		goto L_END;                                    // ����ʧ��
	}
	dlg->SetLimtText(1);
	if(dlg->InputPassword("������У԰������",passwd,10) != 0)
	{
		ret = RET_WF_TIMEOUT;
		goto L_END;
	}
	dlg->ShowTipMessage("���״����У��벻Ҫ�ƶ�У԰��",1);
	if(card.TestCardExists(5))
	{			  
		ret = RET_WF_ERROR;
		goto L_END;
	}
	ret = RET_WF_ERROR;
	drtp = dlg->GetConfig()->CreateDrtpHelper();
	response = dlg->GetConfig()->CreateDrtpHelper();
	
	hasMore = 0;
	SetStopKey(true);
	drtp->Reset();
	if(drtp->Connect())
	{
		dlg->ShowTipMessage(drtp->GetErrMsg().c_str());
		goto L_END;
	}
	drtp->SetRequestHeader(KS_BANK_TX_CODE,1);
	drtp->AddField(F_SCLOSE_EMP,"240105");				// ������
	drtp->AddField(F_LVOL0,cardinfo.cardid);			// ���׿���
	drtp->AddField(F_SEMP_PWD,(LPSTR)(LPCTSTR)passwd);	// ����
//	drtp->AddField(F_DAMT0,cardinfo.balance);			//�뿨ֵ
//	drtp->AddField(F_LVOL1,cardinfo.tx_cnt);			//�ۼƽ��״���
	drtp->AddField(F_SNAME,(LPSTR)(LPCTSTR)dlg->GetConfig()->m_systemNo);// ϵͳ���					//�豸�ɣ�
//	drtp->AddField(F_SCUST_NO,"system");				//����Ա
	dlg->ShowTipMessage("���ڴ������ף��벻Ҫ�ƶ�У԰��...",1);
	SetStopKey(false);
	if(drtp->SendRequest(3000))
	{
		dlg->ClearMessageLine();
		dlg->AddMessageLine(drtp->GetErrMsg().c_str());
		dlg->AddMessageLine("��[�˳�]����",dlg->GetMaxLineNo()+2,-6);
		dlg->DisplayMessage(3);
		ret = RET_WF_TIMEOUT;
		goto L_END;
	}
	if(drtp->HasMoreRecord())
	{
	//	iCount++;
		ST_PACK * pData = drtp->GetNextRecord();
	//	dlg->ShowTipMessage("���ڶ������벻Ҫ�ƶ�У԰��",1);
		if(card.TestCardExists(5))
		{			  
			ret = RET_WF_ERROR;
			goto L_END;
		}
		card.ReadCardInformation(cardinfo.cardid, &updateCardInfo);
		////////////////////////////////////////////////////////////////////////////
		strDealLineDate.Format("    ��ֹ����%s", pData->sdate0);
		strName.Format("    ����%s", pData->sname);
		strLibraryNo.Format("%s", pData->sname2);
		strIdentifyNo.Format("%s", pData->scust_auth2);
		if (pData->scust_type[0] == '0')
		{
			strSexNo.Format("��");
		}
		else if (pData->scust_type[0] == '1')
		{
			strSexNo.Format("Ů");
		}
		else
		{
			strSexNo.Format("��");
		}
		strCardNo.Format("    ѧ����%s", pData->scust_limit);
		strShowCardNo.Format("��ʾ���� %s", pData->scust_no);
		strCertificateNo.Format("%s", pData->scust_auth);
		strDepartmentNo.Format("%s", pData->sserial0, 10);
		strCardType.Format("    ������%s", pData->sbankname);
		tipCardInfoMsg.Format("%s\n%s", strName, strCardNo);
		// ��ʾ����Ҫ���µ�������........................
		dlg->AddMessageLineEx(strCardType, dlg->GetMaxLineNo() + 1, 0, dlg->AT_LEFT);
		dlg->AddMessageLineEx(strDealLineDate, dlg->GetMaxLineNo() + 1, 0, dlg->AT_LEFT);
		dlg->AddMessageLineEx(strName, dlg->GetMaxLineNo() + 1, 0, dlg->AT_LEFT);
		dlg->AddMessageLineEx(strCardNo, dlg->GetMaxLineNo() + 1, 0, dlg->AT_LEFT);
		tipMsg.Format("��[ȷ��]��������, ��[�˳�]����");
		dlg->AddMessageLineEx(tipMsg, dlg->GetMaxLineNo() + 3, -6, dlg->AT_LEFT);
		if (dlg->Confirm(10) != IDOK)
		{
			ret = RET_WF_TIMEOUT;
			goto L_END;
		}
		///////////////////////////////////////////////////////////////////////////
		memcpy(updateCardInfo.DeadLineDate, pData->sdate0, 9);			// ��ֹ����
		updateCardInfo.CardRightType = pData->lvol5;					// ������
		memcpy(updateCardInfo.ucName, pData->sname, 8);					// ����
		memcpy(updateCardInfo.ucLibraryNo, pData->sname2, 20);			// ͼ��֤��
		memcpy(updateCardInfo.ucIdentifyNo, pData->scust_auth2, 4);		// ����֤��
		memcpy(updateCardInfo.ucSexNo, pData->scust_type, 1);			// �Ա�
		memcpy(updateCardInfo.ucCardNo, pData->scust_limit, 20);		// ѧ����
		memcpy(updateCardInfo.ShowCardNo, pData->scust_no, 10);			// ��ʾ����
		memcpy(updateCardInfo.ucCertificateNo, pData->scust_auth, 20);	// ֤������
		memcpy(updateCardInfo.ucDepartmentNo, pData->sserial0, 10);		// ���ű��
		////////////////////////////////////////////////////////////////////////////            
		tipMsg.Format("����д�����벻Ҫ�ƶ�У԰��...");
		dlg->ShowTipMessage(    tipMsg,0);
		ret = DoModifyCardInfo(&card,&cardinfo,pData, &updateCardInfo);  
		//////////////////////////////////////////////////////////////////////////
		if (RET_WF_TIMEOUT == ret)
		{
			goto L_END;
		}
		SetStopKey(false);
	}
	else
	{
		dlg->ClearMessageLine();
		ret = drtp->GetReturnCode();
		if(ret != ERR_NOT_LOGIN)
			ret = RET_WF_TIMEOUT;
		dlg->AddMessageLine(drtp->GetReturnMsg().c_str());
		dlg->AddMessageLine("��[�˳�]����",dlg->GetMaxLineNo()+2,-6);
		dlg->DisplayMessage(3);	
		goto L_END;
	}
	dlg->ShowTipMessage("    ������ɣ�",1);
	ret = RET_WF_SUCCESSED;
L_END:
	if(drtp)
		delete drtp;
	if(response)
		delete response;
	card.CloseCOM();
	SetStopKey(false);
	return ret;
}

CString CKModifyCardInfoWorkflow::GetWorkflowID()
{
	return m_key;
}

int CKModifyCardInfoWorkflow::ReadCard(CKSCardManager* manager,KS_CARD_INFO *cardinfo)
{
	int ret = manager->ReadCardInfo(cardinfo,
		CKSCardManager::ciCardId|CKSCardManager::ciBalance,20);
	if(ret)
	{
		ret = manager->GetErrNo();
		if(ERR_USER_CANCEL == ret)
		{
			return RET_WF_TIMEOUT;
		}
		return RET_WF_ERROR;
	}
	return RET_WF_SUCCESSED;
}

int CKModifyCardInfoWorkflow::DoModifyCardInfo(
								  CKSCardManager* manager,
								  KS_CARD_INFO *cardinfo, 
								  const ST_PACK *rec, 
								  TPublishCard *pc
								  )
{
	CKSNLDlg* dlg = (CKSNLDlg*)GetMainWnd();
	int ret;
	int retries = 3;
	while(--retries>=0)
	{
		// ��⿨
		if(manager->TestCardExists(5))
		{
			continue;
		}
		ret = manager->ModifyCardInfo(cardinfo->cardid, pc);
		if(!ret)
		{
			return RET_WF_SUCCESSED;
		}
		if(manager->GetErrNo() == ERR_USER_CANCEL)
		{
			return RET_WF_TIMEOUT;
		}
		dlg->ClearMessageLine();
		dlg->AddMessageLine("д��ʧ�ܣ������·�У԰����");
		dlg->AddMessageLine("��[�˳�]����",dlg->GetMaxLineNo()+2,-6);
		dlg->DisplayMessage(1);
	}
	return RET_WF_ERROR;
}

//////////////////////////////////////////////////////////////////////////
// ��ѯ��д����Ϣ
//////////////////////////////////////////////////////////////////////////
CKQueryMendCardWorkflow::CKQueryMendCardWorkflow(CDialog *dlg) : CKSWorkflow(dlg)
{
	m_key = "QueryMendCard";
}

CKQueryMendCardWorkflow::~CKQueryMendCardWorkflow(void)
{
	//
}

int CKQueryMendCardWorkflow::DoWork()
{
	CString passwd;
	CString tipMsg("");
	CString tipCardInfoMsg("");
	CDRTPHelper * drtp = NULL;
	KS_CARD_INFO cardinfo;
	TPublishCard updateCardInfo;
	int recCount = 0;
	
	CString strTradeNo = "";
	CString strCutid = "";
	CString strName = "";
	CString strSexNo = "";
	CString strStuempNo = "";
	CString strCardid = "";
	CString strDealDate = "";
	CString strBalance = "";
	
	int ret;
	int hasMore = 0;
	CKSNLDlg* dlg = (CKSNLDlg*)GetMainWnd();  // Get a active main window of the application
	memset(&cardinfo, 0, sizeof(cardinfo));
	memset(&updateCardInfo, 0, sizeof(updateCardInfo));

	dlg->ClearAllColumns();
	dlg->AddListColumns("������", 80);
	dlg->AddListColumns("�ͻ���", 80);
	dlg->AddListColumns("����", 80);
	dlg->AddListColumns("�Ա�", 40);
	dlg->AddListColumns("ѧ����", 80);
	dlg->AddListColumns("���׿���", 80);
	dlg->AddListColumns("��Ч��ֹ����", 100);
	dlg->AddListColumns("�������", 80);

	dlg->ShowTipMessage("���У԰��...",0);
	CKSCardManager card(dlg->GetConfig(),
		(LPSTR)(LPCTSTR)dlg->GetConfig()->m_workKey,this);
	if(card.OpenCOM()!=0)
	{
		dlg->ShowTipMessage(card.GetErrMsg());
		return RET_WF_ERROR;
	}
	
	SetStopKey(false);
	ret = ReadCard(&card, &cardinfo);                    
	if(ret)
	{
		if(RET_WF_ERROR == ret)
		{
			dlg->ShowTipMessage(card.GetErrMsg(), 1);
		}
		goto L_END;                                    // ����ʧ��
	}
	
	ret = card.ReadCardInformation(cardinfo.cardid, &updateCardInfo);
	if(ret)
	{
		if(RET_WF_ERROR == ret)
		{
			dlg->ShowTipMessage(card.GetErrMsg(), 1);
		}
		goto L_END;                                    // ����ʧ��
	}

	if(card.TestCardExists(5))
	{			  
		ret = RET_WF_ERROR;
		goto L_END;
	}
	ret = RET_WF_ERROR;
	drtp = dlg->GetConfig()->CreateDrtpHelper();

	hasMore = 0;
	drtp->Reset();
	if(drtp->Connect())
	{
		dlg->ShowTipMessage(drtp->GetErrMsg().c_str());
		goto L_END;
	}
	drtp->SetRequestHeader(KS_BANK_TX_CODE,1);
	drtp->AddField(F_SCLOSE_EMP,"240108");									// ������							
	drtp->AddField(F_LVOL1, cardinfo.cardid);								// ���׿���
	drtp->AddField(F_SCUST_AUTH, (char*)updateCardInfo.ucCardNo);			// ѧ����
	drtp->AddField(F_SNAME, (LPSTR)(LPCTSTR)dlg->GetConfig()->m_systemNo);	// ϵͳ���				
	dlg->ShowTipMessage("���ڴ������ף��벻Ҫ�ƶ�У԰��...",1);

	if(drtp->SendRequest(3000))
	{
		dlg->ClearMessageLine();
		dlg->AddMessageLine(drtp->GetErrMsg().c_str());
		dlg->AddMessageLine("��[�˳�]����",dlg->GetMaxLineNo()+2,-6);
		dlg->DisplayMessage(3);
		ret = RET_WF_TIMEOUT;
		goto L_END;
	}
	else
	{
		while (drtp->HasMoreRecord())
		{
			SetStopKey(false);
			ST_PACK *pData = drtp->GetNextRecord();
			strTradeNo.Format("%d", pData->lvol2);
			strCutid.Format("%d", pData->lvol3);
			strName.Format("%s", pData->sname);
			if (pData->scust_type[0] == '0')
			{
				strSexNo.Format("��");
			}
			else if (pData->scust_type[0] == '1')
			{
				strSexNo.Format("Ů");
			}
			else
			{
				strSexNo.Format("��");
			}
			strStuempNo.Format("%s", pData->scust_auth);
			strCardid.Format("%d", pData->lvol0);
			strDealDate.Format("%s", pData->sdate0);
			strBalance.Format("%.2f", pData->damt1);
			dlg->AddToListInfo(strTradeNo, strCutid, strName, strSexNo, strStuempNo, strCardid, strDealDate, strBalance);	
			recCount++;
			if (recCount > 10)
				break;
		}
		
		if (recCount > 0)
			dlg->ShowListInfo(25);
		else
			dlg->ShowTipMessage("û�в�д����Ϣ��¼");
	}
	ret = RET_WF_SUCCESSED;
L_END:
	if(drtp)
		delete drtp;
	card.CloseCOM();
	SetStopKey(false);
	return ret;
}

CString CKQueryMendCardWorkflow::GetWorkflowID()
{
	return m_key;
}

int CKQueryMendCardWorkflow::ReadCard(CKSCardManager* manager,KS_CARD_INFO *cardinfo)
{
	int ret = manager->ReadCardInfo(cardinfo,
		CKSCardManager::ciCardId|CKSCardManager::ciBalance,20);
	if(ret)
	{
		ret = manager->GetErrNo();
		if(ERR_USER_CANCEL == ret)
		{
			return RET_WF_TIMEOUT;
		}
		return RET_WF_ERROR;
	}
	return RET_WF_SUCCESSED;
}

//////////////////////////////////////////////////////////////////////////
// ��д����Ϣ
//////////////////////////////////////////////////////////////////////////
CKMendCardWorkflow::CKMendCardWorkflow(CDialog *dlg) : CKSWorkflow(dlg)
{
	m_key = "MendCard";
}

CKMendCardWorkflow::~CKMendCardWorkflow(void)
{
	//
}

int CKMendCardWorkflow::DoWork()
{
	char str_physical_no[9] = "";
	CString passwd("");
	CString tipMsg("");
	CString tipCardInfoMsg("");
	CDRTPHelper * drtp = NULL;
	CDRTPHelper * response = NULL;
	KS_CARD_INFO cardinfo;
	int mend_card_function_no = 847119;
	int ret = 0;
	int hasMore = 0;
	double pre_card_money = 0;
	CKSNLDlg* dlg = (CKSNLDlg*)GetMainWnd();  // Get a active main window of the application
	memset(&cardinfo, 0, sizeof(cardinfo));
	dlg->ShowTipMessage("���У԰��...", 0);
	CKSCardManager card(dlg->GetConfig(),
		(LPSTR)(LPCTSTR)dlg->GetConfig()->m_workKey, this);
	if (card.OpenCOM() != 0)
	{
		dlg->ShowTipMessage(card.GetErrMsg());
		return RET_WF_ERROR;
	}
	
	SetStopKey(false);
	ret = ReadCard(&card, &cardinfo);                    
	if(ret)
	{
		if(RET_WF_ERROR == ret)
		{
			dlg->ShowTipMessage(card.GetErrMsg(), 1);
		}
		goto L_END;                                    // ����ʧ��
	}
	
	ret = ReadSerial(cardinfo.phyid);
	if (ret)
	{
		ret = RET_WF_ERROR;
		goto L_END;	
	}

	HexToAscii(cardinfo.phyid, 4, str_physical_no);

	if (card.TestCardExists(5))
	{			  
		ret = RET_WF_ERROR;
		goto L_END;
	}
	
	ret = RET_WF_ERROR;
	drtp = dlg->GetConfig()->CreateDrtpHelper();
	response = dlg->GetConfig()->CreateDrtpHelper();
	
	hasMore = 0;
	SetStopKey(true);
	drtp->Reset();
	if(drtp->Connect())
	{
		dlg->ShowTipMessage(drtp->GetErrMsg().c_str());
		goto L_END;
	}

	drtp->SetRequestHeader(KS_BANK_TX_CODE,1);
	drtp->AddField(F_SCLOSE_EMP,"240109");											// ������
	drtp->AddField(F_SBANK_ACC, str_physical_no);									// ��������
	drtp->AddField(F_LVOL0, cardinfo.cardid);										// ���׿���
	drtp->AddField(F_LVOL8, cardinfo.tx_cnt);										// �ۼƽ��״���
	drtp->AddField(F_DAMT1, cardinfo.balance);										// �뿨ֵ
	drtp->AddField(F_SCUST_LIMIT, "system");										// ����Ա
	drtp->AddField(F_LVOL6, 0);														// ����վID��
	drtp->AddField(F_LVOL7, atoi((LPSTR)(LPCTSTR)dlg->GetConfig()->m_systemNo));	// �豸ϵͳ���				
	pre_card_money = cardinfo.balance;
	
	SetStopKey(false);
	
	if (drtp->SendRequest(3000))
	{
		dlg->ClearMessageLine();
		dlg->AddMessageLine(drtp->GetErrMsg().c_str());
		dlg->AddMessageLine("��[�˳�]����",dlg->GetMaxLineNo()+2,-6);
		dlg->DisplayMessage(3);
		ret = RET_WF_TIMEOUT;
		goto L_END;
	}

	if (drtp->HasMoreRecord())
	{
		ST_PACK * pData = drtp->GetNextRecord();
		tipMsg.Format("����д�����벻Ҫ�ƶ�У԰��...");
		dlg->ShowTipMessage(tipMsg, 0);
		ret = DoMend(&card, &cardinfo, pData);	
		if (RET_WF_ERROR == ret)
		{
			SetStopKey(false);
			dlg->ClearMessageLine();
			tipMsg = "д��ʧ�����ش����򵽹������Ĵ���";
			dlg->AddMessageLine(tipMsg);
			dlg->AddMessageLine("��[�˳�]����",dlg->GetMaxLineNo()+2,-6);
			dlg->DisplayMessage(1);
			goto L_END;
		}
		else if (RET_WF_TIMEOUT == ret)
		{
			goto L_END;
		}
		response->Reset();
		response->Connect();
		response->SetRequestHeader(KS_BANK_TX_CODE,1);
		response->AddField(F_SCLOSE_EMP,"240110");											// ������
		response->AddField(F_LVOL0, cardinfo.cardid);										// ���׿���	
		response->AddField(F_LSERIAL0, mend_card_function_no);
		if (response->SendRequest(3000))
 		{
			SetStopKey(false);
			dlg->ClearMessageLine();
			tipMsg = "ȷ�Ͻ���ʧ���뵽�������Ĵ���";
			dlg->AddMessageLine(tipMsg);
			dlg->AddMessageLine("��[�˳�]����",dlg->GetMaxLineNo()+2,-6);
			dlg->DisplayMessage(1);
			ret = RET_WF_ERROR;
			goto L_END;
		}
		else
		{
			SetStopKey(false);
			cardinfo.balance = pData->damt2;
			cardinfo.tx_cnt++;
			dlg->ClearMessageLine();
			tipMsg.Format("����дǰ���Ľ��Ϊ%.2lfԪ", pre_card_money);
			dlg->AddMessageLine(tipMsg, dlg->GetMaxLineNo() + 2, 0);
			tipMsg.Format("����д�󿨽��Ϊ%.2lfԪ", pData->damt2);
			dlg->AddMessageLine(tipMsg, dlg->GetMaxLineNo() + 2, 0);
			dlg->DisplayMessage(3);
		}		
	}
	else
	{
		dlg->ClearMessageLine();
		ret = drtp->GetReturnCode();
		if(ret != ERR_NOT_LOGIN)
			ret = RET_WF_TIMEOUT;
		dlg->AddMessageLine(drtp->GetReturnMsg().c_str());
		dlg->AddMessageLine("��[�˳�]����", dlg->GetMaxLineNo() + 2, -6);
		dlg->DisplayMessage(3);	
		goto L_END;
	}

	SetStopKey(false);
	dlg->ShowTipMessage("������ɣ�",1);
	ret = RET_WF_SUCCESSED;
L_END:
	if(drtp)
		delete drtp;
	if(response)
		delete response;
	card.CloseCOM();
	SetStopKey(false);
	return ret;
}

CString CKMendCardWorkflow::GetWorkflowID()
{
	return m_key;
}

int CKMendCardWorkflow::ReadCard(CKSCardManager* manager,KS_CARD_INFO *cardinfo)
{
	int ret = manager->ReadCardInfo(cardinfo,
		CKSCardManager::ciCardId|CKSCardManager::ciBalance, 20);
	if(ret)
	{
		ret = manager->GetErrNo();
		if(ERR_USER_CANCEL == ret)
		{
			return RET_WF_TIMEOUT;
		}
		return RET_WF_ERROR;
	}
	return RET_WF_SUCCESSED;
}

int CKMendCardWorkflow::DoMend(CKSCardManager *manager,
							   KS_CARD_INFO *cardinfo, 
							   const ST_PACK *rec
							   )
{
	CKSNLDlg *dlg = (CKSNLDlg*)GetMainWnd();
	int ret;
	int retries = 3;
	while (--retries >= 0)
	{
		// ��⿨
		if (manager->TestCardExists(5))
		{
			continue;
		}
		ret = manager->SetMoney(cardinfo->cardid, (int)rec->damt2 * 100); // ����ֵ
		if (!ret)
		{
			return RET_WF_SUCCESSED;
		}
		if (manager->GetErrNo() == ERR_USER_CANCEL)
		{
			return RET_WF_TIMEOUT;
		}
		dlg->ClearMessageLine();
		dlg->AddMessageLine("д��ʧ�ܣ����ط�У԰��");
		dlg->AddMessageLine("��[�˳�]����",dlg->GetMaxLineNo()+2,-6);
		dlg->DisplayMessage(1);
	}
	return RET_WF_ERROR;
}

//////////////////////////////////////////////////////////////////////////
// ��ѯ�����շ��ʻ���Ϣ
//////////////////////////////////////////////////////////////////////////
CKQueryNetChargeWorkFlow::CKQueryNetChargeWorkFlow(CDialog *dlg) : CKSWorkflow(dlg)
{
	m_key = "QueryNetCharge";
}

CKQueryNetChargeWorkFlow::~CKQueryNetChargeWorkFlow(void)
{
	//
}

int CKQueryNetChargeWorkFlow::DoWork()
{
	CString strTipMsg = "";
	CDRTPHelper * drtp = NULL;
	CKSNLDlg* dlg = (CKSNLDlg*)GetMainWnd();
	KS_CARD_INFO cardinfo;
	TPublishCard updateCardInfo;
	int ret = RET_WF_ERROR;
	int recCount = 0;
	CString passwd = "";
	CString cardidstr = "";
	double dRemainBalance = 0.0;
	double dOweBalance = 0.0;
	memset(&cardinfo, 0, sizeof(cardinfo));
	memset(&updateCardInfo, 0, sizeof(updateCardInfo));
	
	dlg->ShowTipMessage("���У԰��...",0);
	CKSCardManager card(dlg->GetConfig(),
		(LPSTR)(LPCTSTR)dlg->GetConfig()->m_workKey,this);
	
	if (card.OpenCOM() != 0)
	{
		dlg->ShowTipMessage(card.GetErrMsg());
		return RET_WF_ERROR;
	}

	SetStopKey(false);
	ret = card.ReadCardInfo(&cardinfo, CKSCardManager::ciCardId, 5);
	if (ret)
	{
		dlg->ShowTipMessage(card.GetErrMsg(), 1);
		goto L_END;
	}
	
	ret = card.ReadCardInformation(cardinfo.cardid, &updateCardInfo);
	if(ret)
	{
		if(RET_WF_ERROR == ret)
		{
			dlg->ShowTipMessage(card.GetErrMsg(), 1);
		}
		goto L_END;                                    
	}

	dlg->SetLimtText(1);
	if(dlg->InputPassword("������У԰������", passwd, 10) != 0)
	{
		ret = RET_WF_TIMEOUT;
		goto L_END;
	}

/*
	if (card.TestCardExists(5))
	{
		ret = RET_WF_ERROR;
		goto L_END;
	}
*/
	
	dlg->ShowTipMessage("���״�����,��ȴ�...",1);
	cardidstr.Format("%d", cardinfo.cardid);
	drtp = dlg->GetConfig()->CreateDrtpHelper();
	drtp->SetRequestHeader(KS_BANK_TX_CODE,1);
	drtp->AddField(F_SCLOSE_EMP,"240111");					
	drtp->AddField(F_SSERIAL0, (LPSTR)(LPCTSTR)cardidstr);
	drtp->AddField(F_SCUST_AUTH, (char*)updateCardInfo.ucCardNo);			// ѧ����
	drtp->AddField(F_SEMP_PWD, (LPSTR)(LPCTSTR)passwd);		
	drtp->AddField(F_SORDER2, (LPSTR)(LPCTSTR)dlg->GetConfig()->m_systemNo);

	if (drtp->Connect())
	{
		dlg->ShowTipMessage(drtp->GetErrMsg().c_str());
		goto L_END;		
	}
	
	if (drtp->SendRequest(5000))
	{
		dlg->ClearMessageLine();
		dlg->AddMessageLine(drtp->GetErrMsg().c_str(), dlg->GetMaxLineNo() + 3, 0);
		dlg->AddMessageLine("�밴[�˳�]����", dlg->GetMaxLineNo() + 4, 0);
		dlg->DisplayMessage(2);
		ret = RET_WF_TIMEOUT;
		goto L_END;
	}

	if (drtp->HasMoreRecord())
	{
		ST_PACK *pData = drtp->GetNextRecord();	
		SetStopKey(false);
		dlg->ClearMessageLine();
		strTipMsg.Format("���Ļ����ʻ����Ϊ:%0.2lf", pData->damt0);
		dlg->AddMessageLine(strTipMsg, dlg->GetMaxLineNo() + 3, 0);
		dlg->AddMessageLine("�밴[�˳�]����",dlg->GetMaxLineNo() + 4, 0);
		dlg->DisplayMessage(10);
	}
	else
	{
		dlg->ClearMessageLine();
		ret = drtp->GetReturnCode();
		if(ret != ERR_NOT_LOGIN)
			ret = RET_WF_TIMEOUT;
		dlg->AddMessageLine(drtp->GetReturnMsg().c_str(), dlg->GetMaxLineNo() + 3, 0);
		dlg->AddMessageLine("�밴[�˳�]����",dlg->GetMaxLineNo() + 4, 0);
		dlg->DisplayMessage(3);
		goto L_END;		
	}

	dlg->ShowTipMessage("�������!", 0);
	ret = RET_WF_SUCCESSED;	
L_END:
	if(drtp)
		delete drtp;
	card.CloseCOM();
	SetStopKey(false);
	return ret;
}

CString CKQueryNetChargeWorkFlow::GetWorkflowID()
{
	return m_key;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// ���ܱ��: 240112
// ��������: �����շ�
// ��������: �û�ͨ��Ȧ����Ӵ�Ǯ���۷�תǮ�������շ�ϵͳ��, ������������۷�
// ҵ���߼�: 1.	�û��ڽ�����������, ���뿨���.
//           2. ����ͽ�����̨����̨����û���״̬������ʧ�ܷ��أ�������һ��δ���˽���1
//			 3.	��̨�������ĳ���ֵ����ǰ̨��ǰ̨д����д��ʧ�ܱ������������̨������2
//           4.	��̨������������ֵ���ף������������ʧ�ܲ����ˣ���������(��ʱҲҪ����)
//           5.	��̨���ؽ�������棬����չʾ������Ϣ
// ��ع���: 
// ��ע:	 �˴�����һ��д���Ļ�����ʶ(���ڽ���ֻ��һ������, �˹�������ʱ����)
// ��������: ������(sclose_emp), ���׿���(lvol0), ����(semp_pwd)
//			 ѧ����(scust_auth), �뿨ֵ(damt1), ����ת�˷���(damt0), �ն˺�(sname)
// �������: �����շ��ʻ����(damt0)
//////////////////////////////////////////////////////////////////////////////////////////////////

CKNetChargeTransferWorkFlow::CKNetChargeTransferWorkFlow(CDialog *dlg) : CKSWorkflow(dlg)
{
	m_key = "NetCharge";
}

CKNetChargeTransferWorkFlow::~CKNetChargeTransferWorkFlow()
{
	//
}

CString CKNetChargeTransferWorkFlow::GetWorkflowID()
{
	return m_key;
}

int CKNetChargeTransferWorkFlow::DoWork()
{
	CString strPassword = "";
	CString strTipMsg("");
	CString strBalance = "";
	CString strMateRoomNo= "";
	CString cardidstr = "";
	CDRTPHelper *drtp = NULL;
	CDRTPHelper *response = NULL;
	KS_CARD_INFO cardinfo;
	TPublishCard updateCardInfo;
	int ret;
	int nCount = 0;
	int hasMore = 0;
	CKSNLDlg *dlg = (CKSNLDlg*)GetMainWnd();
	memset(&cardinfo, 0, sizeof(cardinfo));
	memset(&updateCardInfo, 0, sizeof(updateCardInfo));

	dlg->ShowTipMessage("���У԰��...",0);
	SetStopKey(false);
	CKSCardManager card(dlg->GetConfig(), (LPSTR)(LPCTSTR)dlg->GetConfig()->m_workKey, this);
	if (card.OpenCOM() != 0)
	{
		dlg->ShowTipMessage(card.GetErrMsg());
		return RET_WF_ERROR;
	}
	ret = ReadCard(&card, &cardinfo);
	if (ret == RET_WF_ERROR)
	{
		dlg->ShowTipMessage(card.GetErrMsg(), 1);
		return RET_WF_ERROR;
	}
	
	// ��ȡѧ����
	ret = card.ReadCardInformation(cardinfo.cardid, &updateCardInfo);
	if(ret)
	{
		if(RET_WF_ERROR == ret)
		{
			dlg->ShowTipMessage(card.GetErrMsg(), 1);
		}
		goto L_END;                                   
	}

	dlg->SetLimtText(1);
	if (dlg->InputPassword("������У԰������", strPassword, 10) != 0)
	{
		ret = RET_WF_TIMEOUT;
		goto L_END;
	}

	/*
	dlg->ShowTipMessage("���״����У� �벻Ҫ�ƶ�У԰��", 1);
	if (card.TestCardExists(5))
	{			
		ret = RET_WF_ERROR;
		goto L_END;
	}
	*/

	dlg->SetLimtText(0);
	if (dlg->InputQuery("������ת�ʽ��", "", strBalance, 10) != 0)
	{
		ret = RET_WF_TIMEOUT;
		goto L_END;
	}

	cardidstr.Format("%d", cardinfo.cardid);
	dlg->ShowTipMessage("���״�����, �벻Ҫ�ƶ�У԰��", 1);
	
	if (card.TestCardExists(5))
	{
		ret = RET_WF_ERROR;
		goto L_END;
	}

	ret = RET_WF_ERROR;
	drtp = dlg->GetConfig()->CreateDrtpHelper();
	response = dlg->GetConfig()->CreateDrtpHelper();
	while (true)
	{
		hasMore = 0;
		SetStopKey(true);
		drtp->Reset();
		if (drtp->Connect())
		{
			dlg->ShowTipMessage(drtp->GetErrMsg().c_str());
			goto L_END;
		}
		// ��̨��֤�������Ҽ�����ӵ��Է����ݿ��Ƿ�����
		drtp->SetRequestHeader(KS_BANK_TX_CODE, 1);
		drtp->AddField(F_SCLOSE_EMP, "240113");
		drtp->AddField(F_SSERIAL0, (LPSTR)(LPCTSTR)cardidstr);
		drtp->AddField(F_SCUST_AUTH, (char*)updateCardInfo.ucCardNo);
		drtp->AddField(F_SEMP_PWD, (LPSTR)(LPCTSTR)strPassword);
		drtp->AddField(F_DAMT1, cardinfo.balance);
		drtp->AddField(F_DAMT0, (LPSTR)(LPCTSTR)strBalance);
		drtp->AddField(F_LVOL3, cardinfo.tx_cnt);
		drtp->AddField(F_SORDER2, (LPSTR)(LPCTSTR)dlg->GetConfig()->m_systemNo);

		SetStopKey(false);
		if (drtp->SendRequest(5000))
		{
			dlg->ClearMessageLine();
//			dlg->AddMessageLine(drtp->GetErrMsg().c_str());
//			dlg->AddMessageLine("��[�˳�]����", dlg->GetMaxLineNo() + 2, -6);
			dlg->AddMessageLine(drtp->GetErrMsg().c_str(), dlg->GetMaxLineNo() + 3, 0);
			dlg->AddMessageLine("�밴[�˳�]����", dlg->GetMaxLineNo() + 4, 0);
			dlg->DisplayMessage(2);
			goto L_END;
		}	
		
		if (drtp->HasMoreRecord())
		{
			ST_PACK *pData = drtp->GetNextRecord();
			strTipMsg.Format("����д��, �벻Ҫ�ƶ�У԰��...");
			dlg->ShowTipMessage(strTipMsg, 0);
			ret = DoTransfer(&card, &cardinfo, pData);
			if (ret == RET_WF_ERROR)											// д��ʧ�ܾͲ�����, ����д��ʧ�ܣ������½���
			{
				SetStopKey(false);
				strTipMsg.Format("д��ʧ��, �����½��н���");
				dlg->AddMessageLine(strTipMsg, dlg->GetMaxLineNo() + 2, 0);
				goto L_END;
			}	

			if (ret == RET_WF_TIMEOUT)
			{
				SetStopKey(false);
				strTipMsg.Format("�ȴ��ſ���ʱ, �����½��н���");
				dlg->AddMessageLine(strTipMsg, dlg->GetMaxLineNo() + 2, 0);
				goto L_END;
			}

			SetStopKey(true);
			response->Reset();
			response->Connect();
			response->SetRequestHeader(KS_BANK_TX_CODE, 1);
			response->AddField(F_SCLOSE_EMP, "240112");						    // ����ת�˽���
			response->AddField(F_SSERIAL0, (LPSTR)(LPCTSTR)cardidstr);
			response->AddField(F_SCUST_AUTH, (char*)updateCardInfo.ucCardNo);	// ѧ����
			response->AddField(F_SEMP_PWD, (LPSTR)(LPCTSTR)strPassword);
			response->AddField(F_DAMT1, cardinfo.balance);
			response->AddField(F_DAMT0, (LPSTR)(LPCTSTR)strBalance);
			response->AddField(F_LVOL3, cardinfo.tx_cnt);
			response->AddField(F_SORDER2, (LPSTR)(LPCTSTR)dlg->GetConfig()->m_systemNo);
			response->AddField(F_SDATE0, "cr010101");
			response->AddField(F_LVOL0, pData->lvol0);							// ��ˮ��

			if (response->SendRequest(5000))
			{
//				dlg->ShowTipMessage(response->GetReturnMsg().c_str(),3);		// �����ǽ��׷���ʧ��	
				dlg->ClearMessageLine();
				dlg->AddMessageLine(drtp->GetErrMsg().c_str(), dlg->GetMaxLineNo() + 3, 0);
				dlg->AddMessageLine("�밴[�˳�]����", dlg->GetMaxLineNo() + 4, 0);
				dlg->DisplayMessage(2);
				goto L_END;
			}

			if (response->HasMoreRecord())
			{
				ST_PACK *pRespData = response->GetNextRecord();
				cardinfo.balance = pRespData->damt0;
				dlg->ClearMessageLine();
				SetStopKey(false);
				if (1001 == pRespData->lvol11)
				{
					strTipMsg.Format("�����շ�ת�ʳ�ʱ, �뵽�����շ����Ĵ���");	// ��ʱ�����Ѿ�����
				}
				else
				{
					strTipMsg.Format("�����շѽ��Ϊ%sԪ", (LPSTR)(LPCTSTR)strBalance);
					dlg->AddMessageLine(strTipMsg, dlg->GetMaxLineNo() + 2, 0);
					strTipMsg.Format("���Ļ����ʻ����Ϊ%0.2lfԪ", cardinfo.balance);
				}
				dlg->AddMessageLine(strTipMsg, dlg->GetMaxLineNo() + 2, 0);
				dlg->AddMessageLine("�밴[�˳�]����",dlg->GetMaxLineNo() + 3, 0);
				dlg->DisplayMessage(10);	
			}
			else
			{
				SetStopKey(false);
				dlg->ClearMessageLine();
				ret = drtp->GetReturnCode();
				if(ret != ERR_NOT_LOGIN)
					ret = RET_WF_TIMEOUT;
//				dlg->AddMessageLine(drtp->GetReturnMsg().c_str());
//				dlg->AddMessageLine("��[�˳�]����",dlg->GetMaxLineNo() + 2, 0);
				dlg->AddMessageLine(drtp->GetReturnMsg().c_str(), dlg->GetMaxLineNo() + 3, 0);
				dlg->AddMessageLine("�밴[�˳�]����",dlg->GetMaxLineNo() + 4, 0);
				dlg->DisplayMessage(3);
				goto L_END;
			}	
			break;
		}
		else
		{
			dlg->ClearMessageLine();
			ret = drtp->GetReturnCode();
			if(ret != ERR_NOT_LOGIN)
				ret = RET_WF_TIMEOUT;
//			dlg->AddMessageLine(drtp->GetReturnMsg().c_str());
//			dlg->AddMessageLine("��[�˳�]����",dlg->GetMaxLineNo() + 2, 0);
			dlg->AddMessageLine(drtp->GetReturnMsg().c_str(), dlg->GetMaxLineNo() + 3, 0);
			dlg->AddMessageLine("�밴[�˳�]����",dlg->GetMaxLineNo() + 4, 0);
			dlg->DisplayMessage(3);
			goto L_END;		
		}
	}

	dlg->ShowTipMessage("�������!", 0);
	ret = RET_WF_SUCCESSED;
L_END:
	if (drtp) {delete drtp;}
	if (response) {delete response;}
	card.CloseCOM();
	SetStopKey(false);
	return ret;
}

int CKNetChargeTransferWorkFlow::ReadCard(CKSCardManager *manager, KS_CARD_INFO *cardinfo)
{
	int ret = manager->ReadCardInfo(cardinfo, CKSCardManager::ciCardId | CKSCardManager::ciBalance, 20);
	if (ret)
	{
		ret = manager->GetErrNo();
		if (ret == ERR_USER_CANCEL)
		{
			return RET_WF_TIMEOUT;
		}
		return RET_WF_ERROR;
	}
	return RET_WF_SUCCESSED;
}

int CKNetChargeTransferWorkFlow::DoTransfer(CKSCardManager *manager,
							KS_CARD_INFO *cardinfo, const ST_PACK *rec)
{
	CKSNLDlg *dlg = (CKSNLDlg*)GetMainWnd();
	int ret;
	int retries = 3;
	while (--retries >= 0)
	{
		// ��⿨
		if (manager->TestCardExists(5))
		{
			continue;
		}
		ret = manager->SetMoney(cardinfo->cardid, (int)rec->damt1 * 100); // ����ֵ
		if (!ret)
		{
			return RET_WF_SUCCESSED;
		}
		if (manager->GetErrNo() == ERR_USER_CANCEL)
		{
			return RET_WF_TIMEOUT;
		}
		dlg->ClearMessageLine();
		dlg->AddMessageLine("д��ʧ�ܣ����ط�У԰��", dlg->GetMaxLineNo() + 3, 0);
//		dlg->AddMessageLine("�밴[�˳�]����",dlg->GetMaxLineNo() + 2,-6);
		dlg->AddMessageLine("�밴[�˳�]����",dlg->GetMaxLineNo() + 4, 0);
		dlg->DisplayMessage(1);
	}
	return RET_WF_ERROR;
}

//////////////////////////////////////////////////////////////////////////
// CKSMobileTransWorkFlow (�ֻ���ֵ)
CKSMobileTransWorkFlow::CKSMobileTransWorkFlow(CDialog *dlg) : CKSWorkflow(dlg)
{
	m_key = _T("MobileTransfer");
}

CKSMobileTransWorkFlow::~CKSMobileTransWorkFlow()
{

}

CString CKSMobileTransWorkFlow::GetWorkflowID()
{
	return m_key;
}

int CKSMobileTransWorkFlow::ReadCard(CKSCardManager* manager,KS_CARD_INFO *cardinfo)
{
	int ret = manager->ReadCardInfo(cardinfo,
		CKSCardManager::ciCardId | CKSCardManager::ciBalance, 20);
	if(ret)
	{
		ret = manager->GetErrNo();
		if(ERR_USER_CANCEL == ret)
		{
			return RET_WF_TIMEOUT;
		}
		return RET_WF_ERROR;
	}
	return RET_WF_SUCCESSED;
}

//////////////////////////////////////////////////////////////////////////
// ���ܱ��: 260001
// ��������: �ֻ���ֵ
// ��������: �û�ͨ��Ȧ���У԰���۷�, ת�˸��ƶ��ֻ���
// ҵ���߼�: 1.	����û���״̬����鿨���
//			 2. ����δ���˽���, ��һ��δ������ˮ
//			 3.	�ӿ��д�Ǯ���۳����, �ɹ���4, ������д��ʧ�ܽ���
//           4.	�����̨����, ��Ǯ����ˮ����, ����СǮ�����
//           5.	СǮ��д��, �ɹ���6, ������д��ʧ�ܽ���
//           6.	Ȧ�����չʾ�ۿ���Ϣ
// ��ع���: ���д��ʧ�ܷ���ת��ʧ�ܽ���, ������240111
// ��������: 
//			 
// �������: 
//			 
//////////////////////////////////////////////////////////////////////////
int CKSMobileTransWorkFlow::DoWork()
{
	CString mobile_num = "";
	CString mobile_num_2 = "";
	CString password = "";
	CString trade_money = "";
	CString tipMsg = "";
	CDRTPHelper *drtp = NULL;
	CDRTPHelper *response = NULL;
	KS_CARD_INFO cardInfo;
	int ret = 0;
	int iCount = 0;
	int trs_serial_no = 0;
	CKSNLDlg *dlg = (CKSNLDlg*)GetMainWnd();
	memset(&cardInfo, 0, sizeof(cardInfo));
	SetStopKey(false);
	dlg->ShowTipMessage("���У԰��...", 0);
	CKSCardManager card(dlg->GetConfig(),
		(LPSTR)(LPCTSTR)dlg->GetConfig()->m_workKey,this);
	if (card.OpenCOM() != 0)
	{
		dlg->ShowTipMessage(card.GetErrMsg());
		return RET_WF_ERROR;
	}
	// ��ȡ����Ϣ
	ret = ReadCard(&card, &cardInfo);
	if (ret)
	{
		if (RET_WF_ERROR == ret)
		{
			dlg->ShowTipMessage(card.GetErrMsg(), 1);					// �޸Ĺ�
		}
		goto L_END;
	}

	dlg->SetLimtText(1);
	if (dlg->InputPassword("������У԰������", password, 10) != 0)
	{
		ret = RET_WF_TIMEOUT;
		goto L_END;
	}
	
	dlg->SetLimtText(2);
	if (dlg->InputQuery("�������ƶ���ֵ�ֻ���", "", mobile_num, 25) != 0)
	{
		ret = RET_WF_TIMEOUT;
		goto L_END;
	}

	dlg->SetLimtText(2);
	if (dlg->InputQuery("���ٴ������ֻ���ȷ��", "", mobile_num_2, 25) != 0)
	{
		ret = RET_WF_TIMEOUT;
		goto L_END;
	}

	if(mobile_num != mobile_num_2)
	{
		dlg->ShowTipMessage("���������ֻ��Ų���, �����²���", 5);
		ret = RET_WF_ERROR;
		goto L_END;
	}

	dlg->SetLimtText(3);
	tipMsg.Format("ת�˽��30 50 100 300(Ԫ)");
	if (dlg->InputQuery(tipMsg, "", trade_money, 10) != 0)
	{
		ret = RET_WF_TIMEOUT;
		goto L_END;
	}
	
	if (trade_money.Compare("30") != 0	&&
		trade_money.Compare("50") != 0	&&
		trade_money.Compare("100") != 0 &&
		trade_money.Compare("300") != 0
		)
	{
		dlg->ShowTipMessage("����ת�˽���, �����²���", 5);
		ret = RET_WF_ERROR;
		goto L_END;		
	}

	dlg->ShowTipMessage("���״�����, �벻Ҫ�ƶ�У԰��", 1);
	if (card.TestCardExists(5))
	{
		ret = RET_WF_ERROR;
		goto L_END;
	}	
	ret = RET_WF_ERROR;
	drtp = dlg->GetConfig()->CreateDrtpHelper();
	response = dlg->GetConfig()->CreateDrtpHelper();
	
	while (true)
	{
		SetStopKey(true);
		drtp->Reset();
		if (drtp->Connect())
		{
			dlg->ShowTipMessage(drtp->GetErrMsg().c_str());
			goto L_END;
		}
		// ����drtp�������
		drtp->SetRequestHeader(KS_TRANS, 1);
//		drtp->AddField(F_SCLOSE_EMP, "240200");
//		drtp->AddField(F_LCERT_CODE, 240200);							// �ֻ���ֵ
		drtp->AddField(F_LCERT_CODE,m_payCode);
		drtp->AddField(F_SNOTE,m_funcText);
		drtp->AddField(F_LVOL0, cardInfo.cardid);
		drtp->AddField(F_LVOL1, cardInfo.tx_cnt);
		drtp->AddField(F_LVOL2, PURSE_NO1);
		drtp->AddField(F_DAMT0, cardInfo.balance);
		drtp->AddField(F_DAMT1, atof((LPSTR)(LPCTSTR)trade_money));	
		drtp->AddField(F_SEMP_PWD, (LPSTR)(LPCTSTR)password);
		drtp->AddField(F_SCUST_NO, "system");
		drtp->AddField(F_SPHONE, (LPSTR)(LPCTSTR)mobile_num);
		drtp->AddField(F_LVOL4, atoi((LPSTR)(LPCTSTR)dlg->GetConfig()->m_systemNo));		
		drtp->AddField(F_LVOL9, TRADE_NODEBT);							// ��847316��							
		SetStopKey(false);
		// ��������
		if (drtp->SendRequest(5000))
		{
			dlg->ClearMessageLine();
			dlg->AddMessageLine(drtp->GetErrMsg().c_str());
			dlg->AddMessageLine("��[�˳�]����", dlg->GetMaxLineNo() + 2, 0);
			dlg->DisplayMessage(3);
			ret = RET_WF_TIMEOUT;
			goto L_END;
		}
		// ��ȡ���ؼ�¼
		if (drtp->HasMoreRecord())
		{
			ST_PACK *pData = drtp->GetNextRecord();
			tipMsg.Format("����д��, �벻Ҫ�ƶ�У԰��...", 1);
			dlg->ShowTipMessage(tipMsg, 0);
			// д��(��Ǯ���м�Ǯ), �����и�����ֵ
			ret = DoTransferValue(&card, &cardInfo, pData);	
			if (RET_WF_ERROR == ret)
			{
				SetStopKey(false);
				dlg->ClearMessageLine();
				tipMsg = "д��ʧ��������ת��";
				dlg->AddMessageLine(tipMsg);
				dlg->AddMessageLine("��[�˳�]����", dlg->GetMaxLineNo() + 2, 0);
				dlg->DisplayMessage(1);
				ret = RET_WF_ERROR;
				goto L_END;
			}
			// ��ʱ
			if (RET_WF_TIMEOUT == ret)
			{
				goto L_END;
			}
			
			trs_serial_no = pData->lvol0;

			// �����Ǯ�����˽���
			drtp->Reset();
			if (drtp->Connect())
			{
				dlg->ShowTipMessage(drtp->GetErrMsg().c_str());
				goto L_END;
			}	
			// ����drtp�������	
			drtp->SetRequestHeader(KS_TRANS, 1);
//			drtp->AddField(F_SCLOSE_EMP, "240202");
			drtp->AddField(F_LCERT_CODE, m_payCode);						// �ƶ���ֵ
			drtp->AddField(F_LVOL0, cardInfo.cardid);
			drtp->AddField(F_LVOL1, cardInfo.tx_cnt);
			drtp->AddField(F_LVOL2, PURSE_NO1);
			drtp->AddField(F_DAMT0, cardInfo.balance);
			drtp->AddField(F_DAMT1, atof((LPSTR)(LPCTSTR)trade_money));	
			drtp->AddField(F_SEMP_PWD, (LPSTR)(LPCTSTR)password);
			drtp->AddField(F_SCUST_NO, "system");
			drtp->AddField(F_SPHONE, (LPSTR)(LPCTSTR)mobile_num);
			drtp->AddField(F_LVOL4, atoi((LPSTR)(LPCTSTR)dlg->GetConfig()->m_systemNo));
			drtp->AddField(F_LVOL6, trs_serial_no);
			drtp->AddField(F_LVOL9, TRADE_DEBT);						// ��847317��
			SetStopKey(false);
			if (drtp->SendRequest(3000))
			{
				dlg->ClearMessageLine();
				dlg->AddMessageLine(drtp->GetErrMsg().c_str());
				dlg->AddMessageLine("��[�˳�]����", dlg->GetMaxLineNo() + 2, 0);
				dlg->DisplayMessage(3);
				ret = RET_WF_TIMEOUT;
				goto L_END;
			}
			
			if (drtp->HasMoreRecord())
			{
				pData = drtp->GetNextRecord();
			}
			else
			{
				dlg->ClearMessageLine();
				ret = drtp->GetReturnCode();
				if (ret != ERR_NOT_LOGIN)
				{
					ret = RET_WF_TIMEOUT;
				}
				dlg->AddMessageLine(drtp->GetReturnMsg().c_str());
				dlg->AddMessageLine("��[�˳�]����", dlg->GetMaxLineNo() + 2, 0);
				dlg->DisplayMessage(3);
				goto L_END;	
			}

			// д���ɹ�
			//cardInfo.balance = pData->damt0;
			SetStopKey(false);
			dlg->ClearMessageLine();
			tipMsg.Format("ת�˽��:%0.2fԪ", atof((LPSTR)(LPCTSTR)trade_money));
			dlg->AddMessageLine(tipMsg, dlg->GetMaxLineNo() + 1, 0);
			tipMsg.Format("�����:%0.2fԪ", cardInfo.balance);
			dlg->AddMessageLine(tipMsg, dlg->GetMaxLineNo() + 1, 0);
			dlg->AddMessageLine("�밴[�˳�]����",dlg->GetMaxLineNo() + 3, 0);
			dlg->DisplayMessage(10);
			dlg->ClearMessageLine();
			break;
		}
		else
		{
			dlg->ClearMessageLine();
			ret = drtp->GetReturnCode();
			if (ret != ERR_NOT_LOGIN)
			{
				ret = RET_WF_TIMEOUT;
			}
			dlg->AddMessageLine(drtp->GetReturnMsg().c_str());
			dlg->AddMessageLine("��[�˳�]����", dlg->GetMaxLineNo() + 2, 0);
			dlg->DisplayMessage(3);
			goto L_END;
		}
	}
	dlg->ShowTipMessage("�������!", 1);
	ret = RET_WF_SUCCESSED;
L_END:
	if(drtp)
		delete drtp;
	if(response)
		delete response;
	card.CloseCOM();
	SetStopKey(false);
	return ret;
}

// ��Ǯ������
int CKSMobileTransWorkFlow::DoTransferValue(CKSCardManager* manager, KS_CARD_INFO *cardinfo, const ST_PACK *rec)
{
	CKSNLDlg *dlg = (CKSNLDlg*)GetMainWnd();
	int ret;
	int retries = 3;
	while (--retries >= 0)
	{
		if (manager->TestCardExists(5))
		{
			continue;
		}
		// д��
		ret = manager->SetMoney(cardinfo->cardid, D2I(rec->damt0 * 100));
		if (!ret)
		{
			cardinfo->balance = rec->damt0;
			return RET_WF_SUCCESSED;
		}
		if (manager->GetErrNo() == ERR_USER_CANCEL)
		{
			return RET_WF_TIMEOUT;
		}
		dlg->ClearMessageLine();
		dlg->AddMessageLine("д��ʧ��, �����·�У԰��!");
		dlg->AddMessageLine("��[�˳�]����", dlg->GetMaxLineNo() + 2, 0);
		dlg->DisplayMessage(1);
	}
	return RET_WF_ERROR;
}

/************************************************************************/
/* ����ˮ��Ǯ��ת��                                                     */
/************************************************************************/
CKSXTWaterPacketTransferWorkFlow::CKSXTWaterPacketTransferWorkFlow(CDialog *dlg) : CKSWorkflow(dlg)
{
	m_key = _T("xtwaterpacketrf");
}

CKSXTWaterPacketTransferWorkFlow::~CKSXTWaterPacketTransferWorkFlow()
{

}

CString CKSXTWaterPacketTransferWorkFlow::GetWorkflowID()
{
	return m_key;
}

int CKSXTWaterPacketTransferWorkFlow::ReadCard(CKSCardManager* manager,KS_CARD_INFO *cardinfo)
{
	int ret = manager->ReadCardInfo(cardinfo,
		CKSCardManager::ciAllInfo, 20);
	if(ret)
	{
		ret = manager->GetErrNo();
		if(ERR_USER_CANCEL == ret)
		{
			return RET_WF_TIMEOUT;
		}
		return RET_WF_ERROR;
	}
	return RET_WF_SUCCESSED;
}
int CKSXTWaterPacketTransferWorkFlow::XTWaterTransfer(CKSCardManager *manager,char cardphyid[9],int main_money,WATER_PACK_INFO3 *water)
{
	int ret = 0;
	KS_CARD_INFO cardInfo;
	CKSNLDlg *dlg = (CKSNLDlg*)GetMainWnd();
	ret = manager->ReadCardInfo(&cardInfo,
		CKSCardManager::ciPhyId, 20);
	if(ret)
	{
		ret = manager->GetErrNo();
		if(ERR_USER_CANCEL == ret)
		{
			return RET_WF_TIMEOUT;
		}
		return RET_WF_ERROR;
	}
	if(strncmp((char*)cardInfo.phyid,cardphyid,8) !=0)
		return ERR_CARD_NOT_CONSIST;

	ret = SMT_TransWaterPacket_XT(main_money,water);
	if(ret)				// д��ʧ�ܣ�����һ��
	{
		dlg->ShowTipMessage("ˮ��ת��ʧ�ܣ������·ſ�",5);

		 ret = manager->ReadCardInfo(&cardInfo,
			CKSCardManager::ciPhyId, 20);
		if(ret)
		{
			ret = manager->GetErrNo();
			if(ERR_USER_CANCEL == ret)
			{
				return RET_WF_TIMEOUT;
			}
			return RET_WF_ERROR;
		}
		if(strncmp((char*)cardInfo.phyid,cardphyid,8) !=0)
			return ERR_CARD_NOT_CONSIST;

		return SMT_TransWaterPacket_XT(main_money,water);
	}
	return ret;
}
/*
ˮ��ת�˲��裺
1.����������ת�˽��
2.��ʾ��Ϣ�����û�ȷ��
3.����̨ȡ���ײο��ţ���̨������
4.��̨�����˳ɹ�д��
5.д���ɹ����ͽ��ײο��ţ���̨��ʽ����
6.д��ʧ�ܷ��ͽ��ײο��ţ���̨��¼�쳣��ˮ
*/
int CKSXTWaterPacketTransferWorkFlow::DoWork()
{
	CString password = "";
	CString trade_money = "";
	CString tipMsg = "";
	CDRTPHelper *drtp = NULL;
	CDRTPHelper *response = NULL;
	KS_CARD_INFO cardInfo;
	int ret = 0;
	int iCount = 0;
	int main_money = 0;
	char refno[15] = "";
	char mac[9] = "";
	WATER_PACK_INFO3 water;
	CKSNLDlg *dlg = (CKSNLDlg*)GetMainWnd();
	memset(&cardInfo, 0, sizeof(cardInfo));
	memset(&water, 0, sizeof(water));
	SetStopKey(false);
	dlg->ShowTipMessage("���У԰��...", 0);
	CKSCardManager card(dlg->GetConfig(),
		(LPSTR)(LPCTSTR)dlg->GetConfig()->m_workKey,this);
	if (card.OpenCOM() != 0)
	{
		dlg->ShowTipMessage(card.GetErrMsg());
		return RET_WF_ERROR;
	}
	// ��ȡ����Ϣ����Ǯ����
	ret = ReadCard(&card, &cardInfo);
	if (ret)
	{
		if (RET_WF_ERROR == ret)
		{
			dlg->ShowTipMessage(card.GetErrMsg(), 1);					// �޸Ĺ�
		}
		goto L_END;
	}
	
	// ��ˮ��Ǯ��
	ret = SMT_ReadWaterPackInfo_XT(&water);
	if (ret)
	{
		dlg->ShowTipMessage("��ȡˮ��Ǯ����Ϣʧ��", 1);
		goto L_END;
	}

	dlg->SetLimtText(1);
	if (dlg->InputPassword("������У԰������", password, 10) != 0)
	{
		ret = RET_WF_TIMEOUT;
		goto L_END;
	}

	dlg->SetLimtText(3);
	tipMsg.Format("ת�˽��5 10 20 (Ԫ)");
	if (dlg->InputQuery(tipMsg, "", trade_money, 10) != 0)
	{
		ret = RET_WF_TIMEOUT;
		goto L_END;
	}

	if (trade_money.Compare("5") != 0	&&
		trade_money.Compare("10") != 0	&&
		trade_money.Compare("20") != 0
		)
	{
		dlg->ShowTipMessage("����ת�˽���, �����²���", 5);
		ret = RET_WF_ERROR;
		goto L_END;		
	}

	dlg->ShowTipMessage("���״�����, �벻Ҫ�ƶ�У԰��",1);
	if (card.TestCardExists(5))
	{
		ret = RET_WF_ERROR;
		goto L_END;
	}	

	/*
	//////////////////��ʾ��Ϣ���ȴ��û�ȷ��
	tipMsg.Format("�ͻ��� %d",cardInfo.custid);
	dlg->AddMessageLine(tipMsg);
	tipMsg.Format("��Ƭ��� %.2f",cardInfo.balance);
	dlg->AddMessageLine(tipMsg);
	tipMsg.Format("ˮ��Ǯ����� %.2f",water.balance/100.0);
	dlg->AddMessageLine(tipMsg);
	tipMsg.Format("ת�˽�� %.2f",atof((LPSTR)(LPCTSTR)trade_money));
	dlg->AddMessageLine(tipMsg);
	tipMsg.Format("��[ȷ��]��һ������[�˳�]����");
	dlg->AddMessageLine(tipMsg,dlg->GetMaxLineNo()+2,-6);

	if(dlg->Confirm(10) != IDOK)
	{
		ret = RET_WF_TIMEOUT;
		goto L_END;
	}


	dlg->ShowTipMessage("���״�����, �벻Ҫ�ƶ�У԰��",0);
	if (card.TestCardExists(5))
	{
		ret = RET_WF_ERROR;
		goto L_END;
	}	
	*/
	ret = RET_WF_ERROR;
	drtp = dlg->GetConfig()->CreateDrtpHelper();
	response = dlg->GetConfig()->CreateDrtpHelper();
	while (true)
	{
		SetStopKey(true);
		drtp->Reset();
		if (drtp->Connect())
		{
			dlg->ShowTipMessage(drtp->GetErrMsg().c_str());
			goto L_END;
		}
		// ����drtp�������
		drtp->SetRequestHeader(KS_WATERACC, 1);
		drtp->AddField(F_LBANK_ACC_TYPE,1);					// ������
		drtp->AddField(F_LWITHDRAW_FLAG,atoi((LPSTR)(LPCTSTR)dlg->GetConfig()->m_systemNo));				// �ն˺�
		drtp->AddField(F_LCERT_CODE, m_payCode);															// ����ˮ��ת��		
		drtp->AddField(F_SNOTE,m_funcText);
		drtp->AddField(F_LVOL0, cardInfo.cardid);
		drtp->AddField(F_LVOL6, cardInfo.tx_cnt);				// ���Ѵ���
		drtp->AddField(F_LVOL7, cardInfo.balance*100);
		drtp->AddField(F_LVOL1, atof((LPSTR)(LPCTSTR)trade_money)*100);	
		drtp->AddField(F_SSTATION1, (char *)cardInfo.phyid);														
		drtp->AddField(F_LSAFE_LEVEL,water.balance);			//ˮ��Ǯ�����		
		drtp->AddField(F_STX_PWD, (LPSTR)(LPCTSTR)password);
		SetStopKey(false);
		// ��������
		if (drtp->SendRequest(3000))
		{
			dlg->ClearMessageLine();
			dlg->AddMessageLine(drtp->GetErrMsg().c_str());
			dlg->AddMessageLine("��[�˳�]����", dlg->GetMaxLineNo() + 2, 0);
			dlg->DisplayMessage(3);
			ret = RET_WF_TIMEOUT;
			goto L_END;
		}
		// ��ȡ���ؼ�¼
		if (drtp->HasMoreRecord())
		{
			ST_PACK *pData = drtp->GetNextRecord();

			memcpy(refno,pData->sphone3,14);
			memcpy(mac,pData->saddr,8);
			water.price1 = pData->lvol10;			// ˮ����1
			water.price2 = pData->lvol11;			// ˮ����2
			water.price3 = pData->lvol12;			// ˮ����3
			water.balance = pData->lsafe_level2;
			main_money = pData->lvol8;				// ����ֵ
			ret = XTWaterTransfer(&card,(char *)cardInfo.phyid,main_money,&water);
			if(!ret)						// д���ɹ�
			{
				// ��̨���ݽ��ײο�������
				SetStopKey(true);
				drtp->Reset();
				if (drtp->Connect())
				{
					dlg->ShowTipMessage(drtp->GetErrMsg().c_str());
					goto L_END;
				}	
				// ����drtp�������	
				drtp->SetRequestHeader(KS_WATERACC, 1);
				drtp->AddField(F_LWITHDRAW_FLAG,atoi((LPSTR)(LPCTSTR)dlg->GetConfig()->m_systemNo));				// �ն˺�
				drtp->AddField(F_LBANK_ACC_TYPE,2);					// ��ʽ����
				drtp->AddField(F_SPHONE3,refno);
				drtp->AddField(F_SADDR,mac);

				SetStopKey(false);
				if (drtp->SendRequest(5000))
				{
					dlg->ClearMessageLine();
					dlg->AddMessageLine(drtp->GetErrMsg().c_str());
					dlg->AddMessageLine("��[�˳�]����", dlg->GetMaxLineNo() + 2, 0);
					dlg->DisplayMessage(3);
					ret = RET_WF_TIMEOUT;
					goto L_END;
				}
			}
			else				// д��ʧ��
			{
				SetStopKey(true);
				response->Reset();
				response->Connect();
				drtp->SetRequestHeader(KS_WATERACC, 1);
				drtp->AddField(F_LWITHDRAW_FLAG,atoi((LPSTR)(LPCTSTR)dlg->GetConfig()->m_systemNo));				// �ն˺�
				drtp->AddField(F_LBANK_ACC_TYPE,3);					//д��ʧ��
				drtp->AddField(F_SPHONE3,refno);
				drtp->AddField(F_SADDR,mac);
				if (response->SendRequest(3000))
				{
					dlg->ShowTipMessage(response->GetReturnMsg().c_str(), 3);
				}
				else
				{
					SetStopKey(false);
					dlg->ClearMessageLine();
					tipMsg = "д��ʧ���뵽�������Ĵ���!";
					dlg->AddMessageLine(tipMsg);
					dlg->AddMessageLine("��[�˳�]����", dlg->GetMaxLineNo() + 2, 0);
					dlg->DisplayMessage(1);
				}
				ret = RET_WF_ERROR;
				goto L_END;
			}

			SetStopKey(false);
			dlg->ClearMessageLine();
			tipMsg.Format("ת�˽��:%0.2fԪ", atof((LPSTR)(LPCTSTR)trade_money));
			dlg->AddMessageLine(tipMsg, dlg->GetMaxLineNo() + 1, 0);
			tipMsg.Format("�����:%0.2fԪ", main_money/100.0);
			dlg->AddMessageLine(tipMsg, dlg->GetMaxLineNo() + 1, 0);
			tipMsg.Format("ˮ��Ǯ�����%0.2fԪ",water.balance/100.0);
			dlg->AddMessageLine(tipMsg, dlg->GetMaxLineNo() + 1, 0);
			dlg->AddMessageLine("�밴[�˳�]����",dlg->GetMaxLineNo() + 3, 0);
			dlg->DisplayMessage(10);
			break;
		}
		else
		{
			dlg->ClearMessageLine();
			ret = drtp->GetReturnCode();
			if (ret != ERR_NOT_LOGIN)
			{
				ret = RET_WF_TIMEOUT;
			}
			dlg->AddMessageLine(drtp->GetReturnMsg().c_str());
			dlg->AddMessageLine("��[�˳�]����", dlg->GetMaxLineNo() + 2, 0);
			dlg->DisplayMessage(3);
			goto L_END;
		}
	}

	dlg->ShowTipMessage("�������!", 1);
	ret = RET_WF_SUCCESSED;
L_END:
	if(drtp)
		delete drtp;
	if(response)
		delete response;
	card.CloseCOM();
	SetStopKey(false);
	return ret;
}

//////////////////////////////////////////////////////////////////////////
// ��ѯ����ˮ�����
//////////////////////////////////////////////////////////////////////////
CKSXTQueryWaterVolumnWorkFlow::CKSXTQueryWaterVolumnWorkFlow(CDialog *dlg) : CKSWorkflow(dlg)
{
	m_key = _T("qryxtwatervol");
}

CKSXTQueryWaterVolumnWorkFlow::~CKSXTQueryWaterVolumnWorkFlow()
{

}

CString CKSXTQueryWaterVolumnWorkFlow::GetWorkflowID()
{
	return m_key;
}

int CKSXTQueryWaterVolumnWorkFlow::DoWork()
{
	CString water_remain = "";
	CDRTPHelper *drtp = NULL;
	CKSNLDlg *dlg = (CKSNLDlg*)GetMainWnd();
	WATER_PACK_INFO3 water;
	memset(&water, 0, sizeof(water));
	int ret = RET_WF_ERROR;

	CKSCardManager card(dlg->GetConfig(),
		(LPSTR)(LPCTSTR)dlg->GetConfig()->m_workKey,this);

	if (card.OpenCOM() != 0)
	{
		dlg->ShowTipMessage(card.GetErrMsg());
		return RET_WF_ERROR; 
	}

	dlg->ShowTipMessage("���У԰��...", 0);
	if (card.TestCardExists(5))
	{
		ret = RET_WF_ERROR;
		goto L_END;
	}	

	SetStopKey(false);

	ret = SMT_ReadWaterPackInfo_XT(&water);
	if (ret)
	{
		dlg->ShowTipMessage("��ȡˮ��Ǯ����Ϣʧ��", 1);
		goto L_END;
	}

	water_remain.Format("ˮ��Ǯ�����:%0.2lfԪ", water.balance/100.0);
	dlg->AddMessageLine(water_remain);
	dlg->AddMessageLine("��[�˳�]����", dlg->GetMaxLineNo() + 3, 0);
	dlg->DisplayMessage(3);
	
	dlg->ShowTipMessage("�������!", 0);
	ret = RET_WF_SUCCESSED;

L_END:
	if (drtp)
		delete drtp;
	card.CloseCOM();
	SetStopKey(false);
	return ret;
}


//////////////////////////////////////////////////////////////////////////
//Ǯ����ת������ת����				
//////////////////////////////////////////////////////////////////////////
CKSXT2JDTransferWorkFlow::CKSXT2JDTransferWorkFlow(CDialog *dlg) : CKSWorkflow(dlg)
{
	m_key = _T("xt2jdwatertrf");
}

CKSXT2JDTransferWorkFlow::~CKSXT2JDTransferWorkFlow()
{

}

CString CKSXT2JDTransferWorkFlow::GetWorkflowID()
{
	return m_key;
}

int CKSXT2JDTransferWorkFlow::DoWork()
{
	CString tipMsg="";
	CString password="";
	CDRTPHelper *drtp = NULL;
	CKSNLDlg *dlg = (CKSNLDlg*)GetMainWnd();
	WATER_PACK_INFO3 water;
	KS_CARD_INFO cardinfo;
	memset(&cardinfo,0,sizeof cardinfo);
	memset(&water, 0, sizeof(water));
	int ret = RET_WF_ERROR;

	CKSCardManager card(dlg->GetConfig(),
		(LPSTR)(LPCTSTR)dlg->GetConfig()->m_workKey,this);

	if (card.OpenCOM() != 0)
	{
		dlg->ShowTipMessage(card.GetErrMsg());
		return RET_WF_ERROR; 
	}

	dlg->ShowTipMessage("���У԰��...", 0);
	if (card.TestCardExists(5))
	{
		ret = RET_WF_ERROR;
		goto L_END;
	}	

	SetStopKey(false);

	ret = card.ReadCardInfo(&cardinfo,
		CKSCardManager::ciCardId | CKSCardManager::ciPhyId , 20);
	if(ret)
	{
		ret = card.GetErrNo();
		if(ERR_USER_CANCEL == ret)
		{
			return RET_WF_TIMEOUT;
		}
		return RET_WF_ERROR;
	}

	// ��ˮ��Ǯ��
	ret = SMT_ReadWaterPackInfo_XT(&water);
	if (ret)
	{
		dlg->ShowTipMessage("��ȡˮ��Ǯ����Ϣʧ��", 1);
		goto L_END;
	}

	dlg->SetLimtText(1);
	if (dlg->InputPassword("������У԰������", password, 10) != 0)
	{
		ret = RET_WF_TIMEOUT;
		goto L_END;
	}

	dlg->ShowTipMessage("���״�����, �벻Ҫ�ƶ�У԰��", 0);
	if (card.TestCardExists(5))
	{
		ret = RET_WF_ERROR;
		goto L_END;
	}	

	//////////////////��ʾ��Ϣ���ȴ��û�ȷ��
	tipMsg.Format("�ͻ��� %d",cardinfo.custid);
	dlg->AddMessageLine(tipMsg);
	tipMsg.Format("ˮ��Ǯ����� %.2f",water.balance/100.0);
	dlg->AddMessageLine(tipMsg);

	tipMsg.Format("��[ȷ��]��һ������[�˳�]����");
	dlg->AddMessageLine(tipMsg,dlg->GetMaxLineNo()+2,-6);

	if(dlg->Confirm(10) != IDOK)
	{
		ret = RET_WF_TIMEOUT;
		goto L_END;
	}

	dlg->ShowTipMessage("���״�����, �벻Ҫ�ƶ�У԰��",0);
	if (card.TestCardExists(5))
	{
		ret = RET_WF_ERROR;
		goto L_END;
	}	

	drtp = dlg->GetConfig()->CreateDrtpHelper();

	SetStopKey(true);
	drtp->Reset();
	if (drtp->Connect())
	{
		dlg->ShowTipMessage(drtp->GetErrMsg().c_str());
		goto L_END;
	}
	// ����drtp�������
	drtp->SetRequestHeader(KS_WATERTRANS, 1);
	drtp->AddField(F_LBANK_ACC_TYPE,2);					// 2-����ת����
	drtp->AddField(F_LWITHDRAW_FLAG,atoi((LPSTR)(LPCTSTR)dlg->GetConfig()->m_systemNo));				// �ն˺�
	drtp->AddField(F_LVOL0, cardinfo.cardid);
	drtp->AddField(F_SSTATION1, (char *)cardinfo.phyid);	
	drtp->AddField(F_LSAFE_LEVEL,water.balance);			//ˮ��Ǯ�����		
	drtp->AddField(F_STX_PWD, (LPSTR)(LPCTSTR)password);
	SetStopKey(false);
	// ��������
	if (drtp->SendRequest(3000))
	{
		dlg->ClearMessageLine();
		dlg->AddMessageLine(drtp->GetErrMsg().c_str());
		dlg->AddMessageLine("��[�˳�]����", dlg->GetMaxLineNo() + 2, 0);
		dlg->DisplayMessage(3);
		ret = RET_WF_TIMEOUT;
		goto L_END;
	}
	// ��ȡ���ؼ�¼
	ret = drtp->GetReturnCode();
	if (!ret)
	{
		ret = XT2JD_Card();
		if (ret)
		{
			dlg->ShowTipMessage("Ǯ����תʧ��", 1);
			goto L_END;
		}

		tipMsg.Format("Ǯ����ת�ɹ�");
		dlg->AddMessageLine(tipMsg);
		dlg->AddMessageLine("��[�˳�]����", dlg->GetMaxLineNo() + 3, 0);
		dlg->DisplayMessage(3);
	}
	else
	{
		dlg->ClearMessageLine();
		if (ret != ERR_NOT_LOGIN)
		{
			ret = RET_WF_TIMEOUT;
		}
		dlg->AddMessageLine(drtp->GetReturnMsg().c_str());
		dlg->AddMessageLine("��[�˳�]����", dlg->GetMaxLineNo() + 2, 0);
		dlg->DisplayMessage(3);
		goto L_END;
	}

	dlg->ShowTipMessage("�������!", 1);
	ret = RET_WF_SUCCESSED;

L_END:
	if (drtp)
		delete drtp;
	card.CloseCOM();
	SetStopKey(false);
	return ret;
}


/************************************************************************/
/* ����ˮ��Ǯ��ת��                                                     */
/************************************************************************/
CKSJDWaterPacketTransferWorkFlow::CKSJDWaterPacketTransferWorkFlow(CDialog *dlg) : CKSWorkflow(dlg)
{
	m_key = _T("jdwaterpacketrf");
}

CKSJDWaterPacketTransferWorkFlow::~CKSJDWaterPacketTransferWorkFlow()
{

}

CString CKSJDWaterPacketTransferWorkFlow::GetWorkflowID()
{
	return m_key;
}

int CKSJDWaterPacketTransferWorkFlow::ReadCard(CKSCardManager* manager,KS_CARD_INFO *cardinfo)
{
	int ret = manager->ReadCardInfo(cardinfo,
		CKSCardManager::ciAllInfo, 20);
	if(ret)
	{
		ret = manager->GetErrNo();
		if(ERR_USER_CANCEL == ret)
		{
			return RET_WF_TIMEOUT;
		}
		return RET_WF_ERROR;
	}
	return RET_WF_SUCCESSED;
}
int CKSJDWaterPacketTransferWorkFlow::JDWaterTransfer(CKSCardManager *manager,char cardphyid[9],int main_money,WATER_PACK_INFO3 *water)
{
	int ret = 0;
	KS_CARD_INFO cardInfo;
	CKSNLDlg *dlg = (CKSNLDlg*)GetMainWnd();
	ret = manager->ReadCardInfo(&cardInfo,
		CKSCardManager::ciPhyId, 20);
	if(ret)
	{
		ret = manager->GetErrNo();
		if(ERR_USER_CANCEL == ret)
		{
			return RET_WF_TIMEOUT;
		}
		return RET_WF_ERROR;
	}
	if(strncmp((char*)cardInfo.phyid,cardphyid,8) !=0)
		return ERR_CARD_NOT_CONSIST;

	ret = SMT_TransWaterPacket(main_money,water);
	if(ret)				// д��ʧ�ܣ�����һ��
	{
		dlg->ShowTipMessage("ˮ��ת��ʧ�ܣ������·ſ�");
		
		int ret = manager->ReadCardInfo(&cardInfo,
			CKSCardManager::ciPhyId, 20);
		if(ret)
		{
			ret = manager->GetErrNo();
			if(ERR_USER_CANCEL == ret)
			{
				return RET_WF_TIMEOUT;
			}
			return RET_WF_ERROR;
		}
		if(strncmp((char*)cardInfo.phyid,cardphyid,8) !=0)
			return ERR_CARD_NOT_CONSIST;
		
		return SMT_TransWaterPacket(main_money,water);
	}
	return ret;
}
/*
ˮ��ת�˲��裺
1.����������ת�˽��
2.��ʾ��Ϣ�����û�ȷ��
3.����̨ȡ���ײο��ţ���̨������
4.��̨�����˳ɹ�д��
5.д���ɹ����ͽ��ײο��ţ���̨��ʽ����
6.д��ʧ�ܷ��ͽ��ײο��ţ���̨��¼�쳣��ˮ
*/
int CKSJDWaterPacketTransferWorkFlow::DoWork()
{
	CString password = "";
	CString trade_money = "";
	CString tipMsg = "";
	CDRTPHelper *drtp = NULL;
	CDRTPHelper *response = NULL;
	KS_CARD_INFO cardInfo;
	int ret = 0;
	int iCount = 0;
	int main_money = 0;
	char refno[15] = "";
	char mac[9] = "";
	WATER_PACK_INFO3 water;
	CKSNLDlg *dlg = (CKSNLDlg*)GetMainWnd();
	memset(&cardInfo, 0, sizeof(cardInfo));
	memset(&water, 0, sizeof(water));
	SetStopKey(false);
	dlg->ShowTipMessage("���У԰��...", 0);
	CKSCardManager card(dlg->GetConfig(),
		(LPSTR)(LPCTSTR)dlg->GetConfig()->m_workKey,this);
	if (card.OpenCOM() != 0)
	{
		dlg->ShowTipMessage(card.GetErrMsg());
		return RET_WF_ERROR;
	}
	// ��ȡ����Ϣ����Ǯ����
	ret = ReadCard(&card, &cardInfo);
	if (ret)
	{
		if (RET_WF_ERROR == ret)
		{
			dlg->ShowTipMessage(card.GetErrMsg(), 1);					// �޸Ĺ�
		}
		goto L_END;
	}

	// ��ˮ��Ǯ��
	ret = SMT_ReadWaterPackInfo2(&water);
	if (ret)
	{
		dlg->ShowTipMessage("��ȡˮ��Ǯ����Ϣʧ��", 1);
		goto L_END;
	}

	dlg->SetLimtText(1);
	if (dlg->InputPassword("������У԰������", password, 10) != 0)
	{
		ret = RET_WF_TIMEOUT;
		goto L_END;
	}

	dlg->SetLimtText(3);
	tipMsg.Format("ת�˽��5 10 20 (Ԫ)");
	if (dlg->InputQuery(tipMsg, "", trade_money, 10) != 0)
	{
		ret = RET_WF_TIMEOUT;
		goto L_END;
	}

	if (trade_money.Compare("5") != 0	&&
		trade_money.Compare("10") != 0	&&
		trade_money.Compare("20") != 0
		)
	{
		dlg->ShowTipMessage("����ת�˽���, �����²���", 5);
		ret = RET_WF_ERROR;
		goto L_END;		
	}

	dlg->ShowTipMessage("���״�����, �벻Ҫ�ƶ�У԰��", 1);
	if (card.TestCardExists(5))
	{
		ret = RET_WF_ERROR;
		goto L_END;
	}	
	/*
	//////////////////��ʾ��Ϣ���ȴ��û�ȷ��
	tipMsg.Format("�ͻ��� %d",cardInfo.custid);
	dlg->AddMessageLine(tipMsg);
	tipMsg.Format("��Ƭ��� %.2f",cardInfo.balance);
	dlg->AddMessageLine(tipMsg);
	tipMsg.Format("ˮ��Ǯ����� %.2f",water.balance/100.0);
	dlg->AddMessageLine(tipMsg);
	tipMsg.Format("ת�˽�� %.2f",atof((LPSTR)(LPCTSTR)trade_money));
	dlg->AddMessageLine(tipMsg);
	tipMsg.Format("��[ȷ��]��һ������[�˳�]����");
	dlg->AddMessageLine(tipMsg,dlg->GetMaxLineNo()+2,-6);
	
	if(dlg->Confirm(10) != IDOK)
	{
		ret = RET_WF_TIMEOUT;
		goto L_END;
	}

	dlg->ShowTipMessage("���״�����, �벻Ҫ�ƶ�У԰��",0);
	if (card.TestCardExists(5))
	{
		ret = RET_WF_ERROR;
		goto L_END;
	}	
	*/
	ret = RET_WF_ERROR;
	drtp = dlg->GetConfig()->CreateDrtpHelper();
	response = dlg->GetConfig()->CreateDrtpHelper();
	while (true)
	{
		SetStopKey(true);
		drtp->Reset();
		if (drtp->Connect())
		{
			dlg->ShowTipMessage(drtp->GetErrMsg().c_str());
			goto L_END;
		}
		// ����drtp�������
		drtp->SetRequestHeader(KS_WATERACC, 1);
		drtp->AddField(F_LBANK_ACC_TYPE,1);					// ������
		drtp->AddField(F_LWITHDRAW_FLAG,atoi((LPSTR)(LPCTSTR)dlg->GetConfig()->m_systemNo));				// �ն˺�
		drtp->AddField(F_LCERT_CODE, m_payCode);															// ����ˮ��ת��		
		drtp->AddField(F_SNOTE,m_funcText);
		drtp->AddField(F_LVOL0, cardInfo.cardid);
		drtp->AddField(F_LVOL6, cardInfo.tx_cnt);				// ���Ѵ���
		drtp->AddField(F_LVOL7, cardInfo.balance*100);
		drtp->AddField(F_LVOL1, atof((LPSTR)(LPCTSTR)trade_money)*100);	
		drtp->AddField(F_SSTATION1, (char *)cardInfo.phyid);	
		drtp->AddField(F_LSAFE_LEVEL,water.balance);			//ˮ��Ǯ�����		
		drtp->AddField(F_STX_PWD, (LPSTR)(LPCTSTR)password);
		SetStopKey(false);
		// ��������
		if (drtp->SendRequest(3000))
		{
			dlg->ClearMessageLine();
			dlg->AddMessageLine(drtp->GetErrMsg().c_str());
			dlg->AddMessageLine("��[�˳�]����", dlg->GetMaxLineNo() + 2, 0);
			dlg->DisplayMessage(3);
			ret = RET_WF_TIMEOUT;
			goto L_END;
		}
		// ��ȡ���ؼ�¼
		if (drtp->HasMoreRecord())
		{
			ST_PACK *pData = drtp->GetNextRecord();

			memcpy(refno,pData->sphone3,14);
			memcpy(mac,pData->saddr,8);
			water.price1 = pData->lvol10;			// ˮ����1
			water.price2 = pData->lvol11;			// ˮ����2
			water.price3 = pData->lvol12;			// ˮ����3
			water.balance = pData->lsafe_level2;
			main_money = pData->lvol8;				// ����ֵ
			ret = JDWaterTransfer(&card,(char *)cardInfo.phyid,main_money,&water);
			if(!ret)						// д���ɹ�
			{
				// ��̨���ݽ��ײο�������
				SetStopKey(true);
				drtp->Reset();
				if (drtp->Connect())
				{
					dlg->ShowTipMessage(drtp->GetErrMsg().c_str());
					goto L_END;
				}	
				// ����drtp�������	
				drtp->SetRequestHeader(KS_WATERACC, 1);
				drtp->AddField(F_LWITHDRAW_FLAG,atoi((LPSTR)(LPCTSTR)dlg->GetConfig()->m_systemNo));				// �ն˺�
				drtp->AddField(F_LBANK_ACC_TYPE,2);					// ��ʽ����
				drtp->AddField(F_SPHONE3,refno);
				drtp->AddField(F_SADDR,mac);
				
				SetStopKey(false);
				if (drtp->SendRequest(5000))
				{
					dlg->ClearMessageLine();
					dlg->AddMessageLine(drtp->GetErrMsg().c_str());
					dlg->AddMessageLine("��[�˳�]����", dlg->GetMaxLineNo() + 2, 0);
					dlg->DisplayMessage(3);
					ret = RET_WF_TIMEOUT;
					goto L_END;
				}
			}
			else				// д��ʧ��
			{
				SetStopKey(true);
				response->Reset();
				response->Connect();
				drtp->SetRequestHeader(KS_WATERACC, 1);
				drtp->AddField(F_LWITHDRAW_FLAG,atoi((LPSTR)(LPCTSTR)dlg->GetConfig()->m_systemNo));				// �ն˺�
				drtp->AddField(F_LBANK_ACC_TYPE,3);					//д��ʧ��
				drtp->AddField(F_SPHONE3,refno);
				drtp->AddField(F_SADDR,mac);
				if (response->SendRequest(3000))
				{
					dlg->ShowTipMessage(response->GetReturnMsg().c_str(), 3);
				}
				else
				{
					SetStopKey(false);
					dlg->ClearMessageLine();
					tipMsg = "д��ʧ���뵽�������Ĵ���!";
					dlg->AddMessageLine(tipMsg);
					dlg->AddMessageLine("��[�˳�]����", dlg->GetMaxLineNo() + 2, 0);
					dlg->DisplayMessage(1);
				}
				ret = RET_WF_ERROR;
				goto L_END;
			}

			SetStopKey(false);
			dlg->ClearMessageLine();
			tipMsg.Format("ת�˽��:%0.2fԪ", atof((LPSTR)(LPCTSTR)trade_money));
			dlg->AddMessageLine(tipMsg, dlg->GetMaxLineNo() + 1, 0);
			tipMsg.Format("�����:%0.2fԪ", main_money/100.0);
			dlg->AddMessageLine(tipMsg, dlg->GetMaxLineNo() + 1, 0);
			tipMsg.Format("ˮ��Ǯ�����%0.2fԪ",water.balance/100.0);
			dlg->AddMessageLine(tipMsg, dlg->GetMaxLineNo() + 1, 0);
			dlg->AddMessageLine("�밴[�˳�]����",dlg->GetMaxLineNo() + 3, 0);
			dlg->DisplayMessage(10);
			break;
		}
		else
		{
			dlg->ClearMessageLine();
			ret = drtp->GetReturnCode();
			if (ret != ERR_NOT_LOGIN)
			{
				ret = RET_WF_TIMEOUT;
			}
			dlg->AddMessageLine(drtp->GetReturnMsg().c_str());
			dlg->AddMessageLine("��[�˳�]����", dlg->GetMaxLineNo() + 2, 0);
			dlg->DisplayMessage(3);
			goto L_END;
		}
	}

	dlg->ShowTipMessage("�������!", 1);
	ret = RET_WF_SUCCESSED;
L_END:
	if(drtp)
		delete drtp;
	if(response)
		delete response;
	card.CloseCOM();
	SetStopKey(false);
	return ret;
}

//////////////////////////////////////////////////////////////////////////
// ��ѯ����ˮ�����
//////////////////////////////////////////////////////////////////////////
CKSJDQueryWaterVolumnWorkFlow::CKSJDQueryWaterVolumnWorkFlow(CDialog *dlg) : CKSWorkflow(dlg)
{
	m_key = _T("qryjdwatervol");
}

CKSJDQueryWaterVolumnWorkFlow::~CKSJDQueryWaterVolumnWorkFlow()
{

}

CString CKSJDQueryWaterVolumnWorkFlow::GetWorkflowID()
{
	return m_key;
}

int CKSJDQueryWaterVolumnWorkFlow::DoWork()
{
	CString water_remain = "";
	CDRTPHelper *drtp = NULL;
	CKSNLDlg *dlg = (CKSNLDlg*)GetMainWnd();
	WATER_PACK_INFO3 water;
	memset(&water, 0, sizeof(water));
	int ret = RET_WF_ERROR;

	CKSCardManager card(dlg->GetConfig(),
		(LPSTR)(LPCTSTR)dlg->GetConfig()->m_workKey,this);

	if (card.OpenCOM() != 0)
	{
		dlg->ShowTipMessage(card.GetErrMsg());
		return RET_WF_ERROR; 
	}

	dlg->ShowTipMessage("���У԰��...", 0);
	if (card.TestCardExists(5))
	{
		ret = RET_WF_ERROR;
		goto L_END;
	}	

	SetStopKey(false);

	ret = SMT_ReadWaterPackInfo2(&water);
	if (ret)
	{
		dlg->ShowTipMessage("��ȡˮ��Ǯ����Ϣʧ��", 1);
		goto L_END;
	}

	water_remain.Format("ˮ��Ǯ�����:%0.2lfԪ", water.balance/100.0);
	dlg->AddMessageLine(water_remain);
	dlg->AddMessageLine("��[�˳�]����", dlg->GetMaxLineNo() + 3, 0);
	dlg->DisplayMessage(3);

	dlg->ShowTipMessage("�������!", 0);
	ret = RET_WF_SUCCESSED;

L_END:
	if (drtp)
		delete drtp;
	card.CloseCOM();
	SetStopKey(false);
	return ret;
}


//////////////////////////////////////////////////////////////////////////
//Ǯ����ת������ת����				
//////////////////////////////////////////////////////////////////////////
CKSJD2XTTransferWorkFlow::CKSJD2XTTransferWorkFlow(CDialog *dlg) : CKSWorkflow(dlg)
{
	m_key = _T("jd2xtwatertrf");
}

CKSJD2XTTransferWorkFlow::~CKSJD2XTTransferWorkFlow()
{

}

CString CKSJD2XTTransferWorkFlow::GetWorkflowID()
{
	return m_key;
}

int CKSJD2XTTransferWorkFlow::DoWork()
{
	CString tipMsg="";
	CString password="";
	CDRTPHelper *drtp = NULL;
	CKSNLDlg *dlg = (CKSNLDlg*)GetMainWnd();
	WATER_PACK_INFO3 water;
	KS_CARD_INFO cardinfo;
	memset(&cardinfo,0,sizeof cardinfo);
	memset(&water, 0, sizeof(water));
	int ret = RET_WF_ERROR;

	CKSCardManager card(dlg->GetConfig(),
		(LPSTR)(LPCTSTR)dlg->GetConfig()->m_workKey,this);

	if (card.OpenCOM() != 0)
	{
		dlg->ShowTipMessage(card.GetErrMsg());
		return RET_WF_ERROR; 
	}

	dlg->ShowTipMessage("���У԰��...", 0);
	if (card.TestCardExists(5))
	{
		ret = RET_WF_ERROR;
		goto L_END;
	}	

	SetStopKey(false);

	ret = card.ReadCardInfo(&cardinfo,
		CKSCardManager::ciCardId | CKSCardManager::ciPhyId , 20);
	if(ret)
	{
		ret = card.GetErrNo();
		if(ERR_USER_CANCEL == ret)
		{
			return RET_WF_TIMEOUT;
		}
		return RET_WF_ERROR;
	}

	// ��ˮ��Ǯ��
	ret = SMT_ReadWaterPackInfo2(&water);
	if (ret)
	{
		dlg->ShowTipMessage("��ȡˮ��Ǯ����Ϣʧ��", 1);
		goto L_END;
	}

	dlg->SetLimtText(1);
	if (dlg->InputPassword("������У԰������", password, 10) != 0)
	{
		ret = RET_WF_TIMEOUT;
		goto L_END;
	}

	dlg->ShowTipMessage("���״�����, �벻Ҫ�ƶ�У԰��", 1);
	if (card.TestCardExists(5))
	{
		ret = RET_WF_ERROR;
		goto L_END;
	}	

	//////////////////��ʾ��Ϣ���ȴ��û�ȷ��
	tipMsg.Format("�ͻ��� %d",cardinfo.custid);
	dlg->AddMessageLine(tipMsg);
	tipMsg.Format("ˮ��Ǯ����� %.2f",water.balance/100.0);
	dlg->AddMessageLine(tipMsg);
	
	tipMsg.Format("��[ȷ��]��һ������[�˳�]����");
	dlg->AddMessageLine(tipMsg,dlg->GetMaxLineNo()+2,-6);

	if(dlg->Confirm(10) != IDOK)
	{
		ret = RET_WF_TIMEOUT;
		goto L_END;
	}

	dlg->ShowTipMessage("���״�����, �벻Ҫ�ƶ�У԰��",0);
	if (card.TestCardExists(5))
	{
		ret = RET_WF_ERROR;
		goto L_END;
	}	
	
	drtp = dlg->GetConfig()->CreateDrtpHelper();
	
	SetStopKey(true);
	drtp->Reset();
	if (drtp->Connect())
	{
		dlg->ShowTipMessage(drtp->GetErrMsg().c_str());
		goto L_END;
	}
	// ����drtp�������
	drtp->SetRequestHeader(KS_WATERTRANS, 1);
	drtp->AddField(F_LBANK_ACC_TYPE,1);					// 1-����ת����
	drtp->AddField(F_LWITHDRAW_FLAG,atoi((LPSTR)(LPCTSTR)dlg->GetConfig()->m_systemNo));				// �ն˺�
	drtp->AddField(F_LVOL0, cardinfo.cardid);
	drtp->AddField(F_SSTATION1, (char *)cardinfo.phyid);	
	drtp->AddField(F_LSAFE_LEVEL,water.balance);			//ˮ��Ǯ�����		
	drtp->AddField(F_STX_PWD, (LPSTR)(LPCTSTR)password);
	SetStopKey(false);
	// ��������
	if (drtp->SendRequest(3000))
	{
		dlg->ClearMessageLine();
		dlg->AddMessageLine(drtp->GetErrMsg().c_str());
		dlg->AddMessageLine("��[�˳�]����", dlg->GetMaxLineNo() + 2, 0);
		dlg->DisplayMessage(3);
		ret = RET_WF_TIMEOUT;
		goto L_END;
	}
	// ��ȡ���ؼ�¼
	ret = drtp->GetReturnCode();
	if (!ret)
	{
		ret = JD2XT_Card();
		if (ret)
		{
			dlg->ShowTipMessage("Ǯ����תʧ��", 1);
			goto L_END;
		}

		tipMsg.Format("Ǯ����ת�ɹ�");
		dlg->AddMessageLine(tipMsg);
		dlg->AddMessageLine("��[�˳�]����", dlg->GetMaxLineNo() + 3, 0);
		dlg->DisplayMessage(3);
	}
	else
	{
		dlg->ClearMessageLine();
		if (ret != ERR_NOT_LOGIN)
		{
			ret = RET_WF_TIMEOUT;
		}
		dlg->AddMessageLine(drtp->GetReturnMsg().c_str());
		dlg->AddMessageLine("��[�˳�]����", dlg->GetMaxLineNo() + 2, 0);
		dlg->DisplayMessage(3);
		goto L_END;
	}

	dlg->ShowTipMessage("�������!", 1);
	ret = RET_WF_SUCCESSED;

L_END:
	if (drtp)
		delete drtp;
	card.CloseCOM();
	SetStopKey(false);
	return ret;
}


//////////////////////////////////////////////////////////////////////////
// ���ݴ�ѧ�����Ѳ�ѯ
//////////////////////////////////////////////////////////////////////////
CKGDQueryNetChargeWorkFlow::CKGDQueryNetChargeWorkFlow(CDialog *dlg) : CKSWorkflow(dlg)
{
	m_key = "GDQueryNetCharge";
}

CKGDQueryNetChargeWorkFlow::~CKGDQueryNetChargeWorkFlow(void)
{
	//
}

int CKGDQueryNetChargeWorkFlow::DoWork()
{
	CString strTipMsg = "";
	CDRTPHelper * drtp = NULL;
	CKSNLDlg* dlg = (CKSNLDlg*)GetMainWnd();
	KS_CARD_INFO cardinfo;
	int ret = RET_WF_ERROR;
	CString passwd = "";
	memset(&cardinfo, 0, sizeof(cardinfo));

	dlg->ShowTipMessage("���У԰��...",0);
	CKSCardManager card(dlg->GetConfig(),
		(LPSTR)(LPCTSTR)dlg->GetConfig()->m_workKey,this);

	if (card.OpenCOM() != 0)
	{
		dlg->ShowTipMessage(card.GetErrMsg());
		return RET_WF_ERROR;
	}

	SetStopKey(false);
	ret = card.ReadCardInfo(&cardinfo, CKSCardManager::ciCardId, 5);
	if (ret)
	{
		dlg->ShowTipMessage(card.GetErrMsg(), 1);
		goto L_END;
	}

	dlg->SetLimtText(1);
	if(dlg->InputPassword("������У԰������", passwd, 10) != 0)
	{
		ret = RET_WF_TIMEOUT;
		goto L_END;
	}

	/*
	if (card.TestCardExists(5))
	{
	ret = RET_WF_ERROR;
	goto L_END;
	}
	*/

	dlg->ShowTipMessage("���״�����,��ȴ�...",1);
	drtp = dlg->GetConfig()->CreateDrtpHelper();
	SetStopKey(true);
	drtp->SetRequestHeader(KS_WATERACC,1);
	drtp->AddField(F_LBANK_ACC_TYPE,0);							// ��ѯ�������˻�					
	drtp->AddField(F_LCERT_CODE,m_payCode);							// ���Ѳ�ѯ
	drtp->AddField(F_SNOTE,m_funcText);
	drtp->AddField(F_STX_PWD, (LPSTR)(LPCTSTR)passwd);		
	drtp->AddField(F_LWITHDRAW_FLAG,atoi((LPSTR)(LPCTSTR)dlg->GetConfig()->m_systemNo));
	drtp->AddField(F_LVOL0,cardinfo.cardid);

	if (drtp->Connect())
	{
		dlg->ShowTipMessage(drtp->GetErrMsg().c_str());
		goto L_END;		
	}
	SetStopKey(false);
	if (drtp->SendRequest(5000))
	{
		dlg->ClearMessageLine();
		dlg->AddMessageLine(drtp->GetErrMsg().c_str(), dlg->GetMaxLineNo() + 3, 0);
		dlg->AddMessageLine("�밴[�˳�]����", dlg->GetMaxLineNo() + 4, 0);
		dlg->DisplayMessage(2);
		ret = RET_WF_TIMEOUT;
		goto L_END;
	}

	if (drtp->HasMoreRecord())
	{
		ST_PACK *pData = drtp->GetNextRecord();	
		SetStopKey(false);
		dlg->ClearMessageLine();
		strTipMsg.Format("�������Ϊ:%0.2lf", pData->lvol1/100.0);
		dlg->AddMessageLine(strTipMsg, dlg->GetMaxLineNo() + 3, 0);
		dlg->AddMessageLine("�밴[�˳�]����",dlg->GetMaxLineNo() + 4, 0);
		dlg->DisplayMessage(10);
	}
	else
	{
		dlg->ClearMessageLine();
		ret = drtp->GetReturnCode();
		if(ret != ERR_NOT_LOGIN)
			ret = RET_WF_TIMEOUT;
		dlg->AddMessageLine(drtp->GetReturnMsg().c_str(), dlg->GetMaxLineNo() + 3, 0);
		dlg->AddMessageLine("�밴[�˳�]����",dlg->GetMaxLineNo() + 4, 0);
		dlg->DisplayMessage(3);
		goto L_END;		
	}

	dlg->ShowTipMessage("�������!", 0);
	ret = RET_WF_SUCCESSED;	
L_END:
	if(drtp)
		delete drtp;
	card.CloseCOM();
	SetStopKey(false);
	return ret;
}

CString CKGDQueryNetChargeWorkFlow::GetWorkflowID()
{
	return m_key;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
//���ݴ�ѧ�������շ�
//////////////////////////////////////////////////////////////////////////////////////////////////

CKGDNetChargeTransferWorkFlow::CKGDNetChargeTransferWorkFlow(CDialog *dlg) : CKSWorkflow(dlg)
{
	m_key = "GDNetChargeTransfer";
}

CKGDNetChargeTransferWorkFlow::~CKGDNetChargeTransferWorkFlow()
{
	//
}

CString CKGDNetChargeTransferWorkFlow::GetWorkflowID()
{
	return m_key;
}

int CKGDNetChargeTransferWorkFlow::DoWork()
{
	CString strPassword = "";
	CString strTipMsg("");
	CString trade_money = "";
	CString strMateRoomNo= "";
	CDRTPHelper *drtp = NULL;
	CDRTPHelper *response = NULL;
	KS_CARD_INFO cardinfo;
	char refno[15]="";
	char mac[9]="";
	int main_money = 0;
	int ret;
	int nCount = 0;
	int hasMore = 0;
	CKSNLDlg *dlg = (CKSNLDlg*)GetMainWnd();
	memset(&cardinfo, 0, sizeof(cardinfo));
	
	dlg->ShowTipMessage("���У԰��...",0);
	SetStopKey(false);
	CKSCardManager card(dlg->GetConfig(), (LPSTR)(LPCTSTR)dlg->GetConfig()->m_workKey, this);
	if (card.OpenCOM() != 0)
	{
		dlg->ShowTipMessage(card.GetErrMsg());
		return RET_WF_ERROR;
	}
	ret = ReadCard(&card, &cardinfo);
	if (ret == RET_WF_ERROR)
	{
		dlg->ShowTipMessage(card.GetErrMsg(), 1);
		return RET_WF_ERROR;
	}

	dlg->SetLimtText(1);
	if (dlg->InputPassword("������У԰������", strPassword, 10) != 0)
	{
		ret = RET_WF_TIMEOUT;
		goto L_END;
	}

	
	dlg->SetLimtText(3);
	strTipMsg.Format("������ת�˽��,��λ(Ԫ)");
	if (dlg->InputQuery(strTipMsg, "", trade_money, 10) != 0)
	{
		ret = RET_WF_TIMEOUT;
		goto L_END;
	}
	/*
	if (trade_money.Compare("30") != 0	&&
		trade_money.Compare("60") != 0	&&
		trade_money.Compare("150") != 0 &&
		trade_money.Compare("300") != 0 
		)
	{
		dlg->ShowTipMessage("����ת�˽���, �����²���", 5);
		ret = RET_WF_ERROR;
		goto L_END;		
	}
	*/
	dlg->ShowTipMessage("���״�����, �벻Ҫ�ƶ�У԰��", 1);

	if (card.TestCardExists(5))
	{
		ret = RET_WF_ERROR;
		goto L_END;
	}

	/*
	//////////////////��ʾ��Ϣ���ȴ��û�ȷ��
	strTipMsg.Format("�ͻ��� %d",cardinfo.custid);
	dlg->AddMessageLine(strTipMsg);
	strTipMsg.Format("��Ƭ��� %.2f",cardinfo.balance);
	dlg->AddMessageLine(strTipMsg);
	strTipMsg.Format("ת�˽�� %.2f",atof((LPSTR)(LPCTSTR)trade_money));
	dlg->AddMessageLine(strTipMsg);
	strTipMsg.Format("��[ȷ��]��һ������[�˳�]����");
	dlg->AddMessageLine(strTipMsg,dlg->GetMaxLineNo()+2,-6);
	if(dlg->Confirm(10) != IDOK)
	{
		ret = RET_WF_TIMEOUT;
		goto L_END;
	}
	
	dlg->ShowTipMessage("���״�����, �벻Ҫ�ƶ�У԰��",0);
	if (card.TestCardExists(5))
	{
		ret = RET_WF_ERROR;
		goto L_END;
	}	
	*/
	ret = RET_WF_ERROR;
	drtp = dlg->GetConfig()->CreateDrtpHelper();
	response = dlg->GetConfig()->CreateDrtpHelper();
	while (true)
	{
		hasMore = 0;
		SetStopKey(true);
		drtp->Reset();
		if (drtp->Connect())
		{
			dlg->ShowTipMessage(drtp->GetErrMsg().c_str());
			goto L_END;
		}
		// ��̨��֤�������Ҽ�����ӵ��Է����ݿ��Ƿ�����
		drtp->SetRequestHeader(KS_WATERACC, 1);
		drtp->AddField(F_LBANK_ACC_TYPE,1);					// ������
		drtp->AddField(F_LWITHDRAW_FLAG,atoi((LPSTR)(LPCTSTR)dlg->GetConfig()->m_systemNo));				// �ն˺�
		drtp->AddField(F_LCERT_CODE, m_payCode);															// 2061 ��������շ�
		drtp->AddField(F_SNOTE,m_funcText);						// ����˵��
		drtp->AddField(F_LVOL0, cardinfo.cardid);
		drtp->AddField(F_LVOL6, cardinfo.tx_cnt);				// ���Ѵ���
		drtp->AddField(F_LVOL7, cardinfo.balance*100);
		drtp->AddField(F_LVOL1, atof((LPSTR)(LPCTSTR)trade_money)*100);	
		drtp->AddField(F_SSTATION1, (char *)cardinfo.phyid);	
		drtp->AddField(F_STX_PWD, (LPSTR)(LPCTSTR)strPassword);

		SetStopKey(false);
		if (drtp->SendRequest(5000))
		{
			dlg->ClearMessageLine();
			dlg->AddMessageLine(drtp->GetErrMsg().c_str(), dlg->GetMaxLineNo() + 3, 0);
			dlg->AddMessageLine("�밴[�˳�]����", dlg->GetMaxLineNo() + 4, 0);
			dlg->DisplayMessage(2);
			goto L_END;
		}	
		if (drtp->HasMoreRecord())
		{
			ST_PACK *pData = drtp->GetNextRecord();
			memcpy(refno,pData->sphone3,14);
			memcpy(mac,pData->saddr,8);
			main_money = pData->lvol8;				// �����
			ret = DoNetTransfer(&card,cardinfo.cardid,main_money);
			if(!ret)						// д���ɹ�
			{
				// ��̨���ݽ��ײο�������
				SetStopKey(true);
				drtp->Reset();
				if (drtp->Connect())
				{
					dlg->ShowTipMessage(drtp->GetErrMsg().c_str());
					goto L_END;
				}	
				// ����drtp�������	
				drtp->SetRequestHeader(KS_WATERACC, 1);
				drtp->AddField(F_LWITHDRAW_FLAG,atoi((LPSTR)(LPCTSTR)dlg->GetConfig()->m_systemNo));				// �ն˺�
				//drtp->AddField(F_LCERT_CODE, 2061);										// ˮ��ת������
				drtp->AddField(F_LBANK_ACC_TYPE,2);					// ��ʽ����
				drtp->AddField(F_SPHONE3,refno);
				drtp->AddField(F_SADDR,mac);
				SetStopKey(false);
				if (drtp->SendRequest(5000))
				{
					dlg->ClearMessageLine();
					dlg->AddMessageLine(drtp->GetErrMsg().c_str());
					dlg->AddMessageLine("��[�˳�]����", dlg->GetMaxLineNo() + 2, 0);
					dlg->DisplayMessage(3);
					ret = RET_WF_TIMEOUT;
					goto L_END;
				}
			}
			else				// д��ʧ��
			{
				SetStopKey(true);
				response->Reset();
				response->Connect();
				drtp->SetRequestHeader(KS_WATERACC, 1);
				drtp->AddField(F_LWITHDRAW_FLAG,atoi((LPSTR)(LPCTSTR)dlg->GetConfig()->m_systemNo));				// �ն˺�
				drtp->AddField(F_LBANK_ACC_TYPE,3);					//д��ʧ��
				drtp->AddField(F_SPHONE3,refno);
				drtp->AddField(F_SADDR,mac);
				if (response->SendRequest(3000))
				{
					dlg->ShowTipMessage(response->GetReturnMsg().c_str(), 3);
				}
				else
				{
					SetStopKey(false);
					dlg->ClearMessageLine();
					strTipMsg = "д��ʧ���뵽�������Ĵ���!";
					dlg->AddMessageLine(strTipMsg);
					dlg->AddMessageLine("��[�˳�]����", dlg->GetMaxLineNo() + 2, 0);
					dlg->DisplayMessage(1);
				}
				ret = RET_WF_ERROR;
				goto L_END;
			}

			SetStopKey(false);
			dlg->ClearMessageLine();
			strTipMsg.Format("ת�˽��:%0.2fԪ", atof((LPSTR)(LPCTSTR)trade_money));
			dlg->AddMessageLine(strTipMsg, dlg->GetMaxLineNo() + 1, 0);
			strTipMsg.Format("�����:%0.2fԪ", main_money/100.0);
			dlg->AddMessageLine(strTipMsg, dlg->GetMaxLineNo() + 1, 0);
			dlg->AddMessageLine("�밴[�˳�]����",dlg->GetMaxLineNo() + 3, 0);
			dlg->DisplayMessage(10);
			break;
		}
		else
		{
			dlg->ClearMessageLine();
			ret = drtp->GetReturnCode();
			if (ret != ERR_NOT_LOGIN)
			{
				ret = RET_WF_TIMEOUT;
			}
			dlg->AddMessageLine(drtp->GetReturnMsg().c_str());
			dlg->AddMessageLine("��[�˳�]����", dlg->GetMaxLineNo() + 2, 0);
			dlg->DisplayMessage(3);
			goto L_END;
		}
	}

	dlg->ShowTipMessage("�������!", 0);
	ret = RET_WF_SUCCESSED;
L_END:
	if (drtp) {delete drtp;}
	if (response) {delete response;}
	card.CloseCOM();
	SetStopKey(false);
	return ret;
}

int CKGDNetChargeTransferWorkFlow::ReadCard(CKSCardManager *manager, KS_CARD_INFO *cardinfo)
{
	int ret = manager->ReadCardInfo(cardinfo, CKSCardManager::ciAllInfo, 20);
	if (ret)
	{
		ret = manager->GetErrNo();
		if (ret == ERR_USER_CANCEL)
		{
			return RET_WF_TIMEOUT;
		}
		return RET_WF_ERROR;
	}
	return RET_WF_SUCCESSED;
}

int CKGDNetChargeTransferWorkFlow::DoNetTransfer(CKSCardManager *manager,int card_id,int main_money)
{
	int ret = 0;
	KS_CARD_INFO cardInfo;
	CKSNLDlg *dlg = (CKSNLDlg*)GetMainWnd();
	ret = manager->ReadCardInfo(&cardInfo,
		CKSCardManager::ciCardId, 20);
	if(ret)
	{
		ret = manager->GetErrNo();
		if(ERR_USER_CANCEL == ret)
		{
			return RET_WF_TIMEOUT;
		}
		return RET_WF_ERROR;
	}
	if(card_id != cardInfo.cardid)
		return ERR_CARD_NOT_CONSIST;

	ret = manager->SetMoney(card_id, main_money); // ����ֵ
	if(ret)				// д��ʧ�ܣ�����һ��
	{
		dlg->ShowTipMessage("����ת��ʧ�ܣ������·ſ�");

		ret = manager->ReadCardInfo(&cardInfo,
			CKSCardManager::ciCardId, 20);
		if(ret)
		{
			ret = manager->GetErrNo();
			if(ERR_USER_CANCEL == ret)
			{
				return RET_WF_TIMEOUT;
			}
			return RET_WF_ERROR;
		}
		if(card_id != cardInfo.cardid)
			return ERR_CARD_NOT_CONSIST;

		return manager->SetMoney(card_id, main_money);
	}
	return ret;
}

//////////////////////////////////////////////////////////////////////////
// CKSTelecomTransWorkFlow (���ų�ֵ)
CKSTelecomTransWorkFlow::CKSTelecomTransWorkFlow(CDialog *dlg) : CKSWorkflow(dlg)
{
	m_key = _T("TelecomTransfer");
}

CKSTelecomTransWorkFlow::~CKSTelecomTransWorkFlow()
{

}

CString CKSTelecomTransWorkFlow::GetWorkflowID()
{
	return m_key;
}

int CKSTelecomTransWorkFlow::ReadCard(CKSCardManager* manager,KS_CARD_INFO *cardinfo)
{
	int ret = manager->ReadCardInfo(cardinfo,
		CKSCardManager::ciCardId | CKSCardManager::ciBalance, 20);
	if(ret)
	{
		ret = manager->GetErrNo();
		if(ERR_USER_CANCEL == ret)
		{
			return RET_WF_TIMEOUT;
		}
		return RET_WF_ERROR;
	}
	return RET_WF_SUCCESSED;
}

//////////////////////////////////////////////////////////////////////////
// ���ܱ��: 260001
// ��������: ���ų�ֵ
// ��������: �û�ͨ��Ȧ���У԰���۷�, ת�˸��ƶ��ֻ���
// ҵ���߼�: 1.	����û���״̬����鿨���
//			 2. ����δ���˽���, ��һ��δ������ˮ
//			 3.	�ӿ��д�Ǯ���۳����, �ɹ���4, ������д��ʧ�ܽ���
//           4.	�����̨����, ��Ǯ����ˮ����, ����СǮ�����
//           5.	СǮ��д��, �ɹ���6, ������д��ʧ�ܽ���
//           6.	Ȧ�����չʾ�ۿ���Ϣ
// ��ع���: ���д��ʧ�ܷ���ת��ʧ�ܽ���, ������240111
// ��������: 
//			 
// �������: 
//			 
//////////////////////////////////////////////////////////////////////////
int CKSTelecomTransWorkFlow::DoWork()
{
	CString mobile_num = "";
	CString mobile_num_2 = "";
	CString password = "";
	CString trade_mode = "";		// ��ֵ��ʽ
	int input_mode = 0;
	CString trade_money = "";
	CString tipMsg = "";
	CDRTPHelper *drtp = NULL;
	CDRTPHelper *response = NULL;
	KS_CARD_INFO cardInfo;
	int ret = 0;
	int iCount = 0;
	int trs_serial_no = 0;
	CKSNLDlg *dlg = (CKSNLDlg*)GetMainWnd();
	memset(&cardInfo, 0, sizeof(cardInfo));
	SetStopKey(false);
	dlg->ShowTipMessage("���У԰��...", 0);
	CKSCardManager card(dlg->GetConfig(),
		(LPSTR)(LPCTSTR)dlg->GetConfig()->m_workKey,this);
	if (card.OpenCOM() != 0)
	{
		dlg->ShowTipMessage(card.GetErrMsg());
		return RET_WF_ERROR;
	}
	// ��ȡ����Ϣ
	ret = ReadCard(&card, &cardInfo);
	if (ret)
	{
		if (RET_WF_ERROR == ret)
		{
			dlg->ShowTipMessage(card.GetErrMsg(), 1);					// �޸Ĺ�
		}
		goto L_END;
	}

	dlg->SetLimtText(1);
	if (dlg->InputPassword("������У԰������", password, 10) != 0)
	{
		ret = RET_WF_TIMEOUT;
		goto L_END;
	}

	// ��ֵ��ʽ��000001�����ŵ�һ��ֵ��000003��������ֵ�ɷѣ�000004�������ۺϽɷ�
	dlg->SetLimtText(1);	
	tipMsg.Format("�������ֵ��ʽ,1:���ŵ�һ��ֵ,2:�����ۺϽɷ�,3:������ֵ�ɷ�");
	
	if (dlg->InputQuery(tipMsg,"", trade_mode, 5) != 0)
	{
		ret = RET_WF_TIMEOUT;
		goto L_END;
	}
	input_mode = atoi((LPSTR)(LPCTSTR)trade_mode);
	if(input_mode !=1 && input_mode != 2 && input_mode != 3)
	{
		dlg->ShowTipMessage("��ֵ��ʽ���벻��ȷ", 5);
		ret = RET_WF_ERROR;
		goto L_END;
	}
	if(input_mode == 2)
	{
		trade_mode.Format("%d",input_mode+2);
	}
	trade_mode = "00000" + trade_mode;

	dlg->SetLimtText(2);
	if (dlg->InputQuery("�������ֵ����", "", mobile_num, 25) != 0)
	{
		ret = RET_WF_TIMEOUT;
		goto L_END;
	}

	dlg->SetLimtText(2);
	if (dlg->InputQuery("���ٴ������ֵ����ȷ��", "", mobile_num_2, 25) != 0)
	{
		ret = RET_WF_TIMEOUT;
		goto L_END;
	}

	if(mobile_num != mobile_num_2)
	{
		dlg->ShowTipMessage("���������ֵ���벻��, �����²���", 5);
		ret = RET_WF_ERROR;
		goto L_END;
	}

	dlg->SetLimtText(3);
	if (dlg->InputQuery("������ת�˽��(Ԫ)", "", trade_money, 10) != 0)
	{
		ret = RET_WF_TIMEOUT;
		goto L_END;
	}
	
	/*
	if (trade_money.Compare("30") != 0	&&
		trade_money.Compare("50") != 0	&&
		trade_money.Compare("100") != 0 &&
		trade_money.Compare("300") != 0
		)
	{
		dlg->ShowTipMessage("����ת�˽���, �����²���", 5);
		ret = RET_WF_ERROR;
		goto L_END;		
	}
	*/
	dlg->ShowTipMessage("���״�����, �벻Ҫ�ƶ�У԰��", 1);
	if (card.TestCardExists(5))
	{
		ret = RET_WF_ERROR;
		goto L_END;
	}	
	ret = RET_WF_ERROR;
	drtp = dlg->GetConfig()->CreateDrtpHelper();
	response = dlg->GetConfig()->CreateDrtpHelper();

	while (true)
	{
		SetStopKey(true);
		drtp->Reset();
		if (drtp->Connect())
		{
			dlg->ShowTipMessage(drtp->GetErrMsg().c_str());
			goto L_END;
		}
		// ����drtp�������
		drtp->SetRequestHeader(KS_TRANS, 1);
		drtp->AddField(F_LCERT_CODE,m_payCode);
		drtp->AddField(F_SNOTE,m_funcText);
		drtp->AddField(F_LVOL0, cardInfo.cardid);
		drtp->AddField(F_LVOL1, cardInfo.tx_cnt);
		drtp->AddField(F_LVOL2, PURSE_NO1);
		drtp->AddField(F_DAMT0, cardInfo.balance);
		drtp->AddField(F_DAMT1, atof((LPSTR)(LPCTSTR)trade_money));	
		drtp->AddField(F_SEMP_PWD, (LPSTR)(LPCTSTR)password);
		drtp->AddField(F_SCUST_NO, "system");
		drtp->AddField(F_SPHONE, (LPSTR)(LPCTSTR)mobile_num);
		drtp->AddField(F_LVOL4, atoi((LPSTR)(LPCTSTR)dlg->GetConfig()->m_systemNo));	
		drtp->AddField(F_LVOL9, TRADE_NODEBT);							// ��847316��		
		drtp->AddField(F_SBANK_CODE2,(LPSTR)(LPCTSTR)trade_mode);
		SetStopKey(false);
		// ��������
		if (drtp->SendRequest(5000))
		{
			dlg->ClearMessageLine();
			dlg->AddMessageLine(drtp->GetErrMsg().c_str());
			dlg->AddMessageLine("��[�˳�]����", dlg->GetMaxLineNo() + 2, 0);
			dlg->DisplayMessage(3);
			ret = RET_WF_TIMEOUT;
			goto L_END;
		}
		// ��ȡ���ؼ�¼
		if (drtp->HasMoreRecord())
		{
			ST_PACK *pData = drtp->GetNextRecord();
			tipMsg.Format("����д��, �벻Ҫ�ƶ�У԰��...", 1);
			dlg->ShowTipMessage(tipMsg, 0);
			// д��(��Ǯ���м�Ǯ), �����и�����ֵ
			ret = DoTransferValue(&card, &cardInfo, pData);	
			if (RET_WF_ERROR == ret)
			{
				SetStopKey(false);
				dlg->ClearMessageLine();
				tipMsg = "д��ʧ��������ת��";
				dlg->AddMessageLine(tipMsg);
				dlg->AddMessageLine("��[�˳�]����", dlg->GetMaxLineNo() + 2, 0);
				dlg->DisplayMessage(1);
				ret = RET_WF_ERROR;
				goto L_END;
			}
			// ��ʱ
			if (RET_WF_TIMEOUT == ret)
			{
				goto L_END;
			}

			trs_serial_no = pData->lvol0;

			// �����Ǯ�����˽���
			drtp->Reset();
			if (drtp->Connect())
			{
				dlg->ShowTipMessage(drtp->GetErrMsg().c_str());
				goto L_END;
			}	
			// ����drtp�������	
			drtp->SetRequestHeader(KS_TRANS, 1);
			//			drtp->AddField(F_SCLOSE_EMP, "240202");
			drtp->AddField(F_LCERT_CODE, m_payCode);			
			drtp->AddField(F_LVOL0, cardInfo.cardid);
			drtp->AddField(F_LVOL1, cardInfo.tx_cnt);
			drtp->AddField(F_LVOL2, PURSE_NO1);
			drtp->AddField(F_DAMT0, cardInfo.balance);
			drtp->AddField(F_DAMT1, atof((LPSTR)(LPCTSTR)trade_money));	
			drtp->AddField(F_SEMP_PWD, (LPSTR)(LPCTSTR)password);
			drtp->AddField(F_SCUST_NO, "system");
			drtp->AddField(F_SPHONE, (LPSTR)(LPCTSTR)mobile_num);
			drtp->AddField(F_LVOL4, atoi((LPSTR)(LPCTSTR)dlg->GetConfig()->m_systemNo));
			drtp->AddField(F_LVOL6, trs_serial_no);
			drtp->AddField(F_LVOL9, TRADE_DEBT);						// ��847317��
			drtp->AddField(F_SBANK_CODE2,(LPSTR)(LPCTSTR)trade_mode);
			SetStopKey(false);
			if (drtp->SendRequest(3000))
			{
				dlg->ClearMessageLine();
				dlg->AddMessageLine(drtp->GetErrMsg().c_str());
				dlg->AddMessageLine("��[�˳�]����", dlg->GetMaxLineNo() + 2, 0);
				dlg->DisplayMessage(3);
				ret = RET_WF_TIMEOUT;
				goto L_END;
			}

			if (drtp->HasMoreRecord())
			{
				pData = drtp->GetNextRecord();
			}
			else
			{
				dlg->ClearMessageLine();
				ret = drtp->GetReturnCode();
				if (ret != ERR_NOT_LOGIN)
				{
					ret = RET_WF_TIMEOUT;
				}
				dlg->AddMessageLine(drtp->GetReturnMsg().c_str());
				dlg->AddMessageLine("��[�˳�]����", dlg->GetMaxLineNo() + 2, 0);
				dlg->DisplayMessage(3);
				goto L_END;	
			}

			// д���ɹ�
			//cardInfo.balance = pData->damt0;
			SetStopKey(false);
			dlg->ClearMessageLine();
			tipMsg.Format("ת�˽��:%0.2fԪ", atof((LPSTR)(LPCTSTR)trade_money));
			dlg->AddMessageLine(tipMsg, dlg->GetMaxLineNo() + 1, 0);
			tipMsg.Format("�����:%0.2fԪ", cardInfo.balance);
			dlg->AddMessageLine(tipMsg, dlg->GetMaxLineNo() + 1, 0);
			dlg->AddMessageLine("�밴[�˳�]����",dlg->GetMaxLineNo() + 3, 0);
			dlg->DisplayMessage(10);
			dlg->ClearMessageLine();
			break;
		}
		else
		{
			dlg->ClearMessageLine();
			ret = drtp->GetReturnCode();
			if (ret != ERR_NOT_LOGIN)
			{
				ret = RET_WF_TIMEOUT;
			}
			dlg->AddMessageLine(drtp->GetReturnMsg().c_str());
			dlg->AddMessageLine("��[�˳�]����", dlg->GetMaxLineNo() + 2, 0);
			dlg->DisplayMessage(3);
			goto L_END;
		}
	}
	dlg->ShowTipMessage("�������!", 1);
	ret = RET_WF_SUCCESSED;
L_END:
	if(drtp)
		delete drtp;
	if(response)
		delete response;
	card.CloseCOM();
	SetStopKey(false);
	return ret;
}

// ��Ǯ������
int CKSTelecomTransWorkFlow::DoTransferValue(CKSCardManager* manager, KS_CARD_INFO *cardinfo, const ST_PACK *rec)
{
	CKSNLDlg *dlg = (CKSNLDlg*)GetMainWnd();
	int ret;
	int retries = 3;
	while (--retries >= 0)
	{
		if (manager->TestCardExists(5))
		{
			continue;
		}
		// д��
		ret = manager->SetMoney(cardinfo->cardid, D2I(rec->damt0 * 100));
		if (!ret)
		{
			cardinfo->balance = rec->damt0;
			return RET_WF_SUCCESSED;
		}
		if (manager->GetErrNo() == ERR_USER_CANCEL)
		{
			return RET_WF_TIMEOUT;
		}
		dlg->ClearMessageLine();
		dlg->AddMessageLine("д��ʧ��, �����·�У԰��!");
		dlg->AddMessageLine("��[�˳�]����", dlg->GetMaxLineNo() + 2, 0);
		dlg->DisplayMessage(1);
	}
	return RET_WF_ERROR;
}