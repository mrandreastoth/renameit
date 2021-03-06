/*
 * Copyright (C) 2002 Markus Eriksson, marre@renameit.hypermart.net
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */
// RenameItDlg.cpp : implementation file
//

#include "StdAfx.h"
#include "RenameItDlg.h"
#include "../RenameIt.h"

#include "AboutDlg.h"
#include "SettingsDlg.h"
#include "IO/Renaming/FileList.h"
#include "RenamingController.h"
#include "../NewMenu.h"

#include "IO/Renaming/Filter/SearchReplaceFilter.h"
#include "Util/CommandLineAnalyser.h"

#include <iostream>
#include <fstream>

#include <sys/types.h>
#include <sys/stat.h>
#include <io.h>
#include <Shlobj.h>
#include <Shlwapi.h>	// Used for PathGetArgs() and PathIsDirectory()

// Wizards
#include "IO/Renaming/Filter/Wizard/CaseWizard.h"
#include "IO/Renaming/Filter/Wizard/AppendWizard.h"
#include "IO/Renaming/Filter/Wizard/CropWizard.h"
#include "IO/Renaming/Filter/Wizard/EnumWizard.h"
#include "IO/Renaming/Filter/Wizard/ID3TagWizard.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

using namespace Beroux::IO::Renaming;
using namespace Beroux::IO::Renaming::Filter;
using namespace Beroux::IO::Renaming::Filter::Wizard;


/////////////////////////////////////////////////////////////////////////////
// CRenameItDlg dialog

IMPLEMENT_DYNAMIC(CRenameItDlg, CSizingDialog)

CRenameItDlg::CRenameItDlg(CWnd* pParent /*=NULL*/)
	: CSizingDialog(CRenameItDlg::IDD, pParent)
	, m_nUpdatesFreeze(0)
	, m_bDialogInit(false)
	, m_pToolTip(NULL)
{
	//{{AFX_DATA_INIT(CRenameItDlg)
	// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT
	// Note that LoadIcon does not require a subsequent DestroyIcon in Win32

	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);

	/************ Filters **************/
	CString strHomeDir;

	// Get current exe directory.
	DWORD dwSize = MAX_PATH;
	DWORD dwRet = 0;
	while ((dwRet = GetModuleFileName(NULL, strHomeDir.GetBuffer(dwSize), dwSize)) >= dwSize)
	{
		strHomeDir.ReleaseBuffer(0);
		dwSize *= 2;
	}
	ASSERT(dwRet >= 0);
	strHomeDir.ReleaseBuffer(dwRet);
	if (dwRet == 0)
	{
		// Show critical error and exit the application.
		ASSERT(false);
		AfxMessageBox(IDS_CRITICAL_ERROR, MB_ICONSTOP);
		AfxPostQuitMessage(1);
	}
	strHomeDir = strHomeDir.Left( strHomeDir.ReverseFind('\\')+1 );
}

CRenameItDlg::~CRenameItDlg()
{
	delete m_pToolTip;
}

void CRenameItDlg::DoDataExchange(CDataExchange* pDX)
{
	CSizingDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CRenameItDlg)
	DDX_Control(pDX, IDC_BUTTON_MOVEUP, m_ctlButtonMoveUp);
	DDX_Control(pDX, IDC_BUTTON_MOVEDOWN, m_ctlButtonMoveDown);
	DDX_Control(pDX, IDC_RULES_LIST, m_ctlListFilters);
	DDX_Control(pDX, IDC_FILENAMES_IN, m_ctlListFilenames);
	DDX_Control(pDX, IDC_RENAMEPART_RICHEDIT, m_ctrlRenamePart);
	//}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(CRenameItDlg, CSizingDialog)
    //{{AFX_MSG_MAP(CRenameItDlg)
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_BN_CLICKED(IDC_BUTTON_ADDRULE, &CRenameItDlg::OnButtonAddRenamer)
	ON_BN_CLICKED(IDC_BUTTON_FILTERLIST, &CRenameItDlg::OnButtonFilterlist)
	ON_COMMAND(ID_HELP_ABOUT, &CRenameItDlg::OnHelpAbout)
	ON_COMMAND(ID_FILE_OPEN, &CRenameItDlg::OnFileOpen)
	ON_COMMAND(ID_FILE_SAVEAS, &CRenameItDlg::OnFileSaveAs)
	ON_COMMAND(ID_FILE_EXIT, &CRenameItDlg::OnFileExit)
	ON_WM_DROPFILES()
	ON_BN_CLICKED(IDC_BUTTON_CLEARRULE, &CRenameItDlg::OnButtonClearrule)
	ON_BN_CLICKED(IDC_BUTTON_REMOVERULE, &CRenameItDlg::OnButtonRemoverule)
	ON_BN_CLICKED(IDC_BUTTON_ADDFILE, &CRenameItDlg::OnButtonAddfile)
	ON_NOTIFY(LVN_KEYDOWN, IDC_RULES_LIST, &CRenameItDlg::OnKeydownRulesList)
	ON_NOTIFY(NM_DBLCLK, IDC_RULES_LIST, &CRenameItDlg::OnDblclkRulesList)
	ON_COMMAND(ID_FILE_CONFIGURE, &CRenameItDlg::OnFileConfigure)
	ON_BN_CLICKED(IDC_BUTTON_ADDFOLDER, &CRenameItDlg::OnButtonAddfolder)
	ON_WM_CONTEXTMENU()
	ON_NOTIFY(LVN_INSERTITEM, IDC_RULES_LIST, &CRenameItDlg::OnLvnInsertitemRulesList)
	ON_NOTIFY(LVN_DELETEITEM, IDC_RULES_LIST, &CRenameItDlg::OnLvnDeleteitemRulesList)
    ON_BN_CLICKED(IDC_BUTTON_MOVEDOWN, &CRenameItDlg::OnButtonMovedown)
    ON_BN_CLICKED(IDC_BUTTON_MOVEUP, &CRenameItDlg::OnButtonMoveup)
    ON_NOTIFY(LVN_ITEMCHANGED, IDC_RULES_LIST, &CRenameItDlg::OnItemchangedRulesList)
    ON_COMMAND(ID_VIEW_ALWAYS_ON_TOP, &CRenameItDlg::OnViewAlwaysOnTop)
	ON_NOTIFY(LVN_ENDLABELEDIT, IDC_FILENAMES_IN, &CRenameItDlg::OnLvnEndlabeleditFilenamesIn)
	ON_NOTIFY(NM_ELMOVEDITEM, IDC_FILENAMES_IN, &CRenameItDlg::OnMovedItemFileNamesIn)
	ON_WM_ACTIVATEAPP()
	ON_COMMAND(ID_FEEDBACK_BUGREPORT, &CRenameItDlg::OnFeedbackBugreport)
	ON_COMMAND(ID_FEEDBACK_INFORMATIONREQUEST, &CRenameItDlg::OnFeedbackInformationrequest)
	ON_COMMAND(ID_FEEDBACK_SUGGESTIONS, &CRenameItDlg::OnFeedbackSuggestions)
 	ON_NOTIFY(RPS_SELCHANGE, IDC_RENAMEPART_RICHEDIT, &CRenameItDlg::OnRenamePartSelectionChange)
	ON_WM_SIZE()
	ON_WM_SIZING()
	ON_COMMAND(ID_CONTEXT_HELP, &CRenameItDlg::OnContextHelp)
	ON_NOTIFY(LVN_GETDISPINFO, IDC_FILENAMES_IN, &CRenameItDlg::OnLvnGetdispinfoFilenamesIn)
	//}}AFX_MSG_MAP
	ON_NOTIFY(LVN_ODFINDITEM, IDC_FILENAMES_IN, &CRenameItDlg::OnLvnOdfinditemFilenamesIn)
	ON_NOTIFY(NM_CLICK, IDC_FILENAMES_IN, &CRenameItDlg::OnNMClickFilenamesIn)
	ON_NOTIFY(LVN_KEYDOWN, IDC_FILENAMES_IN, &CRenameItDlg::OnLvnKeydownFilenamesIn)
	ON_WM_KEYDOWN()
	ON_BN_CLICKED(IDC_BUTTON_ADDFILTER2, &CRenameItDlg::OnBnClickedButtonAddfilter2)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CRenameItDlg message handlers
BOOL CRenameItDlg::OnInitDialog()
{
    CSizingDialog::OnInitDialog();

	// Set the icon for this dialog.  The framework does this automatically
	//  when the application's main window is not a dialog
	SetIcon(m_hIcon, TRUE);			// Set big icon
	SetIcon(m_hIcon, FALSE);		// Set small icon

	// Add the status bar
	m_statusBar.Create(WS_CHILD | WS_VISIBLE | CCS_BOTTOM, CRect(0,0,0,0), this, IDC_STATUS_BAR);
	CRect rectStatusBar, rectFilesPannel;
	m_statusBar.GetWindowRect(rectStatusBar);
	m_ctlListFilenames.GetWindowRect(rectFilesPannel);
	m_ctlListFilenames.SetWindowPos(NULL, 0, 0, rectFilesPannel.Width(), rectFilesPannel.Height()-rectStatusBar.Height(), SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOZORDER);
	int nSize = 123;
	m_statusBar.SetParts(0, &nSize);

	// Define how to resize.
	AddResizableCtrl(IDC_STATIC_RULES, _T("CX"));
	AddResizableCtrl(IDC_RULES_LIST, _T("CX"));
	AddResizableCtrl(IDC_BUTTON_MOVEDOWN, _T("X"));
	AddResizableCtrl(IDC_BUTTON_MOVEUP, _T("X"));
	AddResizableCtrl(IDOK, _T("X"));
	AddResizableCtrl(IDCANCEL, _T("X"));
	AddResizableCtrl(IDC_FILENAMES_IN, _T("C"));
	AddResizableCtrl(IDC_STATUS_BAR, _T("CX+Y"));

	// Set NewMenu style
	CNewMenu::SetMenuDrawMode( CNewMenu::STYLE_XP );

	// Add columns...
	m_ctlListFilenames.SetExtendedStyle(LVS_EX_CHECKBOXES);
	{
		CString	str;
		str.LoadString(IDS_BEFORE);	m_ctlListFilenames.InsertColumn( 0, str, LVCFMT_LEFT, 100, 0 );
		str.LoadString(IDS_AFTER);	m_ctlListFilenames.InsertColumn( 1, str, LVCFMT_LEFT, 100, 1 );
		str.LoadString(IDS_FILTER);	m_ctlListFilters.InsertColumn( 0, str, LVCFMT_LEFT, 200, 0 );
	}

	// Set what part of the path to rename
	CSettingsDlg	config;
	m_ctrlRenamePart.SetRenameParts( config.GetType() );
	m_fcFilters.SetPathRenamePart( config.GetType() );

	// Set up the tooltip
	m_pToolTip = new CToolTipCtrl();
	VERIFY( m_pToolTip->Create(this) );
	VERIFY( m_pToolTip->AddTool(&m_ctrlRenamePart, IDS_TT_SELECT_RENAME_PARTS) );
	m_pToolTip->Activate(TRUE);

	// Process command line
	CString strCmd = PathGetArgs(GetCommandLine()); // Get command line without path to this exe.
	ProcessCommandLine( strCmd );

	// If there are files to rename.
	if (GetDisplayedList().IsEmpty())
	{
		CSettingsDlg config;
		if (m_fcFilters.GetFilterCount() == 0 && config.AutoAddRenamer())
			// Find the default filter in the filters' list.
			OnButtonAddRenamer();
	}

	m_bDialogInit = true;
	SetWindowPos(NULL, 0, 0, 575, 481, SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOZORDER);
	return TRUE;  // return TRUE  unless you set the focus to a control
}

