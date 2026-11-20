#pragma once
#include <afxcontrolbars.h>
#include <functional>

// Text edits commit before a canvas click can change the selected object.
class COverlayTextRibbonEdit : public CMFCRibbonEdit
{
    DECLARE_DYNCREATE(COverlayTextRibbonEdit)
public:
    COverlayTextRibbonEdit();
    COverlayTextRibbonEdit(UINT id, int width, LPCTSTR label);
    CMFCRibbonRichEditCtrl* CreateEdit(CWnd* parent, DWORD style) override;
    void CommitText(const CStringW& text);
    const CStringW& CommittedText() const;
    void EnableDelayedCommit(BOOL enable);
private:
    CStringW m_committedText;
};

class COverlayTextRichEditCtrl : public CMFCRibbonRichEditCtrl
{
public:
    explicit COverlayTextRichEditCtrl(COverlayTextRibbonEdit& owner);
    COverlayTextRichEditCtrl(CMFCRibbonEdit& owner,
        std::function<void(const CStringW&)> commit, std::function<void(BOOL)> delayed);
    BOOL PreTranslateMessage(MSG* message) override;
protected:
    afx_msg void OnKillFocus(CWnd* newWindow);
    LRESULT WindowProc(UINT message, WPARAM wParam, LPARAM lParam) override;
    DECLARE_MESSAGE_MAP()
private:
    bool m_composing;
    std::function<void(const CStringW&)> m_commit;
    std::function<void(BOOL)> m_delayed;
    void Commit();
};

class COverlayFontRibbonCombo : public CMFCRibbonFontComboBox
{
    DECLARE_DYNCREATE(COverlayFontRibbonCombo)
public:
    COverlayFontRibbonCombo();
    explicit COverlayFontRibbonCombo(UINT id);
    CMFCRibbonRichEditCtrl* CreateEdit(CWnd* parent, DWORD style) override;
    void OnSelectItem(int index) override;
};

class COverlaySizeRibbonCombo : public CMFCRibbonComboBox
{
    DECLARE_DYNCREATE(COverlaySizeRibbonCombo)
public:
    COverlaySizeRibbonCombo();
    COverlaySizeRibbonCombo(UINT id, int width);
    CMFCRibbonRichEditCtrl* CreateEdit(CWnd* parent, DWORD style) override;
    void OnSelectItem(int index) override;
};
