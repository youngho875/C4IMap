#include "pch.h"
#include "C4IMapViewHeader.h"
#include "OverlayStyleDialog.h"
BOOL CC4IMapView::OnOverlayStyleCommand(UINT id)
{
    if (m_localOverlay.IsDrawing()) return TRUE;
    auto style = m_localOverlay.GetStyle();
    auto frame = get_mainfrm();
    if (!frame) return TRUE;
    bool dialog = false;
    if (id == ID_MENU_OVERLAY_LINE_COLOR || id == ID_MENU_OVERLAY_FILL_COLOR || id == ID_MENU_OVERLAY_PATTERN_COLOR)
    {
        auto button = id == ID_MENU_OVERLAY_LINE_COLOR ? frame->m_pRibbonDrawShapeLineColor :
            (id == ID_MENU_OVERLAY_FILL_COLOR ? frame->m_pRibbonDrawShapeFillColor : frame->m_pRibbonDrawShapePatternColor);
        if (!button) return TRUE;
        COLORREF color = button->GetColor();
        if (id == ID_MENU_OVERLAY_LINE_COLOR) {style.lineEnabled = color != (COLORREF)-1; if(style.lineEnabled)style.lineColor = color;}
        else if (id == ID_MENU_OVERLAY_FILL_COLOR) {style.fillEnabled = color != (COLORREF)-1; if(style.fillEnabled)style.fillColor = color;}
        else if(color != (COLORREF)-1)style.patternColor = color;
    }
    else if (id >= ID_MENU_OVERLAY_LINE_WIDTH_0 && id <= ID_MENU_OVERLAY_LINE_WIDTH_10)
        {style.width = id-ID_MENU_OVERLAY_LINE_WIDTH_0;style.lineEnabled = style.width > 0;}
    else if (id >= ID_MENU_OVERLAY_LINE_STYLE_0 && id <= ID_MENU_OVERLAY_LINE_STYLE_4)
        style.dash = id-ID_MENU_OVERLAY_LINE_STYLE_0;
    else if (id >= ID_MENU_OVERLAY_FILL_TYPE_0 && id <= ID_MENU_OVERLAY_FILL_TYPE_4)
        {int type=id-ID_MENU_OVERLAY_FILL_TYPE_0;style.fillEnabled=type!=0;if(type)style.fillType=type;}
    else if (id >= ID_MENU_OVERLAY_HATCH_STYLE_0 && id <= ID_MENU_OVERLAY_HATCH_STYLE_52)
        {style.hatch=id-ID_MENU_OVERLAY_HATCH_STYLE_0;style.fillType=2;style.fillEnabled=true;}
    else if (id == ID_MENU_OVERLAY_FILL_USE) style.fillEnabled = !style.fillEnabled;
    else if (id == ID_MENU_OVERLAY_LINE_WIDTH_COLOR)
    {
        const double widths[]={0.75,1,1.5,2.25,3,4.5,6};
        int index=frame->m_pRibbonDrawShapeLineWidth->GetSelectedItem();
        if(index<0 || index>=7)return TRUE;
        style.width=widths[index];style.lineEnabled=true;
    }
    else if (id == ID_MENU_OVERLAY_LINE_DESH_COLOR)
    {
        int index=frame->m_pRibbonDrawShapeLineDesh->GetSelectedItem();
        if(index<0 || index>4)return TRUE;
        style.dash=index;
    }
    else dialog=true;
    if (dialog)
    {
        COverlayStyleDialog editor(style,m_localOverlay.HasSelection(),this);
        if(editor.Show()!=IDOK)return TRUE;
        style=editor.Style();
    }
    if(m_localOverlay.SetStyle(style))Invalidate(FALSE);
    return TRUE;
}
void CC4IMapView::OnUpdateOverlayStyle(CCmdUI* ui)
{
    ui->Enable(!m_localOverlay.IsDrawing());
    auto s=m_localOverlay.GetStyle();
    if(ui->m_nID==ID_MENU_OVERLAY_FILL_USE)ui->SetCheck(s.fillEnabled);
    else if(ui->m_nID>=ID_MENU_OVERLAY_LINE_WIDTH_0 && ui->m_nID<=ID_MENU_OVERLAY_LINE_WIDTH_10)ui->SetCheck(s.width==ui->m_nID-ID_MENU_OVERLAY_LINE_WIDTH_0);
    else if(ui->m_nID>=ID_MENU_OVERLAY_LINE_STYLE_0 && ui->m_nID<=ID_MENU_OVERLAY_LINE_STYLE_4)ui->SetCheck(s.dash==ui->m_nID-ID_MENU_OVERLAY_LINE_STYLE_0);
    else if(ui->m_nID>=ID_MENU_OVERLAY_FILL_TYPE_0 && ui->m_nID<=ID_MENU_OVERLAY_FILL_TYPE_4)ui->SetCheck((s.fillEnabled?s.fillType:0)==ui->m_nID-ID_MENU_OVERLAY_FILL_TYPE_0);
    else if(ui->m_nID>=ID_MENU_OVERLAY_HATCH_STYLE_0 && ui->m_nID<=ID_MENU_OVERLAY_HATCH_STYLE_52)ui->SetCheck(s.fillEnabled&&s.fillType==2&&s.hatch==ui->m_nID-ID_MENU_OVERLAY_HATCH_STYLE_0);
}