// If you add a minimize button to your dialog, you will need the code below
//  to draw the icon.  For MFC applications using the document/view model,
//  this is automatically done for you by the framework.

void CRenameItDlg::OnPaint() 
{
	if (IsIconic())
	{
		CPaintDC dc(this); // device context for painting

		SendMessage(WM_ICONERASEBKGND, (WPARAM) dc.GetSafeHdc(), 0);

		// Center icon in client rectangle
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// Draw the icon
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
		CSizingDialog::OnPaint();
}

// The system calls this to obtain the cursor to display while the user drags
//  the minimized window.
HCURSOR CRenameItDlg::OnQueryDragIcon()
{
	return (HCURSOR) m_hIcon;
}

void CRenameItDlg::OnHelpAbout() 
{
	CAboutDlg dlgAbout;
	dlgAbout.DoModal();
}

void CRenameItDlg::OnFileOpen()
{
	CSettingsDlg	config;

	CString rFilter;
	rFilter.LoadString(IDS_RIT_OFN_FILTER);
	CFileDialog dlgFile(TRUE, _T(".rit"), NULL, OFN_HIDEREADONLY|OFN_EXPLORER, rFilter);
	dlgFile.m_ofn.lpstrInitialDir = config.GetPath();

	if (dlgFile.DoModal() == IDOK)
		LoadFilterList(dlgFile.GetPathName());
}

void CRenameItDlg::OnFileSaveAs()
{
	if (m_ctlListFilters.GetItemCount() == 0)
	{
		AfxMessageBox(IDS_NO_FILTER_TO_SAVE, MB_ICONINFORMATION);
		return;
	}

	SaveFilterList();
}

void CRenameItDlg::OnFileExit()
{
	// This is the only way to exit the application from the main screen.

	// ALTERNATIVE: ExitInstance() method of the App.
	AfxPostQuitMessage(0);
}

void CRenameItDlg::OnOK() 
{
	RenameAllFiles();
}

void CRenameItDlg::OnCancel() 
{
	if (GetDisplayedList().IsEmpty()
		|| m_ctlListFilters.GetItemCount() == 0
		|| AfxMessageBox(IDS_CONFIRM_EXIT, MB_YESNO) == IDYES)
		OnFileExit();
}

void CRenameItDlg::OnButtonAddRenamer() 
{
	boost::scoped_ptr<Filter::IFilter> filter(new CSearchReplaceFilter());
	AddFilter(filter.get());
}

void CRenameItDlg::OnBnClickedButtonAddfilter2()
{
	// Get the point where the menu is to be displayed.
	CRect rect;
	GetDlgItem(IDC_BUTTON_ADDRULE)->GetWindowRect(rect);
	POINT point;
	point.x = rect.left;
	point.y = rect.bottom-1;

	// Show menu
	CNewMenu menu;
    VERIFY( menu.LoadMenu(IDR_FILTERS_MENU) );
	CNewMenu* pmenuPopup = (CNewMenu*) menu.GetSubMenu(0);
    ASSERT(pmenuPopup != NULL);
	pmenuPopup->LoadToolBar(IDR_FILTERS_TOOLBAR);
	pmenuPopup->SetDefaultItem(ID_FILTERS_SEARCHREPLACE);
    DWORD dwSelectionMade = pmenuPopup->TrackPopupMenu(
		(TPM_LEFTALIGN|TPM_LEFTBUTTON|TPM_NONOTIFY|TPM_RETURNCMD),
		point.x, point.y, this);

	// Clean Up and Return
    pmenuPopup->DestroyMenu();

	// Clean Up
	menu.DestroyMenu();

	std::auto_ptr<CFilterWizard> wizard;
	switch (dwSelectionMade)
	{
	case 0:
		// Nothing selected...
		break;

	case ID_FILTERS_SEARCHREPLACE:
		OnButtonAddRenamer();
		break;

	case ID_FILTERS_CHANGECASE:		wizard.reset(new Wizard::CCaseWizard()); break;
	case ID_FILTERS_APPEND:			wizard.reset(new Wizard::CAppendWizard()); break;
	case ID_FILTERS_CROP:			wizard.reset(new Wizard::CCropWizard()); break;
	case ID_FILTERS_ENUMERATION:	wizard.reset(new Wizard::CEnumWizard()); break;
	case ID_FILTERS_ID3TAG:			wizard.reset(new Wizard::CID3TagWizard()); break;

	default:
		ASSERT(false);
	}

	if (wizard.get() != NULL)
	{
		boost::scoped_ptr<Filter::IPreviewFileList> previewSamples(GetPreviewSamples(m_fcFilters.GetFilterCount()));
		shared_ptr<Filter::IFilter> filter(wizard->Execute(*previewSamples));
		if (filter.get() != NULL)
			AddFilter(filter.get());
	}
}

void CRenameItDlg::OnButtonFilterlist() 
{
	CNewMenu	menu,
				menuRecents;
	CBitmap		bmpSave,
				bmpOpen;
	CSettingsDlg	config;
	DWORD		dwSelectionMade;
	CString		strBuffer,
				strPath;
	CRect		rect;
	POINT		point;

	// Load bitmaps
	VERIFY( bmpOpen.LoadBitmap(IDB_OPEN) );
	VERIFY( bmpSave.LoadBitmap(IDB_SAVE) );

	// Create menus
	VERIFY( menu.CreatePopupMenu() );
	VERIFY( menuRecents.CreatePopupMenu() );

	// Create the recent RIT sub-menu
	if (config.GetRecentFilterCount() == 0)
	{
		VERIFY( strBuffer.LoadString( IDS_NO_RECENT_FILTERS ) );
		VERIFY( menuRecents.AppendMenu( MF_STRING|MF_GRAYED, 0, strBuffer ) );
	}
	else
		for (int i=1; i<=config.GetRecentFilterCount(); ++i)
		{
			config.GetRecentFilter(i, strPath);
			strBuffer.Format(_T("&%d %s"), i, strPath);
			VERIFY( menuRecents.AppendMenu( MF_STRING, 100+i, strBuffer ) );
		}

	// Append "Open"
	VERIFY( strBuffer.LoadString( IDS_OPEN ) );
	VERIFY( menu.AppendMenu( MF_STRING, 2000, strBuffer, &bmpOpen) );

	// Append "Save"
	VERIFY( strBuffer.LoadString( IDS_SAVE_AS ) );
	VERIFY( menu.AppendMenu( MF_STRING | (m_ctlListFilters.GetItemCount()==0?MF_DISABLED:0), 1000, strBuffer, &bmpSave) );

	// Append --------------
	VERIFY(  menu.AppendMenu( MF_SEPARATOR ) );

	// Append "Recent RIT"
	VERIFY( strBuffer.LoadString( IDS_RECENT_FILTERS ) );
	VERIFY( menu.AppendMenu(MF_POPUP, (UINT)(UINT_PTR)menuRecents.GetSafeHmenu(), strBuffer) );

	// Get the point where the menu is to be displayed.
	GetDlgItem(IDC_BUTTON_FILTERLIST)->GetWindowRect(rect);
	point.x = rect.left;
	point.y = rect.bottom-1;

	// Show and track the menu
	menu.SetDefaultItem(1);
	dwSelectionMade = menu.TrackPopupMenu(
		TPM_LEFTALIGN|TPM_LEFTBUTTON|TPM_NONOTIFY|TPM_RETURNCMD,
		point.x, point.y, this );

	// Clean Up
	menu.DestroyMenu();
	menuRecents.DestroyMenu();

	switch (dwSelectionMade)
	{
	case 0:
		// Nothing selected...
		break;
	case 1000:	// Save
		OnFileSaveAs();
		break;
	case 2000:	// Open
		OnFileOpen();
		break;
	default:
		// Recent RIT
		config.GetRecentFilter(dwSelectionMade-100, strPath);
		LoadFilterList(strPath);
	}

	// Clean memory
	if (!bmpOpen.m_hObject)
		bmpOpen.DeleteObject();
	if (!bmpSave.m_hObject)
		bmpSave.DeleteObject();
}

/** Replace the current filters with the filters contained in a RIT file.
 * @param szFileName	Full path to the RIT file to load.
 * @return True if the filters have been loaded.
 */
bool CRenameItDlg::LoadFilterList(LPCTSTR szFileName)
{
	CSettingsDlg config;

	// Is current filter list is not empty?
	if (m_ctlListFilters.GetItemCount())
	{
		// Ask user: Save current filter?
		switch (AfxMessageBox(IDS_NOT_EMPTY_FILTER_LIST_SAVE, MB_YESNOCANCEL))
		{
		case IDYES:
			// Save the current filter list
			if (!SaveFilterList())
				return false;
			break;
		case IDCANCEL:
			return false;
		}
	}

	// Replace the filters
	m_fcFilters.RemoveAllFilters();
	if (m_fcFilters.LoadFilters(szFileName) > 0)
	{
		// Freeze the updates.
		PushUpdatesFreeze();

		VERIFY( config.AddRecentFilter(szFileName) );
		m_ctrlRenamePart.SetRenameParts( m_fcFilters.GetPathRenamePart() );

		// Un-freeze the updates.
		PopUpdatesFreeze();
		return true;
	}
	AfxMessageBox(IDS_CANT_OPEN_FILTERS, MB_ICONERROR);
	return false;
}

/** Save the current filter list in a RIT file.
 * @return True if the RIT has been saved sucessfully.
 */
bool CRenameItDlg::SaveFilterList()
{
	CSettingsDlg config;
	CString rFilter;
	rFilter.LoadString(IDS_RIT_OFN_FILTER);
	CFileDialog dlgFile(
		FALSE, 
		_T(".rit"), 
		NULL, 
		OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT | OFN_EXPLORER, 
		rFilter);

	// Ask where to save
	dlgFile.m_ofn.lpstrInitialDir = config.GetPath();
	if (dlgFile.DoModal() != IDOK)
		return false;

	// Save the list
	if (!m_fcFilters.SaveFilters(dlgFile.GetPathName()))
	{
		AfxMessageBox(IDS_CANT_SAVE_FILTERS, MB_ICONERROR);
		return false;
	}
	VERIFY( config.AddRecentFilter(dlgFile.GetPathName()) );
	return true;
}

void CRenameItDlg::OnDropFiles(HDROP hDropInfo) 
{
	if (hDropInfo)
	{
		int iFiles = DragQueryFile(hDropInfo, 0xFFFFFFFF, NULL, 0);		

		// Clear error list
		m_dlgNotAddedFiles.ClearList();

		// Freeze the updates.
		PushUpdatesFreeze();

		// For each dropped file...
		for (int i=0; i<iFiles; i++)
		{
			CString strPath;
			{
				UINT nPathLength = DragQueryFile(hDropInfo, i, NULL, 0);
				DragQueryFile(hDropInfo, i, strPath.GetBuffer(nPathLength + 1), nPathLength + 1);
				strPath.ReleaseBuffer(nPathLength);
			}

			if (PathIsDirectory(strPath))
			{
				// Add the folder also to enable renaming folders.
				AddFolder(strPath);

				CString msg;
				AfxFormatString1(msg, IDS_ADDDIR, strPath);
				if (AfxMessageBox(msg, MB_YESNO|MB_ICONQUESTION) == IDYES)
					// Add dir recursive
					AddFilesInFolder(strPath);
			}
			else
				// Not a subdir...
				AddFile(strPath);
		}

		// Un-freeze the updates.
		PopUpdatesFreeze();

		// Display errors if there are some.
		if (!m_dlgNotAddedFiles.HasErrors())
			m_dlgNotAddedFiles.DoModal();
	}

	DragFinish(hDropInfo);
	hDropInfo = NULL;	
}

void CRenameItDlg::UpdateFilelist()
{
	ASSERT(m_nUpdatesFreeze == 0);

	// Which list to use: Files or Directories?
	Gui::CMemoryFileList& mflItems = GetDisplayedList();

	// "Insert/Remove" items in our virtual list.
	m_ctlListFilenames.SetItemCount(mflItems.GetFileCount());

	// Filter all the checked files.
	m_fcFilters.FilterFileNames(
		mflItems.GetInputIteratorAt(mflItems.GetFirstChecked()),	// Begin
		mflItems.GetInputIteratorAt(mflItems.GetFirstChecked()),	// First
		mflItems.GetInputIteratorAt(mflItems.GetTail()),			// Last
		mflItems.GetOutputIteratorAt(mflItems.GetFirstChecked()));	// Result

	// Find how many files will be renamed, and keep the same name for unchecked files.
	int nRenameCount = 0;	// Number of files affected.
	BOOST_FOREACH(Gui::CMemoryFileList::ITEM& item, mflItems)
	{
		if (item.bChecked)
		{
			// Preview the renamed file name.
			if (item.fnAfter != item.fnBefore)
				++nRenameCount;
		}
		else
		{
			// Keep the same name as before.
			item.fnAfter = item.fnBefore;
		}
	}

	// Update the view.
	m_ctlListFilenames.RedrawItems(0, m_ctlListFilenames.GetItemCount());

	// Update the status bar.
	{
		CString strRenameCount,
				strFilesCount,
				strStatus;
		strRenameCount.Format(_T("%d"), nRenameCount);
		strFilesCount.Format(_T("%d"), mflItems.GetFileCount());
		AfxFormatString2(strStatus, IDS_STATUS_RENAME_COUNT, strRenameCount, strFilesCount);
		m_statusBar.SetText(strStatus, 0, 0);
	}

	// Enable the renaming only when there are some files to rename.
	GetDlgItem(IDOK)->EnableWindow( !mflItems.IsEmpty() );
}

// Update the filter list control with the new filters and their descriptions
void CRenameItDlg::UpdateFilterlist()
{
	ASSERT(m_nUpdatesFreeze == 0);

	m_ctlListFilters.DeleteAllItems();
    LV_ITEM lvi = {LVIF_TEXT};
	CString strRule;

	for (int i=0; i<m_fcFilters.GetFilterCount(); ++i)
	{
		shared_ptr<Filter::IFilter> filter = m_fcFilters.GetFilterAt(i);
		ASSERT(filter.get() != NULL);

		strRule = filter->GetFilterDescription();

		lvi.mask = LVIF_TEXT;
		lvi.iItem = m_ctlListFilters.GetItemCount();
		lvi.iSubItem = 0;
		lvi.pszText = strRule.GetBuffer(0);
		m_ctlListFilters.InsertItem(&lvi);
		strRule.ReleaseBuffer();
	}
}

void CRenameItDlg::SwapFilterItems(UINT itemIdx1, UINT itemIdx2)
{
    // Swap the filter details entries.
    m_fcFilters.SwapFilters(itemIdx1, itemIdx2);

    // Retrieve the selection states of both filter list box items.
    const UINT KAllStatesMask = UINT(-1);
    UINT uState1 = m_ctlListFilters.GetItemState( itemIdx1, KAllStatesMask);
    UINT uState2 = m_ctlListFilters.GetItemState( itemIdx2, KAllStatesMask);

    // Replace/refresh the text in the filter list box with the text from
    // the filter details entries (which were swapped earlier).

    // Update filter list box item specified by itemIdx1.
	{
		shared_ptr<Filter::IFilter> filter = m_fcFilters.GetFilterAt(itemIdx1);
		ASSERT(filter.get() != NULL);
		CString strRule = filter->GetFilterDescription();
		LV_ITEM lvi = {(LVIF_TEXT | LVIF_STATE)}; // NOTE: Rest of struct init'd to 0's.
		lvi.iItem = itemIdx1;
		lvi.state = uState2;
		lvi.stateMask = KAllStatesMask;
		lvi.pszText = strRule.GetBuffer(0);
		m_ctlListFilters.SetItem(&lvi);
	}

    // Update filter list box item specified by itemIdx2.
	{
		shared_ptr<Filter::IFilter> filter = m_fcFilters.GetFilterAt(itemIdx2);
		ASSERT(filter.get() != NULL);
		CString strRule = filter->GetFilterDescription();
		LV_ITEM lvi = {(LVIF_TEXT | LVIF_STATE)}; // NOTE: Rest of struct init'd to 0's.
		lvi.iItem = itemIdx2;
		lvi.state = uState1;
		lvi.pszText = strRule.GetBuffer(0);
		m_ctlListFilters.SetItem(&lvi);
	}
}

void CRenameItDlg::OnButtonClearfiles() 
{
	// Freeze the updates.
	PushUpdatesFreeze();

	// Clear the files in the list.
	m_flFiles.RemoveAll();
	m_flDirectories.RemoveAll();

	// Un-freeze the updates.
	PopUpdatesFreeze();
}

void CRenameItDlg::OnButtonClearrule() 
{
	// Freeze the updates.
	PushUpdatesFreeze();

	// Remove all filters.
	m_fcFilters.RemoveAllFilters();
	
	// Un-freeze the updates.
	PopUpdatesFreeze();
}

void CRenameItDlg::OnButtonRemovefile() 
{
	// Freeze the updates.
	PushUpdatesFreeze();

	// Capture the indexes to erase.
	vector<int> vIndexesToErase;
	POSITION pos = m_ctlListFilenames.GetFirstSelectedItemPosition();
	while (pos)
		vIndexesToErase.push_back(m_ctlListFilenames.GetNextSelectedItem(pos));

	// Which list to use: Files or Directories?
	Gui::CMemoryFileList& mflItems = GetDisplayedList();

	// Erase them starting for the last one so that the index always match.
	sort(vIndexesToErase.begin(), vIndexesToErase.end(), greater<int>());
	for (unsigned i=0; i<vIndexesToErase.size(); ++i)
		mflItems.RemoveAt(mflItems.GetIteratorAt(vIndexesToErase[i]));

	// Un-freeze the updates.
	PopUpdatesFreeze();
}

void CRenameItDlg::OnButtonRemoverule() 
{
	// Freeze the updates.
	PushUpdatesFreeze();

	// Erase them
	int iselectedNo;
	while ((iselectedNo = m_ctlListFilters.GetNextItem(-1, LVIS_SELECTED)) != -1)
	{
		m_ctlListFilters.DeleteItem(iselectedNo);
		m_fcFilters.RemoveFilter(iselectedNo);
	}

	// Un-freeze the updates.
	PopUpdatesFreeze();
}

void CRenameItDlg::OnButtonAddfile() 
{
	TCHAR		buffer[_MAX_PATH*100];
	POSITION	pos;
	CString		rOfnFilter,
				rOfnTitle;
	rOfnFilter.LoadString(IDS_FILES_OFN_FILTER);
	CFileDialog dlgFile(TRUE, NULL, NULL,
		OFN_FILEMUSTEXIST | OFN_ALLOWMULTISELECT | OFN_HIDEREADONLY | OFN_EXPLORER,
		rOfnFilter, this);

	// provide wide buffer
#if (_WIN32_WINNT >= 0x0500)	// Windows 2000/XP
	dlgFile.m_ofn.Flags |= OFN_DONTADDTORECENT;
#endif
	dlgFile.m_ofn.lpstrFile = buffer;
	dlgFile.m_ofn.lpstrFile[0] = 0;
	dlgFile.m_ofn.nMaxFile = sizeof(buffer)/sizeof(buffer[0]);
	rOfnTitle.LoadString(IDS_FILES_OFN_TITLE);
	dlgFile.m_ofn.lpstrTitle = rOfnTitle;

	// Run dialog
	if (dlgFile.DoModal() == IDOK)
	{
		// Clear error list
		m_dlgNotAddedFiles.ClearList();

		// Freeze the updates.
		PushUpdatesFreeze();

		// Add each file...
		pos = dlgFile.GetStartPosition();
		while (pos)
			AddFile( dlgFile.GetNextPathName(pos) );

		// Un-freeze the updates.
		PopUpdatesFreeze();

		// Display errors if there are some.
		if (!m_dlgNotAddedFiles.HasErrors())
			m_dlgNotAddedFiles.DoModal();
	}
}

void CRenameItDlg::OnKeydownRulesList(NMHDR* pNMHDR, LRESULT* pResult) 
{
	LV_KEYDOWN* pLVKeyDown = (LV_KEYDOWN*)pNMHDR;

	// Intercept DEL-KEY
	switch (pLVKeyDown->wVKey)
	{
	case VK_DELETE:
		OnButtonRemoverule();
		break;
	case VK_INSERT:
		OnButtonAddRenamer();
		break;
	}

	*pResult = 0;
}

bool CRenameItDlg::AddFile(const CString &strFileName)
{
	// Get the full path
	CPath fnFileName;
	{
		CString strFullPath;
		DWORD dwRet = GetFullPathName(strFileName, MAX_PATH, strFullPath.GetBuffer(MAX_PATH), NULL);
		strFullPath.ReleaseBuffer();
		if (dwRet > MAX_PATH)	// The buffer is too small?
		{
			dwRet = GetFullPathName(strFileName, dwRet, strFullPath.GetBuffer(dwRet), NULL);
			strFullPath.ReleaseBuffer();
		}
		if (dwRet == 0)	// Some error? (file not found?)
		{
			// Get error message
			LPTSTR lpMsgBuf = NULL;
			FormatMessage( 
				FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
				NULL,
				GetLastError(),
				MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
				(LPTSTR) &lpMsgBuf,
				0,
				NULL );
	
			// Add that error.
			fnFileName = strFileName;
			m_dlgNotAddedFiles.AddFile(fnFileName.GetPath(), lpMsgBuf);
	
			// Free the buffer.
			LocalFree( lpMsgBuf );
			
			return false;
		}
		
		fnFileName = strFullPath;
	}

	// Check if already in the list
	if (m_flFiles.FindFile(fnFileName) != m_flFiles.GetTail())
	{
		CString	strErrorMessage;
		strErrorMessage.LoadString(IDS_FILE_ALREADY_EXIST);
		m_dlgNotAddedFiles.AddFile(fnFileName.GetPath(), strErrorMessage);
		return false;
	}

	// Check file's attributes
	DWORD dwAttr = GetFileAttributes(fnFileName.GetPath());
	if (dwAttr & FILE_ATTRIBUTE_SYSTEM)
	{
		CString	strErrorMessage;
		strErrorMessage.LoadString(IDS_RENAME_SYSTEM_FILE);
		m_dlgNotAddedFiles.AddFile(fnFileName.GetPath(), strErrorMessage);
		return false;
	}

	// Add file name to memory list
	m_flFiles.AddFile(fnFileName);
	return true;
}

bool CRenameItDlg::AddFolder(const CString& strPath)
{
	// Get the full path and create the CPath.
	CPath path;
	{
		// Get the full path.
		CString strFullPath;
		DWORD dwRet = GetFullPathName(strPath, MAX_PATH, strFullPath.GetBuffer(MAX_PATH), NULL);
		strFullPath.ReleaseBuffer();
		if (dwRet > MAX_PATH)	// The buffer is too small?
		{
			dwRet = GetFullPathName(strPath, dwRet, strFullPath.GetBuffer(dwRet), NULL);
			strFullPath.ReleaseBuffer();
		}
		if (dwRet == 0)	// Some error? (file not found?)
		{
			// Get error message
			LPTSTR lpMsgBuf = NULL;
			FormatMessage( 
				FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
				NULL,
				GetLastError(),
				MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
				(LPTSTR) &lpMsgBuf,
				0,
				NULL );
	
			// Add that error.
			path = strPath;
			m_dlgNotAddedFiles.AddFile(path.GetPath(), lpMsgBuf);
	
			// Free the buffer.
			LocalFree( lpMsgBuf );
			
			return false;
		}
		
		// Make sure the path ends by a \.
		TCHAR chLastCharacter = strFullPath[strFullPath.GetLength() - 1];
		if (chLastCharacter != '\\' && chLastCharacter != '/')
			strFullPath.AppendChar('\\');

		path = strFullPath;
	}

	// Check if already in the list
	if (m_flDirectories.FindFile(path) != m_flDirectories.GetTail())
	{
		m_dlgNotAddedFiles.AddFile(path.GetPath(), IDS_FILE_ALREADY_EXIST);
		return false;
	}

	// Check directory's attributes
	DWORD dwAttr = GetFileAttributes(path.GetPath());
	if (dwAttr & FILE_ATTRIBUTE_SYSTEM)
	{
		m_dlgNotAddedFiles.AddFile(path.GetPath(), IDS_RENAME_SYSTEM_FILE);
		return false;
	}

	// Add directory name to memory list
	m_flDirectories.AddFile(path);
	return true;
}

void CRenameItDlg::AddFilesInFolder(const CString &strDirName, bool bSubfolders)
{
	// Freeze the updates.
	PushUpdatesFreeze();

	if (PathIsDirectory(strDirName))
	{
		CFileFind	ffFileFind;

		// Search dir
		CString	 strFullPath(strDirName);
		if (strFullPath.Right(1) != _T("\\"))
			strFullPath.AppendChar(_T('\\'));
		strFullPath +=	"*.*";
		if (ffFileFind.FindFile(strFullPath))
		{
			// Add each file
			BOOL bMoreFiles;
			do
			{
				bMoreFiles = ffFileFind.FindNextFile();
				if (ffFileFind.GetFileName() != _T(".") &&
					ffFileFind.GetFileName() != _T(".."))
				{
					if (!ffFileFind.IsDirectory())
						// Not a subdir...
						AddFile(ffFileFind.GetFilePath());
					else if (bSubfolders)
					{
						// Add the folder also to enable renaming folders.
						AddFolder(ffFileFind.GetFilePath());

						// Add recurcively the subfolder
						AddFilesInFolder(ffFileFind.GetFilePath(), bSubfolders);
					}
				}
			} while (bMoreFiles);
			ffFileFind.Close();
		}
	}
	else
		// Ordinary	file
		AddFile(strDirName);

	// Un-freeze the updates.
	PopUpdatesFreeze();
}

void CRenameItDlg::OnDblclkRulesList(NMHDR* pNMHDR, LRESULT* pResult)
{
	int iselectedNo;

	if (m_ctlListFilters.GetSelectedCount() != 1)
		return;

    iselectedNo = m_ctlListFilters.GetNextItem(-1, LVIS_SELECTED);
	EditRule(iselectedNo);

	*pResult = 0;
}

void CRenameItDlg::OnFileConfigure() 
{
	// Freeze the updates.
	PushUpdatesFreeze();

	// Show configuration dialog
	CSettingsDlg dlg;
	dlg.DoModal();

	// Un-freeze the updates.
	PopUpdatesFreeze();
}

void CRenameItDlg::OnButtonAddfolder()
{
	// Set Browse Info
	BROWSEINFO		bi;
	ZeroMemory(&bi, sizeof(BROWSEINFO));
	bi.hwndOwner = GetSafeHwnd();
	CString	rOfnTitle;
	rOfnTitle.LoadString(IDS_FOLDER_OFN_TITLE);
	bi.lpszTitle = rOfnTitle;
	bi.ulFlags = BIF_DONTGOBELOWDOMAIN | BIF_RETURNONLYFSDIRS | BIF_BROWSEINCLUDEFILES
		| BIF_NONEWFOLDERBUTTON;

	// Show Browe Dialog and Get Path
	LPITEMIDLIST	pidl = SHBrowseForFolder(&bi);
	if (pidl == NULL)
		return;		// User selected Cancel
	CString strPath;
	bool bSuccess = SHGetPathFromIDList(pidl, strPath.GetBuffer(MAX_PATH)) != 0;

	// free memory used
	IMalloc* imalloc = 0;
	if (SUCCEEDED( SHGetMalloc(&imalloc) ))
	{
		imalloc->Free(pidl);
		imalloc->Release();
	}

	// If it failed, we stop here.
	if (!bSuccess)
	{
		strPath.ReleaseBuffer(0);
		return;
	}
	strPath.ReleaseBuffer();

	// Clear error list
	m_dlgNotAddedFiles.ClearList();

	// Add the folder also to enable renaming folders.
	AddFolder(strPath);

	// Add the files in folder and subfolders to the list
	AddFilesInFolder(strPath);

	// Display errors if there are some.
	if (!m_dlgNotAddedFiles.HasErrors())
		m_dlgNotAddedFiles.DoModal();
}

// Process the provided command line arguments
bool CRenameItDlg::ProcessCommandLine(LPCTSTR szArgs)
{
	// Clear error list
	m_dlgNotAddedFiles.ClearList();

	if (!_tcsncmp(szArgs, _T("/$shell$ext$ "), 13))
		// Files provided by the shell extension
		ProcessShellCommandLine(&szArgs[13]);
	else
		// User command-line or drag&drop files on the exe
		ProcessUserCommandLine();

	return true;
}

void CRenameItDlg::ProcessShellCommandLine(LPCTSTR szArgs)
{
	TCHAR			szBuffer[MAX_PATH];

	// File mapping
	LPCTSTR pStart = szArgs;
	LPCTSTR pEnd = _tcschr(&pStart[1], ':');
	if (pEnd == NULL)
	{
		ASSERT(false);
		MsgBoxUnhandledError(__FILE__, __LINE__);
		return;
	}
	_tcsncpy_s(szBuffer, pStart, pEnd-pStart);
	szBuffer[pEnd-pStart] = _T('\0');
	HANDLE hMapFile = OpenFileMapping(FILE_MAP_READ, FALSE, szBuffer);
	if (hMapFile == NULL)
	{
		ASSERT(false);
		MsgBoxUnhandledError(__FILE__, __LINE__);
		return;
	}
	pStart = pEnd+1;
	pEnd = _tcschr(&pStart[1], ':');
	if (pEnd == NULL)
	{
		ASSERT(false);
		MsgBoxUnhandledError(__FILE__, __LINE__);
		return;
	}
	_tcsncpy_s(szBuffer, pStart, pEnd-pStart);
	szBuffer[pEnd-pStart] = _T('\0');
    LPTSTR szFiles = (LPTSTR) MapViewOfFile(
		hMapFile,							// Handle to mapping object
		FILE_MAP_READ,						// Read/write permission
		0,									// Max. object size
		0,									// Size of hFile
		StrToInt(szBuffer));				// Map size
	if (szFiles == NULL)
	{
		ASSERT(false);
		MsgBoxUnhandledError(__FILE__, __LINE__);
		return;
	}

	// Done Event
	pStart = pEnd+1;
	HANDLE hDoneEvent = OpenEvent(EVENT_ALL_ACCESS, FALSE, pStart);
	if (hDoneEvent == NULL)
	{
		ASSERT(false);
		MsgBoxUnhandledError(__FILE__, __LINE__);
		return;
	}

	// Freeze the updates.
	PushUpdatesFreeze();

    // Add files&folders
	while (*szFiles != _T('\0'))
	{
		// Add to list
		if (PathIsDirectory(szFiles))
		{
			// Add the folder also to enable renaming folders.
			AddFolder(szFiles);

			// Add sub-folders.
			AddFilesInFolder(szFiles, true);
		}
		else
			AddFile(szFiles);

		// Go to next file path
		szFiles += _tcslen(szFiles)+1;
	}

	// Tell it's done
	SetEvent(hDoneEvent);

	// Un-freeze the updates.
	PopUpdatesFreeze();

	// Display errors if there are some.
	if (!m_dlgNotAddedFiles.HasErrors())
		m_dlgNotAddedFiles.DoModal();
}

void CRenameItDlg::ProcessUserCommandLine()
{
	// TODO: The LOG file is saved but never used in the software.

	Beroux::Util::CCommandLineAnalyser cmdLine;
	if (!cmdLine.AnalyseCommandLine())
		return;

	// Freeze the updates.
	PushUpdatesFreeze();

	// Add each file
	for (Beroux::Util::CCommandLineAnalyser::PathElement* pe = cmdLine.GetFirstPathElement(); pe!=NULL; pe = cmdLine.GetNextPathElement())
	{
		// Add to list
		CFileFind ffFileFind;
		if (ffFileFind.FindFile(pe->strPath))	// Consider it as wildcard (may a single file as well)
		{
			// Add each file
			BOOL bMoreFiles;
			do
			{
				bMoreFiles = ffFileFind.FindNextFile();
				if (ffFileFind.GetFileName() != _T(".") &&
					ffFileFind.GetFileName() != _T(".."))
				{
					if (ffFileFind.IsDirectory())
					{
						// Add the folder also to enable renaming folders.
						AddFolder(ffFileFind.GetFilePath());

						// Add the folder
						AddFilesInFolder(ffFileFind.GetFilePath(), pe->bRecursive);
					}
					else
					{
						// Not a subdir...
						AddFile(ffFileFind.GetFilePath());
					}
				}
			} while (bMoreFiles);
			ffFileFind.Close();
		}
	}

	// Un-freeze the updates.
	PopUpdatesFreeze();

	// Load the filters.
	if (!cmdLine.GetFilterFile().IsEmpty())
		LoadFilterList(cmdLine.GetFilterFile());

	// Display errors if there are some.
	if (!m_dlgNotAddedFiles.HasErrors())
		m_dlgNotAddedFiles.DoModal();

	// Command-line execution mode?
	if (cmdLine.IsCommandLineExecutionMode())
	{
		if (m_fcFilters.GetFilterCount() == 0)
			AfxMessageBox(IDS_CMDLINE_NO_FILTER, MB_ICONEXCLAMATION);
		else
		{
			if (RenameAllFiles(CRenamingController::elError | CRenamingController::elWarning))
				AfxPostQuitMessage(0);
			//else if (cmdLine.IsSilentMode())
			//	AfxPostQuitMessage(1);
		}
	}
}

void CRenameItDlg::OnContextMenu(CWnd* pWnd, CPoint point)
{
	switch (pWnd->GetDlgCtrlID())
	{
	case IDC_RULES_LIST:
		OnContextMenuRules(point);
		break;
	case IDC_FILENAMES_IN:
		OnContextMenuFilenames(point);
		break;
	}
}

BOOL CRenameItDlg::OnContextMenuRules(CPoint point)
{
	CNewMenu	menu;
	CString		str;
	DWORD		dwSelectionMade;
	CBitmap		bmpAdd,
				bmpUp,
				bmpDown,
				bmpDel,
				bmpDelAll,
				bmpProp;
	UINT		uSelItem,
				uFlagUp = 0,
				uFlagDown = 0,
				uFlagDel = 0,
				uFlagDelAll = 0,
				uFlagProp = 0;
	CRect		rect;
    
	// Get focused item of the list
    UINT        uSelectedCount = m_ctlListFilters.GetSelectedCount();
    switch(uSelectedCount)
    {
    case 0:     // No item selected
        uSelItem = -1;
        uFlagUp = uFlagDown = uFlagProp = uFlagDel = MF_GRAYED;
        break;
    default:    // More than one item selected
        // More than one item selected. Gray "Edit" menu item
        uFlagProp = MF_GRAYED;
        // NOTE: !!! NO "break" !!!
        //       !!! PROCESSING INTENTIONALLY CONTINUES INTO "case 1"
    case 1:     // 1 item selected
        // Retrieve index of first selected item for possible use later.
        POSITION pos = m_ctlListFilters.GetFirstSelectedItemPosition();
        ASSERT(pos);
        uSelItem = m_ctlListFilters.GetNextSelectedItem(pos);
        // May gray move up/down menu entries.
        if (!m_ctlButtonMoveUp.IsWindowEnabled())
            uFlagUp = MF_GRAYED;
        if (!m_ctlButtonMoveDown.IsWindowEnabled())
            uFlagDown = MF_GRAYED;
        break;
    }
	// No item in the list
	if (!m_ctlListFilters.GetItemCount())
		uFlagDelAll = MF_GRAYED;

	// If used keyboard to open context menu, find point
	if (point.x < 0 && point.y < 0)
	{
		if (uSelItem == -1)	// Get control position
			m_ctlListFilters.GetWindowRect(rect);
		else				// Get selected item position
		{
			VERIFY( m_ctlListFilters.GetItemRect(uSelItem, rect, LVIR_BOUNDS) );
			m_ctlListFilters.ClientToScreen(rect);
		}
		point = rect.CenterPoint();
	}

	// Create menu bitmaps
	VERIFY( bmpAdd.LoadBitmap(IDB_ADDFILTER) );
	VERIFY( bmpUp.LoadBitmap(IDB_UP) );
	VERIFY( bmpDown.LoadBitmap(IDB_DOWN) );
	VERIFY( bmpDel.LoadBitmap(IDB_DEL) );
	VERIFY( bmpDelAll.LoadBitmap(IDB_DELALL) );
	VERIFY( bmpProp.LoadBitmap(IDB_PROP) );

	// Create a context menu
	VERIFY( menu.CreatePopupMenu() );
	str.LoadString(IDS_MENU_ADD_FILTER);
	VERIFY( menu.AppendMenu(MF_STRING, 10, str, &bmpAdd) );

	VERIFY( menu.AppendMenu(MF_SEPARATOR) );

	str.LoadString(IDS_MENU_MOVE_UP);
	VERIFY( menu.AppendMenu(MF_STRING | uFlagUp, 20, str, &bmpUp) );
	str.LoadString(IDS_MENU_MOVE_DOWN);
	VERIFY( menu.AppendMenu(MF_STRING | uFlagDown, 30, str, &bmpDown) );

	VERIFY( menu.AppendMenu(MF_SEPARATOR) );

	str.LoadString(IDS_MENU_REMOVE);
	VERIFY( menu.AppendMenu(MF_STRING | uFlagDel, 40, str, &bmpDel) );
	str.LoadString(IDS_MENU_REMOVE_ALL);
	VERIFY( menu.AppendMenu(MF_STRING | uFlagDelAll, 45, str, &bmpDelAll) );

	VERIFY( menu.AppendMenu(MF_SEPARATOR) );

	str.LoadString(IDS_MENU_PROPERTIES);
	VERIFY( menu.AppendMenu(MF_STRING | uFlagProp, 50, str, &bmpProp) );
	VERIFY( menu.SetDefaultItem(50) );
	str.LoadString(IDS_FILTERS);
	VERIFY( menu.SetMenuTitle(str) );

	// Show and track the menu
	dwSelectionMade = menu.TrackPopupMenu(
		TPM_LEFTALIGN|TPM_LEFTBUTTON|TPM_NONOTIFY|TPM_RETURNCMD,
		point.x,
		point.y,
		this);

	// Execute command
	switch (dwSelectionMade)
	{
	case 10:	// Add filter
		OnButtonAddRenamer();
		break;
	case 20:	// Move up
        OnButtonMoveup();
		break;
	case 30:	// Move down
        OnButtonMovedown();
		break;
	case 40:	// Remove
		OnButtonRemoverule();
		break;
	case 45:	// Remove all
		OnButtonClearrule();
		break;
	case 50:	// Edit
		EditRule(uSelItem);
		break;
	}

	// Clean memory
	if (bmpAdd.m_hObject)
		bmpAdd.DeleteObject();
	if (bmpUp.m_hObject)
		bmpUp.DeleteObject();
	if (bmpDown.m_hObject)
		bmpDown.DeleteObject();
	if (bmpDel.m_hObject)
		bmpDel.DeleteObject();
	if (bmpDelAll.m_hObject)
		bmpDelAll.DeleteObject();
	if (bmpProp.m_hObject)
		bmpProp.DeleteObject();
	return TRUE;
}

BOOL CRenameItDlg::EditRule(int nRuleItem)
{
	// Show filter dialog
	boost::scoped_ptr<Filter::IPreviewFileList> previewSamples(GetPreviewSamples(nRuleItem));
	shared_ptr<Filter::IFilter> filter = m_fcFilters.GetFilterAt(nRuleItem);
	ASSERT(filter.get() != NULL);
	if (filter->ShowDialog(*previewSamples) == IDOK)
	{
		// Freeze the updates.
		PushUpdatesFreeze();

		// Change the filter.
		m_fcFilters.UpdateFilter(nRuleItem, filter.get());

		// Un-freeze the updates.
		PopUpdatesFreeze();
		return TRUE;
	}
	else
		return FALSE;
}

BOOL CRenameItDlg::OnContextMenuFilenames(CPoint point)
{
	CNewMenu	menu;
	CString		str;
	DWORD		dwSelectionMade;
	POSITION	pos;
	CBitmap		bmpAddFolder,
				bmpAddFiles,
				bmpDel,
				bmpDelAll,
				bmpSelectAll,
				bmpUnselectAll,
				bmpInverseSelection;
	UINT		uSelItem,
				uFlagDel = 0,
				uFlagDelAll = 0;
	CRect		rect;
    
	// Get focused item of the list
	switch (m_ctlListFilenames.GetSelectedCount())
	{
	case 0:		// No item selected
		uSelItem = -1;
		uFlagDel = MF_GRAYED;
		break;
	default:	// More than one item selected
		// Get first selected item index
		pos = m_ctlListFilenames.GetFirstSelectedItemPosition();
		uSelItem = m_ctlListFilenames.GetNextSelectedItem( pos );
	}
	// No item in the list
	if (!m_ctlListFilenames.GetItemCount())
		uFlagDelAll = MF_GRAYED;

	// If used keyboard to open context menu, find point
	if (point.x < 0 && point.y < 0)
	{
		if (uSelItem == -1)	// Get control position
			m_ctlListFilenames.GetWindowRect(rect);
		else				// Get selected item position
		{
			VERIFY( m_ctlListFilenames.GetItemRect(uSelItem, rect, LVIR_BOUNDS) );
			m_ctlListFilenames.ClientToScreen(rect);
		}
		point = rect.CenterPoint();
	}

	// Create menu bitmaps
	VERIFY( bmpAddFolder.LoadBitmap(IDB_ADDFOLDER) );
	VERIFY( bmpAddFiles.LoadBitmap(IDB_ADDFILES) );
	VERIFY( bmpDel.LoadBitmap(IDB_DEL) );
	VERIFY( bmpDelAll.LoadBitmap(IDB_DELALL) );
	VERIFY( bmpSelectAll.LoadBitmap(IDB_SELECT_ALL) );
	VERIFY( bmpUnselectAll.LoadBitmap(IDB_UNSELECT_ALL) );
	VERIFY( bmpInverseSelection.LoadBitmap(IDB_INVERSE_SELECTION) );

	// Create a context menu
	VERIFY( menu.CreatePopupMenu() );
	str.LoadString(IDS_MENU_ADD_FILES);
	VERIFY( menu.AppendMenu(MF_STRING, 10, str, &bmpAddFiles) );
	str.LoadString(IDS_MENU_ADD_FOLDER);
	VERIFY( menu.AppendMenu(MF_STRING, 15, str, &bmpAddFolder) );

	VERIFY( menu.AppendMenu(MF_SEPARATOR) );

	str.LoadString(IDS_MENU_SELECT_ALL);
	VERIFY( menu.AppendMenu(MF_STRING | uFlagDelAll, 60, str, &bmpSelectAll) );
	str.LoadString(IDS_MENU_SELECT_NONE);
	VERIFY( menu.AppendMenu(MF_STRING | uFlagDelAll, 70, str, &bmpUnselectAll) );
	str.LoadString(IDS_MENU_SELECT_INVERT);
	VERIFY( menu.AppendMenu(MF_STRING | uFlagDelAll, 80, str, &bmpInverseSelection) );

	VERIFY( menu.AppendMenu(MF_SEPARATOR) );

	str.LoadString(IDS_MENU_REMOVE);
	VERIFY( menu.AppendMenu(MF_STRING | uFlagDel, 40, str, &bmpDel) );
	str.LoadString(IDS_MENU_REMOVE_ALL);
	VERIFY( menu.AppendMenu(MF_STRING | uFlagDelAll, 50, str, &bmpDelAll) );
	str.LoadString(IDS_MENU_RENAME);
	VERIFY( menu.AppendMenu(MF_STRING | uFlagDel, 90, str, NULL) );

	str.LoadString(IDS_FILES);
	VERIFY( menu.SetMenuTitle(str) );

	// Show and track the menu
	dwSelectionMade = menu.TrackPopupMenu(
		TPM_LEFTALIGN|TPM_LEFTBUTTON|TPM_NONOTIFY|TPM_RETURNCMD,
		point.x,
		point.y,
		this);

	// Execute command
	switch (dwSelectionMade)
	{
	case 10:	// Add files
		OnButtonAddfile();
		break;
	case 15:	// Add folders
		OnButtonAddfolder();
		break;
	case 60:	// Select All
		SelectAllFiles();
		break;
	case 70:	// Unselect All
		UnselectAllFiles();
		break;
	case 80:	// Inverse Selection
		InverseFileSelection();
		break;
	case 40:	// Remove
		OnButtonRemovefile();
		break;
	case 50:	// Remove all
		OnButtonClearfiles();
		break;
	case 90:	// Rename
		if (m_ctlListFilenames.GetSelectionMark() != -1)
			m_ctlListFilenames.EditLabel( m_ctlListFilenames.GetSelectionMark() );
		break;
	}

	// Clean memory
	if (bmpAddFolder.m_hObject)
		bmpAddFolder.DeleteObject();
	if (bmpAddFiles.m_hObject)
		bmpAddFiles.DeleteObject();
	if (bmpDel.m_hObject)
		bmpDel.DeleteObject();
	if (bmpDelAll.m_hObject)
		bmpDelAll.DeleteObject();
	return TRUE;
}

void CRenameItDlg::OnLvnInsertitemRulesList(NMHDR *pNMHDR, LRESULT *pResult)
{
	LPNMLISTVIEW pNMLV = reinterpret_cast<LPNMLISTVIEW>(pNMHDR);
	GetDlgItem(IDOK)->EnableWindow( m_ctlListFilenames.GetItemCount()>0 );
	*pResult = 0;
}

void CRenameItDlg::OnLvnDeleteitemRulesList(NMHDR *pNMHDR, LRESULT *pResult)
{
	LPNMLISTVIEW pNMLV = reinterpret_cast<LPNMLISTVIEW>(pNMHDR);
	GetDlgItem(IDOK)->EnableWindow( m_ctlListFilenames.GetItemCount()>0 && m_ctlListFilters.GetItemCount()-1>0 );
	UpdateMoveButtonState();
	*pResult = 0;
}

void CRenameItDlg::OnButtonMovedown()
{
	// Walk filter list from end to beginning, moving selected item's down 1 slot.
	// After the filter list has been updated, reapply the filters to the file list.

	UINT idxLastItem   = m_ctlListFilters.GetItemCount() - 1;
	ASSERT(idxLastItem > 0);
	UINT selectedCount = m_ctlListFilters.GetSelectedCount();
	ASSERT(selectedCount > 0);
	ASSERT(!IsFilterSelected(idxLastItem));

	// Freeze the updates.
	PushUpdatesFreeze();

	INT  idxVisibleItem = -1;
	UINT idxItem = idxLastItem;
	while (idxItem-- > 0)
	{
		if (IsFilterSelected(idxItem))
		{
			SwapFilterItems(idxItem, idxItem+1);
			if (idxVisibleItem < 0)
				idxVisibleItem = (((idxItem+1) < idxLastItem)?(idxItem + 2):idxLastItem);
			// exit list early when there are no more selected items to process
			if (--selectedCount == 0)
				break;
		}
	}

	// Un-freeze the updates.
	PopUpdatesFreeze();

	// Scroll filter list to ensure bottom-most selected item remains visible.
	m_ctlListFilters.EnsureVisible( idxVisibleItem, true);
}

void CRenameItDlg::OnButtonMoveup()
{
	// Walk filter list from beginning to end, moving selected item's up 1 slot.
	// After the filter list has been updated, reapply the filters to the file list.

	UINT idxLastItem   = m_ctlListFilters.GetItemCount() - 1;
	ASSERT(idxLastItem > 0);
	UINT selectedCount = m_ctlListFilters.GetSelectedCount();
	ASSERT(selectedCount > 0);
	ASSERT(!IsFilterSelected(0));

	// Freeze the updates.
	PushUpdatesFreeze();

	INT  idxVisibleItem = -1;
	UINT idxItem = 0;
	while (idxItem++ < idxLastItem)
	{
		if (IsFilterSelected(idxItem))
		{
			SwapFilterItems(idxItem-1, idxItem);
			if (idxVisibleItem < 0)
				idxVisibleItem = ((idxItem > 1)?(idxItem - 2):0);
			// exit list early when there are no more selected items to process
			if (--selectedCount == 0)
				break;
		}
	}

	// Un-freeze the updates.
	PopUpdatesFreeze();

	// scroll filter list to ensure top-most selected item remains visible.
	m_ctlListFilters.EnsureVisible( idxVisibleItem, true);
}

void CRenameItDlg::OnItemchangedRulesList(NMHDR* pNMHDR, LRESULT* pResult)
{
    NM_LISTVIEW* pNMListView = (NM_LISTVIEW*)pNMHDR;

    // Whenever the selection state of an entry in the filter list changes,
    // enable or disable the filter Move buttons if appropriate

    // Process only list item selection state changes.
    if (pNMListView->uChanged & LVIF_STATE)
    {
        UINT wasSelected = (pNMListView->uOldState & LVIS_SELECTED);
        UINT isSelected  = (pNMListView->uNewState & LVIS_SELECTED);
        if (wasSelected != isSelected)
        {
            // TRACE1("Filter item %3d selection change.\n", pNMListView->iItem);
            UpdateMoveButtonState();
        }
    }

    *pResult = 0;
}

void CRenameItDlg::UpdateMoveButtonState()
{
    UINT idxLastItem = m_ctlListFilters.GetItemCount() - 1;
    if ((idxLastItem == 0) ||
        (m_ctlListFilters.GetSelectedCount() == 0))
    {
        // Only 1 item in list or no items current selected.
        // Can't move anything anywhere anyhow. Disable both move buttons.
        m_ctlButtonMoveDown.EnableWindow(false);
        m_ctlButtonMoveUp.EnableWindow(false);
    }
    else
    {
        // One or more items selected.
        // Enable Move Up if and only if first list item NOT selected.
        bool bIsEnabled =
            (m_ctlListFilters.GetItemState(0, LVIS_SELECTED) == 0);
        m_ctlButtonMoveUp.EnableWindow(bIsEnabled);

        // Enable Move Down if and only if last list item NOT selected.
        bIsEnabled = (m_ctlListFilters.GetItemState(idxLastItem, LVIS_SELECTED) == 0);
        m_ctlButtonMoveDown.EnableWindow(bIsEnabled);
    }
}

void CRenameItDlg::OnViewAlwaysOnTop()
{
   // Get the popup menu which contains the "Test" menu item.
   CMenu* pmenu = GetMenu();

   // Check the state of menu item. Check the menu item
   // if it is currently unchecked. Otherwise, uncheck the menu item
   // if it is not currently checked.
   UINT state = pmenu->GetMenuState(ID_VIEW_ALWAYS_ON_TOP, MF_BYCOMMAND);
   ASSERT(state != 0xFFFFFFFF);

   if (state & MF_CHECKED)
   {
      // Current status CHECKED ... toggle to UNCHECKED. Let window fall to bottom.
      pmenu->CheckMenuItem(ID_VIEW_ALWAYS_ON_TOP, MF_UNCHECKED | MF_BYCOMMAND);
      GetParentOwner()->
        SetWindowPos( &wndNoTopMost, 0, 0, 0, 0, SWP_NOMOVE|SWP_NOSIZE );
   }
   else
   {
      // Current status UNCHECKED ... toggle to CHECKED. Force window always on top.
      pmenu->CheckMenuItem(ID_VIEW_ALWAYS_ON_TOP, MF_CHECKED | MF_BYCOMMAND);
      GetParentOwner()->
        SetWindowPos( &wndTopMost, 0, 0, 0, 0, SWP_NOMOVE|SWP_NOSIZE );
   }

}

void CRenameItDlg::OnLvnEndlabeleditFilenamesIn(NMHDR *pNMHDR, LRESULT *pResult)
{
	NMLVDISPINFO *pDispInfo = reinterpret_cast<NMLVDISPINFO*>(pNMHDR);

	// We suppose that the renaming is rejected.
	*pResult = FALSE;

	// Manual renaming of the file.
	if (pDispInfo->item.pszText != NULL)
	{
		// Which list to use: Files or Directories?
		Gui::CMemoryFileList& mflItems = GetDisplayedList();

		// Get new file path.
		CPath fnBefore = mflItems.GetIteratorAt(pDispInfo->item.iItem)->fnBefore;
		CFilteredPath fnNewBefore = CFilteredPath(fnBefore, m_fcFilters.GetPathRenamePart());
		fnNewBefore.SetFilteredSubstring(pDispInfo->item.pszText);
		
		// Rename file.
		if (MoveFile(
				CPath::MakeUnicodePath(fnBefore.GetPath()),
				CPath::MakeUnicodePath(fnNewBefore.GetFilteredPath())
			))
		{
			// Freeze the updates.
			PushUpdatesFreeze();

			// Set the new name.
			mflItems.GetIteratorAt(pDispInfo->item.iItem)->fnBefore = fnNewBefore;

			// Un-freeze the updates.
			PopUpdatesFreeze();

			// Accept the renaming.
			*pResult = TRUE;
		}
		else
		{// Show the error.
			LPVOID lpMsgBuf = NULL;

			DWORD dwError = GetLastError();
			if (dwError)
			{
				FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
					NULL,
					dwError,
					MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
					(LPTSTR) &lpMsgBuf,
					0,
					NULL );
			}

			if (lpMsgBuf)
			{
				// Display the string.
				AfxMessageBox((LPCTSTR)lpMsgBuf, MB_ICONSTOP);

				// Free the buffer.
				LocalFree(lpMsgBuf);
			}
			else
				AfxMessageBox(IDS_MANUAL_RENAMING_ERROR, MB_ICONSTOP);
		}
	}
}

