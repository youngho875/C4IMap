#include "pch.h"
#include "ModelessDialog.h"
#include <afxcmn.h>
#include <shlwapi.h>
#include <vector>
#pragma comment(lib, "shlwapi.lib")
BEGIN_MESSAGE_MAP(CModelessDialog, CDialogEx)
    ON_WM_CLOSE()
END_MESSAGE_MAP()
void CModelessDialog::OnClose() { OnCancel(); }
CModelessDialog::CModelessDialog(UINT resource, CWnd* parent) : CDialogEx(resource, parent) {}
bool CModelessDialog::OpenTemplate(const DLGTEMPLATE* layout, CWnd* parent, std::function<void()> accepted)
{
    ownerWindow = parent ? parent->GetSafeHwnd() : nullptr;
    completion = std::move(accepted);
    if (!CreateIndirect(layout, parent)) { delete this; return false; }
    selfOwned = true;
    ShowWindow(SW_SHOW);
    return true;
}
bool CModelessDialog::OpenResource(UINT resource, CWnd* parent, std::function<void()> accepted)
{
    ownerWindow = parent ? parent->GetSafeHwnd() : nullptr;
    completion = std::move(accepted);
    if (!Create(resource, parent)) { delete this; return false; }
    selfOwned = true;
    ShowWindow(SW_SHOW);
    return true;
}
void CModelessDialog::OnOK()
{
    // The callback can close the owning view (and this owned window).
    const HWND window = GetSafeHwnd();
    auto callback = completion;
    if (callback) callback();
    if (::IsWindow(window)) ::DestroyWindow(window);
}
void CModelessDialog::OnCancel() { DestroyWindow(); }
void CModelessDialog::PostNcDestroy()
{
    if (::IsWindow(ownerWindow)) ::PostMessage(ownerWindow, WM_C4_MODELESS_CHANGED, 0, 0);
    const bool release = selfOwned;
    CDialogEx::PostNcDestroy();
    if (release) delete this;
}
namespace
{
#pragma pack(push, 2)
struct EmptyLayout
{
    DLGTEMPLATE dialog{};
    WORD menu = 0, windowClass = 0, title = 0;
};
#pragma pack(pop)
class PathDialog : public CModelessDialog
{
public:
    bool opening = true, folder = false;
    CString initial, filterText;
    std::function<void(CString, UINT)> callback;
    CEdit directory, filename;
    CListCtrl list;
    CComboBox filters;
    CButton up, go, accept, cancel;
    std::vector<CString> patterns;
    std::vector<CString> names;
    std::vector<bool> directories;
    CString Value(CWnd& control) { CString s; control.GetWindowText(s); s.Trim(); return s; }
    void Refresh()
    {
        CString path = Value(directory);
        if (!PathIsDirectory(path)) { AfxMessageBox(_T("폴더 경로를 확인하세요.")); return; }
        list.DeleteAllItems(); names.clear(); directories.clear();
        if (path.Right(1) != _T("\\") && path.Right(1) != _T("/")) path += _T("\\");
        int selected = filters.GetCurSel();
        CString pattern = selected >= 0 && selected < (int)patterns.size() ? patterns[selected] : _T("*.*");
        CFileFind find;
        BOOL more = find.FindFile(path + _T("*"));
        while (more)
        {
            more = find.FindNextFile();
            if (find.IsDots()) continue;
            const bool dir = find.IsDirectory() != FALSE;
            if (!dir && (folder || !PathMatchSpec(find.GetFileName(), pattern))) continue;
            int index = list.InsertItem(list.GetItemCount(), find.GetFileName());
            list.SetItemText(index, 1, dir ? _T("폴더") : _T("파일"));
            names.push_back(find.GetFileName()); directories.push_back(dir);
        }
    }
    BOOL OnInitDialog() override
    {
        CModelessDialog::OnInitDialog();
        SetWindowText(folder ? _T("폴더 선택") : (opening ? _T("파일 열기") : _T("파일 저장")));
        const DWORD child = WS_CHILD | WS_VISIBLE | WS_TABSTOP;
        CRect rc; GetClientRect(&rc); const int width = rc.Width(), height = rc.Height();
        directory.Create(child | WS_BORDER | ES_AUTOHSCROLL, CRect(12,12,width-152,37),this,100);
        up.Create(_T("상위 폴더"),child,CRect(width-145,12,width-75,37),this,101);
        go.Create(_T("이동"),child,CRect(width-70,12,width-12,37),this,102);
        list.Create(child|WS_BORDER|LVS_REPORT|LVS_SINGLESEL,CRect(12,45,width-12,height-105),this,103);
        list.SetExtendedStyle(LVS_EX_FULLROWSELECT|LVS_EX_DOUBLEBUFFER);
        list.InsertColumn(0,_T("이름"),LVCFMT_LEFT,width-155);
        list.InsertColumn(1,_T("종류"),LVCFMT_LEFT,100);
        filename.Create(child|WS_BORDER|ES_AUTOHSCROLL,CRect(12,height-96,width-12,height-71),this,104);
        filters.Create(child|CBS_DROPDOWNLIST,CRect(12,height-64,width-200,height+100),this,105);
        accept.Create(folder?_T("폴더 선택"):(opening?_T("열기"):_T("저장")),child|BS_DEFPUSHBUTTON,
                      CRect(width-185,height-42,width-100,height-12),this,IDOK);
        cancel.Create(_T("취소"),child,CRect(width-90,height-42,width-12,height-12),this,IDCANCEL);
        int position=0;
        while(position>=0)
        {
            CString title=filterText.Tokenize(_T("|"),position);
            if(title.IsEmpty()||position<0) break;
            CString pattern=filterText.Tokenize(_T("|"),position);
            if(pattern.IsEmpty()) break;
            filters.AddString(title); patterns.push_back(pattern);
        }
        if(patterns.empty()){filters.AddString(_T("모든 파일"));patterns.push_back(_T("*.*"));}
        filters.SetCurSel(0);
        CString path=initial;
        if(!PathIsDirectory(path))
        {
            filename.SetWindowText(PathFindFileName(path));
            int split=(std::max)(path.ReverseFind('\\'),path.ReverseFind('/'));
            path=split>=0?path.Left(split+1):CString();
        }
        if(path.IsEmpty()||!PathIsDirectory(path)){TCHAR current[MAX_PATH];GetCurrentDirectory(MAX_PATH,current);path=current;}
        directory.SetWindowText(path);
        if(folder){filename.EnableWindow(FALSE);filters.EnableWindow(FALSE);}
        Refresh(); CenterWindow(); return TRUE;
    }
    BOOL OnCommand(WPARAM wp, LPARAM lp) override
    {
        const int id=LOWORD(wp);
        if(id==101){CString path=Value(directory);PathRemoveBackslash(path.GetBuffer());path.ReleaseBuffer();
            PathRemoveFileSpec(path.GetBuffer());path.ReleaseBuffer();directory.SetWindowText(path);Refresh();return TRUE;}
        if(id==102 || (id==105 && HIWORD(wp)==CBN_SELCHANGE)){Refresh();return TRUE;}
        return CModelessDialog::OnCommand(wp,lp);
    }
    BOOL OnNotify(WPARAM wp, LPARAM lp, LRESULT* result) override
    {
        const NMHDR* header=reinterpret_cast<NMHDR*>(lp);
        if(header->idFrom==103)
        {
            int selected=list.GetNextItem(-1,LVNI_SELECTED);
            if(selected>=0 && selected<(int)names.size())
            {
                if(!directories[selected]) filename.SetWindowText(names[selected]);
                if(header->code==NM_DBLCLK)
                {
                    if(directories[selected]){CString path=Value(directory)+_T("\\")+names[selected];directory.SetWindowText(path);Refresh();}
                    else OnOK();
                    *result=0;return TRUE;
                }
            }
        }
        return CModelessDialog::OnNotify(wp,lp,result);
    }
    void OnOK() override
    {
        CString path=Value(directory);
        if(folder)
        {
            int selected=list.GetNextItem(-1,LVNI_SELECTED);
            if(selected>=0&&selected<(int)names.size()&&directories[selected])path+=_T("\\")+names[selected];
        }
        else
        {
            CString name=Value(filename);
            if(name.IsEmpty()){filename.SetFocus();return;}
            path=PathIsRelative(name)?path+_T("\\")+name:name;
            if(!opening)
            {
                const int index=filters.GetCurSel();
                CString pattern=index>=0?patterns[index]:_T("*.*");
                if(pattern.Left(2)==_T("*.")&&pattern.Find(';')<0&&pattern.Mid(2).Find('*')<0)
                {
                    CString extension=PathFindExtension(path),wanted=pattern.Mid(1);
                    if(extension.CompareNoCase(wanted)!=0)path=path.Left(path.GetLength()-extension.GetLength())+wanted;
                }
                CString parent=path;
                PathRemoveFileSpec(parent.GetBuffer());parent.ReleaseBuffer();
                if(!PathIsDirectory(parent)){AfxMessageBox(_T("저장 폴더가 존재하지 않습니다."));return;}
            }
        }
        if(folder?!PathIsDirectory(path):(opening&&(GetFileAttributes(path)==INVALID_FILE_ATTRIBUTES||PathIsDirectory(path))))
        {AfxMessageBox(_T("선택한 경로가 존재하지 않거나 형식이 맞지 않습니다."));return;}
        if(!folder&&!opening&&GetFileAttributes(path)!=INVALID_FILE_ATTRIBUTES&&
            AfxMessageBox(path+_T("\n덮어쓰시겠습니까?"),MB_YESNO|MB_ICONQUESTION)!=IDYES)return;
        auto complete=callback;const UINT index=(UINT)filters.GetCurSel()+1;
        const HWND window=GetSafeHwnd();
        if(complete)complete(path,index);
        if(::IsWindow(window))::DestroyWindow(window);
    }
};
class ColorDialog : public CModelessDialog
{
public:
    COLORREF value=0;
    std::function<void(COLORREF)> callback;
    CEdit channels[3]; CStatic labels[3]; CButton accept,cancel;
    BOOL OnInitDialog() override
    {
        CModelessDialog::OnInitDialog();SetWindowText(_T("색상 설정 (RGB)"));
        const TCHAR* names[]={_T("빨강 (0~255)"),_T("초록 (0~255)"),_T("파랑 (0~255)")};
        int values[]={GetRValue(value),GetGValue(value),GetBValue(value)};
        for(int i=0;i<3;++i){labels[i].Create(names[i],WS_CHILD|WS_VISIBLE,CRect(12,15+i*35,130,40+i*35),this);
            channels[i].Create(WS_CHILD|WS_VISIBLE|WS_BORDER|WS_TABSTOP|ES_NUMBER,CRect(140,12+i*35,220,38+i*35),this,200+i);
            CString number;number.Format(_T("%d"),values[i]);channels[i].SetWindowText(number);}
        accept.Create(_T("확인"),WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_DEFPUSHBUTTON,CRect(50,125,125,155),this,IDOK);
        cancel.Create(_T("취소"),WS_CHILD|WS_VISIBLE|WS_TABSTOP,CRect(135,125,210,155),this,IDCANCEL);
        CenterWindow();return TRUE;
    }
    void OnOK() override
    {
        int values[3];
        for(int i=0;i<3;++i){CString text;channels[i].GetWindowText(text);TCHAR* end=nullptr;
            long value=_tcstol(text,&end,10);if(text.IsEmpty()||*end||value<0||value>255){channels[i].SetFocus();return;}values[i]=(int)value;}
        auto complete=callback;const HWND window=GetSafeHwnd();
        if(complete)complete(RGB(values[0],values[1],values[2]));
        if(::IsWindow(window))::DestroyWindow(window);
    }
};
}
void ModelessDialogs::File(CWnd* owner,bool open,const CString& initial,const CString& filters,
    std::function<void(CString,UINT)> accepted,bool folder)
{
    auto* dialog=new PathDialog;
    dialog->opening=open;dialog->folder=folder;dialog->initial=initial;dialog->filterText=filters;dialog->callback=std::move(accepted);
    EmptyLayout layout;layout.dialog.style=WS_POPUP|WS_CAPTION|WS_SYSMENU;layout.dialog.cx=440;layout.dialog.cy=290;
    dialog->OpenTemplate(&layout.dialog,owner);
}
void ModelessDialogs::Color(CWnd* owner,COLORREF initial,std::function<void(COLORREF)> accepted)
{
    auto* dialog=new ColorDialog;dialog->value=initial;dialog->callback=std::move(accepted);
    EmptyLayout layout;layout.dialog.style=WS_POPUP|WS_CAPTION|WS_SYSMENU;layout.dialog.cx=165;layout.dialog.cy=115;
    dialog->OpenTemplate(&layout.dialog,owner);
}
