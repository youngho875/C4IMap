#include "pch.h"
#include "OverlayTextRibbonEdit.h"

IMPLEMENT_DYNCREATE(COverlayTextRibbonEdit, CMFCRibbonEdit)
BEGIN_MESSAGE_MAP(COverlayTextRichEditCtrl, CMFCRibbonRichEditCtrl)
    ON_WM_KILLFOCUS()
END_MESSAGE_MAP()

COverlayTextRibbonEdit::COverlayTextRibbonEdit() = default;
COverlayTextRibbonEdit::COverlayTextRibbonEdit(UINT id, int width, LPCTSTR label)
    : CMFCRibbonEdit(id, width, label)
{
}

CMFCRibbonRichEditCtrl* COverlayTextRibbonEdit::CreateEdit(CWnd* parent, DWORD style)
{
    auto edit = new COverlayTextRichEditCtrl(*this);
    if (!edit->Create(style, CRect(0, 0, 0, 0), parent, GetID()))
    {
        delete edit;
        return nullptr;
    }
    if (m_bHasSpinButtons)
        CreateSpinButton(edit, parent);
    return edit;
}

const CStringW& COverlayTextRibbonEdit::CommittedText() const
{
    return m_committedText;
}

void COverlayTextRibbonEdit::CommitText(const CStringW& text)
{
    m_committedText = text;
    auto original = DYNAMIC_DOWNCAST(COverlayTextRibbonEdit, GetOriginal());
    if (original)
    {
        original->CommitText(text);
        return;
    }
    // The application is MBCS. Do not use CStringW(CString), whose thread code
    // page can differ from the native edit's ANSI code page.
    SetEditText(CString(CW2A(text, CP_ACP)));
    if (auto ribbon = GetTopLevelRibbonBar())
    {
        CArray<CMFCRibbonBaseElement*, CMFCRibbonBaseElement*> elements;
        ribbon->GetElementsByID(GetID(), elements);
        for (INT_PTR i = 0; i < elements.GetSize(); ++i)
            if (auto edit = DYNAMIC_DOWNCAST(COverlayTextRibbonEdit, elements[i]))
                edit->m_committedText = text;
    }
    NotifyCommand(FALSE);
}

void COverlayTextRibbonEdit::EnableDelayedCommit(BOOL enable)
{
    m_bNotifyCommand = enable;
}

COverlayTextRichEditCtrl::COverlayTextRichEditCtrl(COverlayTextRibbonEdit& owner)
    : COverlayTextRichEditCtrl(owner,
        [&owner](const CStringW& text) { owner.CommitText(text); },
        [&owner](BOOL enable) { owner.EnableDelayedCommit(enable); })
{
}

COverlayTextRichEditCtrl::COverlayTextRichEditCtrl(CMFCRibbonEdit& owner,
    std::function<void(const CStringW&)> commit, std::function<void(BOOL)> delayed)
    : CMFCRibbonRichEditCtrl(owner), m_composing(false),
      m_commit(std::move(commit)), m_delayed(std::move(delayed))
{
}

void COverlayTextRichEditCtrl::Commit()
{
    CStringW text;
    const int length = ::GetWindowTextLengthW(GetSafeHwnd());
    ::GetWindowTextW(GetSafeHwnd(), text.GetBuffer(length + 1), length + 1);
    text.ReleaseBuffer();
    m_commit(text);
    GetWindowText(m_strOldText);
}

BOOL COverlayTextRichEditCtrl::PreTranslateMessage(MSG* message)
{
    if (message->message == WM_KEYDOWN && message->wParam == VK_RETURN)
    {
        // Let the IME finish its current composition before applying the text.
        if (m_composing)
            return FALSE;
        Commit();
        return TRUE;
    }
    return CMFCRibbonRichEditCtrl::PreTranslateMessage(message);
}

void COverlayTextRichEditCtrl::OnKillFocus(CWnd* newWindow)
{
    // Commit synchronously: a posted WM_COMMAND can arrive after selection changes.
    Commit();
    m_delayed(FALSE);
    CMFCRibbonRichEditCtrl::OnKillFocus(newWindow);
    m_delayed(TRUE);
}

LRESULT COverlayTextRichEditCtrl::WindowProc(UINT message, WPARAM wParam, LPARAM lParam)
{
    if (message == WM_IME_STARTCOMPOSITION)
        m_composing = true;
    else if (message == WM_IME_ENDCOMPOSITION)
        m_composing = false;
    return CMFCRibbonRichEditCtrl::WindowProc(message, wParam, lParam);
}

namespace
{
// Preserve popup dismissal without queuing an edit command behind canvas input.
void FinishComboSelection(CMFCRibbonComboBox& combo, CMFCRibbonPanelMenuBar* menu)
{
    combo.NotifyCommand(FALSE);
    if (menu)
        if (auto window = menu->GetParent())
            window->PostMessage(WM_CLOSE);
    combo.Redraw();
}
}

IMPLEMENT_DYNCREATE(COverlayFontRibbonCombo, CMFCRibbonFontComboBox)
IMPLEMENT_DYNCREATE(COverlaySizeRibbonCombo, CMFCRibbonComboBox)

COverlayFontRibbonCombo::COverlayFontRibbonCombo() = default;
COverlayFontRibbonCombo::COverlayFontRibbonCombo(UINT id) : CMFCRibbonFontComboBox(id) {}
COverlaySizeRibbonCombo::COverlaySizeRibbonCombo() = default;
COverlaySizeRibbonCombo::COverlaySizeRibbonCombo(UINT id, int width)
    : CMFCRibbonComboBox(id, TRUE, width) {}

CMFCRibbonRichEditCtrl* COverlayFontRibbonCombo::CreateEdit(CWnd* parent, DWORD style)
{
    auto edit = new COverlayTextRichEditCtrl(*this,
        [this](const CStringW& text) {
            SetEditText(CString(CW2A(text, CP_ACP)));
            NotifyCommand(FALSE);
        },
        [this](BOOL enabled) { m_bNotifyCommand = enabled; });
    if (!edit->Create(style, CRect(0, 0, 0, 0), parent, GetID()))
    {
        delete edit;
        return nullptr;
    }
    return edit;
}

CMFCRibbonRichEditCtrl* COverlaySizeRibbonCombo::CreateEdit(CWnd* parent, DWORD style)
{
    auto edit = new COverlayTextRichEditCtrl(*this,
        [this](const CStringW& text) {
            SetEditText(CString(CW2A(text, CP_ACP)));
            NotifyCommand(FALSE);
        },
        [this](BOOL enabled) { m_bNotifyCommand = enabled; });
    if (!edit->Create(style, CRect(0, 0, 0, 0), parent, GetID()))
    {
        delete edit;
        return nullptr;
    }
    return edit;
}

void COverlayFontRibbonCombo::OnSelectItem(int index)
{
    if (SelectItem(index))
        FinishComboSelection(*this, m_pParentMenu);
}

void COverlaySizeRibbonCombo::OnSelectItem(int index)
{
    if (SelectItem(index))
        FinishComboSelection(*this, m_pParentMenu);
}