void CRenameItDlg::SelectAllFiles()
{
	// Freeze the updates.
	PushUpdatesFreeze();

	// Check all files.
	BOOST_FOREACH(Gui::CMemoryFileList::ITEM& item, m_flFiles)
		item.bChecked = true;
	BOOST_FOREACH(Gui::CMemoryFileList::ITEM& item, m_flDirectories)
		item.bChecked = true;

	// Un-freeze the updates.
	PopUpdatesFreeze();
}

void CRenameItDlg::UnselectAllFiles()
{
	// Freeze the updates.
	PushUpdatesFreeze();

	// Un-check all files.
	BOOST_FOREACH(Gui::CMemoryFileList::ITEM& item, m_flFiles)
		item.bChecked = false;
	BOOST_FOREACH(Gui::CMemoryFileList::ITEM& item, m_flDirectories)
		item.bChecked = false;

	// Un-freeze the updates.
	PopUpdatesFreeze();
}

void CRenameItDlg::InverseFileSelection()
{
	// Freeze the updates.
	PushUpdatesFreeze();

	// Which list to use: Files or Directories?
	Gui::CMemoryFileList& mflItems = GetDisplayedList();

	// Toggle check all files.
	BOOST_FOREACH(Gui::CMemoryFileList::ITEM& item, mflItems)
		item.bChecked = !item.bChecked;

	// Un-freeze the updates.
	PopUpdatesFreeze();
}

