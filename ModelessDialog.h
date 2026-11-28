#pragma once
#include <afxdialogex.h>
#include <functional>
#pragma pack(push, 8)
// Heap-owned UI-thread windows. No nested modal loop or owner disabling.
constexpr UINT WM_C4_MODELESS_CHANGED = WM_APP + 0x3A1;
class CModelessDialog : public CDialogEx
{
public:
    CModelessDialog(UINT resource = 0, CWnd* parent = nullptr);
    bool OpenTemplate(const DLGTEMPLATE* layout, CWnd* parent, std::function<void()> accepted = {});
    bool OpenResource(UINT resource, CWnd* parent, std::function<void()> accepted = {});
protected:
    afx_msg void OnClose();
    DECLARE_MESSAGE_MAP()
    void OnOK() override;
    void OnCancel() override;
    void PostNcDestroy() override;
private:
    HWND ownerWindow = nullptr;
    bool selfOwned = false;
    std::function<void()> completion;
};
namespace ModelessDialogs
{
// File/folder picker replacements keep the map interactive while paths are chosen.
void File(CWnd* owner, bool open, const CString& initial, const CString& filters,
          std::function<void(CString, UINT)> accepted, bool folder = false);
void Color(CWnd* owner, COLORREF initial, std::function<void(COLORREF)> accepted);
}
#pragma pack(pop)