bool CRenameItDlg::AddFilter(Filter::IFilter *pFilter)
{
	UINT idxLastItem = m_ctlListFilters.GetItemCount();

	// Load filter with default arguments
	pFilter->LoadDefaultArgs();
	m_fcFilters.AddFilter(pFilter);

	// Edit filter (show filter's config dialog)
	if (!EditRule(idxLastItem))
	{// If user cancelled, remove filter
		m_fcFilters.RemoveFilter(idxLastItem);
		return false;
	}

	// If user validated, update filter list on screen...

	// Freeze the updates.
	PushUpdatesFreeze();

	// Scroll filter list to ensure new/last filter list is visible.
	// Select the new filter to allow user to move it (up) if s/he wants to.
	m_ctlListFilters.SetItemState( idxLastItem,
									(LVIS_SELECTED | LVIS_FOCUSED),
									(LVIS_SELECTED | LVIS_FOCUSED));
	m_ctlListFilters.EnsureVisible( idxLastItem, false);

	// Un-freeze the updates.
	PopUpdatesFreeze();
	return true;
}

void CRenameItDlg::OnMovedItemFileNamesIn(NMHDR *pNMHDR, LRESULT *pResult)
{
	NMELMOVEITEM *pMoveItem = reinterpret_cast<NMELMOVEITEM*>(pNMHDR);

	// Move the item in the hidden list.
	if (pMoveItem->nTo != pMoveItem->nFrom)
	{
		// Freeze the updates.
		PushUpdatesFreeze();

		// Which list to use: Files or Directories?
		Gui::CMemoryFileList& mflItems = GetDisplayedList();

		// Reoder items.
		Gui::CMemoryFileList::iterator iterFrom = mflItems.GetIteratorAt(pMoveItem->nFrom);
		mflItems.ReorderFiles(
			mflItems.GetIteratorAt(pMoveItem->nTo), 
			iterFrom,
			iterFrom);

		// Un-freeze the updates.
		PopUpdatesFreeze();
	}

	*pResult = 0;
}

void CRenameItDlg::OnFeedbackBugreport()
{
	ShellExecute(GetSafeHwnd(), _T("open"), _T("http://www.beroux.com/?id=47"), NULL, NULL, SW_SHOWNORMAL);
}

void CRenameItDlg::OnFeedbackInformationrequest()
{
	ShellExecute(GetSafeHwnd(), _T("open"), _T("http://www.beroux.com/?id=57;subject=1"), NULL, NULL, SW_SHOWNORMAL);
}

void CRenameItDlg::OnFeedbackSuggestions()
{
	ShellExecute(GetSafeHwnd(), _T("open"), _T("http://www.beroux.com/?id=57;subject=2"), NULL, NULL, SW_SHOWNORMAL);
}

void CRenameItDlg::OnRenamePartSelectionChange(NMHDR *pNMHDR, LRESULT *pResult)
{
	// Freeze the updates.
	PushUpdatesFreeze();

	// Set what part of the path to rename
	m_fcFilters.SetPathRenamePart( m_ctrlRenamePart.GetRenameParts() );

	// Save
	CSettingsDlg	config;
	config.SetType( m_ctrlRenamePart.GetRenameParts() );
	config.SaveConfig();

	// Un-freeze the updates.
	PopUpdatesFreeze();

	*pResult = 0;
}

BOOL CRenameItDlg::PreTranslateMessage(MSG* pMsg)
{
	if (NULL != m_pToolTip)
		m_pToolTip->RelayEvent(pMsg);

	// Shift+F1
	if (pMsg->message == 0x004D && GetKeyState(VK_SHIFT) < 0)
	{
		OnContextHelp();
		return(TRUE);
	}

	return CSizingDialog::PreTranslateMessage(pMsg);
}

void CRenameItDlg::OnSize(UINT nType, int cx, int cy)
{
	CSizingDialog::OnSize(nType, cx, cy);

	// Resize columns...
	if (m_bDialogInit)
	{
		RECT rect;
		int nCtrlWidth;
		const int nScrollBarWidth = 30;

		// Filters list control columns
		m_ctlListFilters.GetWindowRect(&rect);
		nCtrlWidth = rect.right - rect.left - nScrollBarWidth;
		m_ctlListFilters.SetColumnWidth(0, nCtrlWidth);

		// Preview list control columns
		m_ctlListFilenames.GetWindowRect(&rect);
		nCtrlWidth = rect.right - rect.left - nScrollBarWidth;
		int nColumnsTotalWidth = m_ctlListFilenames.GetColumnWidth(0) + m_ctlListFilenames.GetColumnWidth(1);
		float fRatio0 = (float) m_ctlListFilenames.GetColumnWidth(0) / nColumnsTotalWidth;
		m_ctlListFilenames.SetColumnWidth(0, (int)(nCtrlWidth*fRatio0 + 0.5f));
		m_ctlListFilenames.SetColumnWidth(1, nCtrlWidth - (int)(nCtrlWidth*fRatio0 + 0.5f));
	}
}

void CRenameItDlg::OnContextHelp()
{
	SendMessage(WM_SYSCOMMAND, SC_CONTEXTHELP);
}

bool CRenameItDlg::RenameAllFiles(unsigned nRenamingControllerErrorLevel)
{
	// Which list to use: Files or Directories?
	Gui::CMemoryFileList& mflItems = GetDisplayedList();

	// Pre-condition.
	ASSERT(mflItems.GetFileCount() == m_ctlListFilenames.GetItemCount());

	// Filter all the checked files (this is optional since it's kept up to date by UpdateFilelist).
	m_fcFilters.FilterFileNames(
		mflItems.GetInputIteratorAt(mflItems.GetFirstChecked()),	// Begin
		mflItems.GetInputIteratorAt(mflItems.GetFirstChecked()),	// First
		mflItems.GetInputIteratorAt(mflItems.GetTail()),	// Last
		mflItems.GetOutputIteratorAt(mflItems.GetFirstChecked()));	// Result

	// Generate the file list of original and new file names.
	CFileList flBefore;
	CFileList flAfter;
	BOOST_FOREACH(Gui::CMemoryFileList::ITEM& item, mflItems)
	{
		if (item.bChecked)
		{
			flBefore.AddPath(item.fnBefore);
			flAfter.AddPath(item.fnAfter);
		}
	}
	ASSERT(flBefore.GetCount() == flAfter.GetCount());

	// Do the renaming.
	CRenamingController	renaming;
	renaming.SetErrorLevel(nRenamingControllerErrorLevel);
	if (renaming.RenameFiles(flBefore, flAfter))
	{
		// Clear file list
		OnButtonClearfiles();
		return true;
	}
	else
		return false;
}

void CRenameItDlg::OnLvnGetdispinfoFilenamesIn(NMHDR *pNMHDR, LRESULT *pResult)
{
	NMLVDISPINFO *pDispInfo = reinterpret_cast<NMLVDISPINFO*>(pNMHDR);
	
	// Create a pointer to the item
	LV_ITEM* pItem= &(pDispInfo)->item;

	// Which list to use: Files or Directories?
	Gui::CMemoryFileList& mflItems = GetDisplayedList();

	// Which item number?
	int itemid = pItem->iItem;
	if (itemid >= mflItems.GetFileCount())	// There should never be a call to an un-existing item.
	{
		ASSERT(FALSE);
		*pResult = 0;
		return;
	}

	// Do the list need text information?
	if (pItem->mask & LVIF_TEXT)
	{
		// Which column?
		CPath fnToRename;
		if (pItem->iSubItem == 0)
			fnToRename = mflItems.GetIteratorAt(itemid)->fnBefore;
		else
			fnToRename = mflItems.GetIteratorAt(itemid)->fnAfter;
		CString strText = CFilteredPath(fnToRename, m_fcFilters.GetPathRenamePart()).GetFilteredSubstring();

		// Copy the text to the LV_ITEM structure
		// Maximum number of characters is in pItem->cchTextMax
		lstrcpyn(pItem->pszText, strText, pItem->cchTextMax);
	}

	// Do the list need image information?
	if (pItem->mask & LVIF_IMAGE)
	{
		// Set which image to use
//		pItem->iImage = m_database[itemid].m_image;

		// Show check box.
		{
			// To enable check box, we have to enable state mask...
			pItem->mask |= LVIF_STATE;
			pItem->stateMask = LVIS_STATEIMAGEMASK;

			if (mflItems.GetIteratorAt(itemid)->bChecked)
			{
				// Turn check box on
				pItem->state = INDEXTOSTATEIMAGEMASK(2);
			}
			else
			{
				// Turn check box off
				pItem->state = INDEXTOSTATEIMAGEMASK(1);
			}
		}
	}

	*pResult = 0;
}

/**
 * Used to move the selection to the item starting by a specific string.
 * Example: In the Explorer, typing PRO will move the selection to the next
 * file/folder name starting by PRO (like "Program files").
 */
void CRenameItDlg::OnLvnOdfinditemFilenamesIn(NMHDR *pNMHDR, LRESULT *pResult)
{
	LPNMLVFINDITEM pFindInfo = reinterpret_cast<LPNMLVFINDITEM>(pNMHDR);

	/* pFindInfo->iStart is from which item we should search.
	 * We search to bottom, and then restart at top and will stop
	 * at pFindInfo->iStart, unless we find an item that match
	 */

	// Set the default return value to -1
	// That means we didn't find any match.
	*pResult = -1;

	// Is search NOT based on string?
	if( (pFindInfo->lvfi.flags & LVFI_STRING) == 0 )
	{
		// This will probably never happend...
		return;
	}

	// Which list to use: Files or Directories?
	Gui::CMemoryFileList& mflItems = GetDisplayedList();

	// This is the string we search for.
	CString strSearch = pFindInfo->lvfi.psz;

	int startPos = pFindInfo->iStart;
	// Is startPos outside the list (happens if last item is selected).
	if (startPos >= mflItems.GetFileCount())
		startPos = 0;
	Gui::CMemoryFileList::iterator iterStart = mflItems.GetIteratorAt(startPos);

	// Let's search...
	Gui::CMemoryFileList::iterator iter = iterStart;
	do
	{        
		// Do this word begins with all characters in strSearch?
		if (iter->fnBefore.FSCompare(strSearch, strSearch.GetLength()) == 0)
		{
			// Select this item and stop search.
			*pResult = mflItems.FindIndexOf(iter);
			break;
		}

		// Go to next item.
		++iter;

		// Need to restart at top?
		if (iter == mflItems.GetTail())
			iter = mflItems.GetHead();

		// Stop if back to start.
	} while (iter != iterStart);
}

void CRenameItDlg::ToggleCheckBox(int nItem)
{
	// Freeze the updates.
	PushUpdatesFreeze();

	// Change check box
	Gui::CMemoryFileList::iterator iter = GetDisplayedList().GetIteratorAt(nItem);
	iter->bChecked = !iter->bChecked;

 	// Un-freeze the updates.
	PopUpdatesFreeze();
}

void CRenameItDlg::OnLvnKeydownFilenamesIn(NMHDR *pNMHDR, LRESULT *pResult)
{
	LPNMLVKEYDOWN pLVKeyDown = reinterpret_cast<LPNMLVKEYDOWN>(pNMHDR);

	switch (pLVKeyDown->wVKey)
	{
	// DEL to remove the file.
	case VK_DELETE:
		OnButtonRemovefile();
		break;

	// F2 to rename manually.
	case VK_F2:
		// Rename manually the focused item.
		{
			int nFocusedItem = m_ctlListFilenames.GetNextItem(-1, LVNI_FOCUSED);
			if (nFocusedItem >= 0)
				m_ctlListFilenames.EditLabel(nFocusedItem);
		}
		break;

	// F5 refresh the list.
	case VK_F5:
		// Force the updates.
		m_nUpdatesFreeze = 0;
		PushUpdatesFreeze();
		PopUpdatesFreeze();
		break;

	// If user press space, toggle flag on selected item
	case VK_SPACE:
		// Toggle checked for the focused item.
		{
			int nFocusedItem = m_ctlListFilenames.GetNextItem(-1, LVNI_FOCUSED);
			if (nFocusedItem >= 0)
				ToggleCheckBox(nFocusedItem);
		}
		break;
	}

	*pResult = 0;
}

void CRenameItDlg::OnNMClickFilenamesIn(NMHDR *pNMHDR, LRESULT *pResult)
{
	NMLISTVIEW* pNMListView = (NM_LISTVIEW*)pNMHDR;

	LVHITTESTINFO hitinfo;
	// Copy click point
	hitinfo.pt = pNMListView->ptAction;

	// Make the hit test...
	int item = m_ctlListFilenames.HitTest(&hitinfo); 

	if (item != -1)
	{
		// We hit one item... did we hit state image (check box)?
		// This test only works if we are in list or report mode.
		if ((hitinfo.flags & LVHT_ONITEMSTATEICON) != 0)
			ToggleCheckBox(item);
	}

	*pResult = 0;
}

IPreviewFileList* CRenameItDlg::GetPreviewSamples(int nFilterIndex)
{
	ASSERT(0 <= nFilterIndex && nFilterIndex <= m_fcFilters.GetFilterCount());

	// Which list to use: Files or Directories?
	Gui::CMemoryFileList& mflItems = GetDisplayedList();

	// Find the default sample to use.
	Gui::CMemoryFileList::const_iterator iterDefault;
	int nFocusedItem = m_ctlListFilenames.GetNextItem(-1, LVNI_FOCUSED);
	if (nFocusedItem >= 0)
		// Use the focused file.
		iterDefault = mflItems.GetIteratorAt(nFocusedItem);
	else
		// By default use the first file.
		iterDefault = mflItems.GetHead();

	// If the focused item is not checked, find the next checked item.
	while (iterDefault != mflItems.GetTail() && !iterDefault->bChecked)
		++iterDefault;
	if (iterDefault == mflItems.GetTail())
		iterDefault = mflItems.GetFirstChecked();

	// When the filter to preview is not the last filter,
	// then we create a filter container will all the filter before that filter.
	CFilterContainer fcFilters = m_fcFilters;
	while (fcFilters.GetFilterCount() > nFilterIndex)
		fcFilters.RemoveFilter(fcFilters.GetFilterCount() - 1);

	// When no item is checked (or when the list is empty),
	// use a default sample.
	if (iterDefault == mflItems.GetTail())
	{
		// Load sample path is none is provided.
		CString strSamplePath;
		strSamplePath.LoadString(IDS_SAMPLE_PATH);
		static CPath fnSamplePath = strSamplePath;
		
		// Return the created object.
		return new CPreviewFileList<CPath*>(
			&fnSamplePath,		// Begin
			&fnSamplePath,		// First
			&fnSamplePath + 1,	// Last
			&fnSamplePath,		// Default sample
			fcFilters);		// Filters BEFORE the new coming one
	}
	else
	{
		// Return the created object.
		return new CPreviewFileList<Gui::CMemoryFileList::InputIterator>(
			mflItems.GetInputIteratorAt(mflItems.GetFirstChecked()),	// Begin
			mflItems.GetInputIteratorAt(mflItems.GetFirstChecked()),	// First
			mflItems.GetInputIteratorAt(mflItems.GetTail()),			// Last
			mflItems.GetInputIteratorAt(iterDefault),		// Default sample
			fcFilters);			// Filters BEFORE the new coming one
	}
}

Gui::CMemoryFileList& CRenameItDlg::GetDisplayedList()
{
	// Which list to use: Files or Directories?
	if (m_ctrlRenamePart.GetRenameParts() & (CFilteredPath::renameFilename | CFilteredPath::renameExtension))
		return m_flFiles;
	else
		return m_flDirectories;
}
