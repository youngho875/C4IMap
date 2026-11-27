////////////////////////////////////////////////////////////////////////////////
//
// 본 프로그램 소스에 대한 모든 권리는 (주)씨엔지솔루션에 있습니다.
// Copyright (C) C&G Solution, Inc. All rights reserved.
//
// 본 프로그램 소스는 '군대부호 도시 표준 컴포넌트 사용을 위한 SDK'
// (이하 'SDK')에 포함되지 않은 경우 (주)씨엔지솔루션의 사전 승인없이 임의로
// 열람/복제/복사/배포할 수 없습니다.
//
// 본 프로그램 소스는 SDK에 포함된 경우 SDK의 일부로서 열람/배포가 가능하나,
// 개별 파일로 배포할 수 없으며 SDK 상태로 배포해야 합니다.
//
// SDK에 포함된 프로그램 소스는 SDK를 사용하여 개발하는 응용프로그램의 컴파일을
// 위해 제공하는 것이며 용도 이외의 목적으로 사용할 수 없습니다.
//
// 국군지휘통신사령부를 통하여 제공하는 SDK는 한국군의 사용을 목적으로 체계를
// 개발하기 위해 군에서 발주하는 사업 수행을 목적으로한 개발에 한하여 배포 및
// 사용이 가능합니다.
//
// 기업/연구기관/개인이 SDK를 사용하여 제품을 개발하거나 연구 또는 학습하고자
// 하는 경우 사전에 (주)씨엔지솔루션과 협의해야 합니다.
//
// 한국군의 사용을 위한 체계 개발을 위해 군에서 발주한 사업 수행이 목적이라도
// 기업의 제품 개발에 SDK를 사용할 경우 사전에 (주)씨엔지솔루션과 협의해야 하며
// (주)씨엔지솔루션은 SDK 및 실행모듈의 사용료를 청구할 수 있습니다.
// 
//
// 작성자 : (주)씨엔지솔루션 임창관, H.P: 010-2352-1487, E-mail: cgyim@korea.com
// 작성일 : 2010/08/15
//
////////////////////////////////////////////////////////////////////////////////

#include "pch.h"
#pragma pack(push, 8)
#include "CTest_OverItem.h"
#include "C4IMap.h"
#include "ICOPS_Framework/IMapProjection.h"
#include "ICOPS_Framework/CStrUtil.h"
#include "ICOPS_Framework/CXmlUtil.h"
#include "ICOPS_WarSym/CwsUtil.h"
#include "ICOPS_WarSym/CwsSymEditContext.h"
#include "ICOPS_WarSym/CwsLegacyUtil.h"
#include "ICOPS_WarSym/IWarSymView.h"
#include "ICOPS_WarSym/IWarSymPrpTacSymbol.h"
#include "ICOPS_WarSym/IWarSymPrpTacPoint.h"
#include "ICOPS_WarSym/IWarSymPrpTacGraphic.h"
#include "ICOPS_LogManager/CLog.h"

CTest_OverItem::CTest_OverItem(void)
{
	m_Unk.m_pContainer = this;
}


CTest_OverItem::~CTest_OverItem(void)
{
}


////////////////////////////////////////////////////////////////////////////////
// IcopsUnknown implementation

BOOL CTest_OverItem::QueryInterface(const CString &ifName, void **ppv)
{
	if (ifName==_T("IcopsUnknown"))
	{
		*ppv = dynamic_cast<IcopsUnknown *>(this);
		AddRef();
		return TRUE;
	}
	if (ifName==_T("IWarSymEditable"))
	{
		*ppv = dynamic_cast<IWarSymEditable *>(this);
		AddRef();
		return TRUE;
	}
	if (ifName==_T("IWarSymbol"))
	{
		*ppv = (void *) m_piSym;
		m_piSym->AddRef();
		return TRUE;
	}
	if (ifName==_T("CTest_OverItem"))
	{
		*ppv = (void *) this;
		AddRef();
		return TRUE;
	}

	return FALSE;
}


////////////////////////////////////////////////////////////////////////////////
// IWarSymImplEditable implementation

BOOL CTest_OverItem::AddPoint(CwsSymEditContext *pEditCxt, UINT nFlags, const SGeoCoord2 *gp)
{
	ASSERT(m_piSym!=NULL);

	BOOL	bRes;
	IWarSymEditablePtr	piEditable;

	bRes = m_piSym->QueryInterface(_T("IWarSymEditable"), (void **) &piEditable);
	if (bRes) return piEditable->AddPoint(pEditCxt, nFlags, gp);
	else return FALSE;
}


void CTest_OverItem::SetAddingPoint(CwsSymEditContext *pEditCxt, const SGeoCoord2 *gp)
{
	ASSERT(m_piSym!=NULL);

	BOOL	bRes;
	IWarSymEditablePtr	piEditable;

	bRes = m_piSym->QueryInterface(_T("IWarSymEditable"), (void **) &piEditable);
	if (bRes) piEditable->SetAddingPoint(pEditCxt, gp);
}


BOOL CTest_OverItem::DeleteLastPoint(CwsSymEditContext *pEditCxt)
{
	ASSERT(m_piSym!=NULL);

	BOOL	bRes;
	IWarSymEditablePtr	piEditable;

	bRes = m_piSym->QueryInterface(_T("IWarSymEditable"), (void **) &piEditable);
	if (bRes) return piEditable->DeleteLastPoint(pEditCxt);
	else return FALSE;
}


BOOL CTest_OverItem::CompleteAdding(CwsSymEditContext *pEditCxt)
{
	ASSERT(m_piSym!=NULL);

	BOOL	bRes;
	IWarSymEditablePtr	piEditable;

	bRes = m_piSym->QueryInterface(_T("IWarSymEditable"), (void **) &piEditable);
	if (bRes) return piEditable->CompleteAdding(pEditCxt);
	else return FALSE;
}


void CTest_OverItem::GetDragScale(CwsSymEditContext *pEditCxt, Gdiplus::Graphics *pGrpx, Gdiplus::REAL dx, Gdiplus::REAL dy, Gdiplus::REAL *sx, Gdiplus::REAL *sy)
{
	*sx = 1.0f;
	*sy = 1.0f;
}


void CTest_OverItem::OffsetControl(CwsSymEditContext *pEditCxt, Gdiplus::Graphics *pGrpx, Gdiplus::PointF pfDragCur, Gdiplus::REAL dx, Gdiplus::REAL dy, Gdiplus::REAL sx, Gdiplus::REAL sy)
{
	ASSERT(m_piSym!=NULL);

	INT_PTR		inx, count;
	Gdiplus::PointF		*pWP;
	const SGeoCoord2	*pGP;
	CTypedBuffer<SGeoCoord2>	bufGP;
	SGeoCoord2	*pBufGP;

	count = m_piSym->GetCoordCount();
	if (count<=0) return;

	// 도형의 지리좌표를 가상좌표로 변경한 후 가상좌표를 더하고 다시 지리좌표로 변환
	pGP = m_piSym->GetCoordData(pEditCxt->m_piMapPrj);
	pWP = pEditCxt->m_Point.SetCount(count);
	pEditCxt->m_piMapPrj->Forward(pGP, pWP, count);

	switch (pEditCxt->m_shr.eStatus)
	{
	case HitStatus_None :
		// do nothing
		break;
	case HitStatus_Point :
		pWP[pEditCxt->m_shr.ctrlInx].X += dx;
		pWP[pEditCxt->m_shr.ctrlInx].Y += dy;
		break;
	default :
		for (inx=0; inx<count; inx++)
		{
			pWP[inx].X += dx;
			pWP[inx].Y += dy;
		}
		break;
	}

	pBufGP = bufGP.SetCount(count);
	pEditCxt->m_piMapPrj->Inverse(pWP, pBufGP, count);
	m_piSym->SetCoordinates(pEditCxt->m_piMapPrj, pBufGP, count);
}


void CTest_OverItem::SimpleDraw(Gdiplus::Graphics *pGrpx, const Gdiplus::PointF *pPoints, INT_PTR nCount, double mpw, int nRop2)
{
	IWarSymEditablePtr	piEditable;
	BOOL	bRes;

	bRes = m_piSym->QueryInterface(_T("IWarSymEditable"), (void **) &piEditable);
	if (bRes) piEditable->SimpleDraw(pGrpx, pPoints, nCount, mpw, nRop2);
}


////////////////////////////////////////////////////////////////////////////////
// CTest_OverItem operations

BOOL CTest_OverItem::Clone(CTest_OverItem **pNew)
{
	BOOL	bRes;
	CTest_OverItem	*pNewOverItem;
	ICOPS::IWarSymbolPtr	piNewSym;

	if (m_piSym!=NULL)
	{
		bRes = m_piSym->Clone(&piNewSym);
		if (!bRes) return FALSE;
	}

	pNewOverItem = new CTest_OverItem;
	pNewOverItem->m_piSym = piNewSym;
    pNewOverItem->layer = layer;
    pNewOverItem->group = group;

	*pNew = pNewOverItem;
	(*pNew)->AddRef();

	return TRUE;
}


void CTest_OverItem::DrawControl(IMapProjection *piMapProj, Gdiplus::Graphics *pGrpx, EWarSymEditControlStyle eStyle, Gdiplus::Color clrCtrl)
{
	INT_PTR		inx, count;

	Gdiplus::PointF		*pWp;
	const ICOPS::SGeoCoord2	*pGp;
	ICOPS::CTypedBuffer<Gdiplus::PointF>	bufWp;

	count = m_piSym->GetCoordCount();
	if (count<=0) return;

	pGp = m_piSym->GetCoordData(piMapProj);
	pWp = bufWp.SetCount(count);
	piMapProj->Forward(pGp, pWp, count);

	for (inx=0; inx<count; inx++)
	{
		CwsUtil::DrawControl(pGrpx, pWp[inx], eStyle, clrCtrl);
	}
}


BOOL CTest_OverItem::GetAssembleStream(CString &strAsm)
{
	if (m_piSym==NULL) return FALSE;
	strAsm = m_piSym->GetAssembleStream();
	return !strAsm.IsEmpty();
}


BOOL CTest_OverItem::SetAssembleStream(const CString &strAsm)
{
	if (m_piSym==NULL)
	{
		BOOL	bRes;
		CString	strTmp;
		ICOPS::IWarSymFactoryPtr	piSymFac;
		ICOPS::IWarSymbolPtr		piSym;
		CC4IMapApp*pApp = (CC4IMapApp*) ::AfxGetApp();
        if(!pApp || !pApp->m_pAppCxt) return FALSE;

		bRes = pApp->m_pAppCxt->GetStockObject(_T("IWarSymFactory"), (void **) &piSymFac);
		if (!bRes) return FALSE;

		strTmp.Empty();
		bRes = piSymFac->CreateSymbol(strTmp, &piSym);
		if (!bRes) return FALSE;

		m_piSym = piSym;
	}

	m_piSym->SetAssembleStream(strAsm);

	return TRUE;
}


BOOL CTest_OverItem::GetCoordStream(CString &strCoord, int nResolution)
{
	INT_PTR		inx, count;
	CString		strFmt, strX, strY;
	const SGeoCoord2	*pCoord;

	count = GetCoordCount();
	if (count<=0) return FALSE;

	pCoord = GetCoordData(NULL);
	if (pCoord==NULL) return FALSE;

	strFmt = _T("%s,%s,");
	strCoord.Empty();

#if _MFC_VER < 0x0700
	CString		strPt;
	for (inx=0; inx<count; inx++)
	{
		//xstrX = CMathUtil::ftostr(pCoord[inx].lon, nResolution);
		//xstrY = CMathUtil::ftostr(pCoord[inx].lat, nResolution);
		strX.Empty();
		strY.Empty();
		CStrUtil::num2str(strX.GetBuffer(CStrUtil_NumBufSize), CStrUtil_NumBufSize, pCoord[inx].lon, nResolution, TRUE);
		CStrUtil::num2str(strY.GetBuffer(CStrUtil_NumBufSize), CStrUtil_NumBufSize, pCoord[inx].lat, nResolution, TRUE);
		strX.ReleaseBuffer();
		strY.ReleaseBuffer();

		strPt.Format(strFmt, strX, strY);
		strCoord += strPt;
	}
#else
	for (inx=0; inx<count; inx++)
	{
		//xstrX = CMathUtil::ftostr(pCoord[inx].lon, nResolution);
		//xstrY = CMathUtil::ftostr(pCoord[inx].lat, nResolution);
		strX.Empty();
		strY.Empty();
		CStrUtil::num2str(strX.GetBuffer(CStrUtil_NumBufSize), CStrUtil_NumBufSize, pCoord[inx].lon, nResolution, TRUE);
		CStrUtil::num2str(strY.GetBuffer(CStrUtil_NumBufSize), CStrUtil_NumBufSize, pCoord[inx].lat, nResolution, TRUE);
		strX.ReleaseBuffer();
		strY.ReleaseBuffer();

		strCoord.AppendFormat(strFmt, strX, strY);
	}
#endif

	if (!strCoord.IsEmpty())
	{
		strCoord.SetAt(strCoord.GetLength()-1, TCHAR('\0'));
	}

	return TRUE;

}


BOOL CTest_OverItem::SetCoordStream(const CString &strCoord)
{
	CString			strToken;
	CStringArray	straTokens;
	ICOPS::SGeoCoord2	*pGP;
	INT_PTR		inx, count;
	INT_PTR		curPos = 0;

	if (m_piSym==NULL)
	{
		CLog::AddLog(__FILE__, __LINE__, CLog::Log_Warning, _T("좌표를 지정할 부호 객체가 없음."));
		ASSERT(FALSE);
		return FALSE;
	}

	inx = 0;
	strToken = CwsLegacyUtil::CString_Tokenize(strCoord, _T(","), curPos);
	while (!strToken.IsEmpty())
	{
		straTokens.Add(strToken);
		strToken = CwsLegacyUtil::CString_Tokenize(strCoord, _T(","), curPos);
	}

	count = (int) straTokens.GetSize();
	if (count%2!=0)
	{
		CLog::AddLog(__FILE__, __LINE__, CLog::Log_Warning, _T("좌표문자열의 숫자 쌍이 맞지 않음"));
	}

	count /= 2;
	if (count<=0)
	{
		CLog::AddLog(__FILE__, __LINE__, CLog::Log_Error, _T("좌표문자열에 좌표가 없음"));
		return FALSE;
	}

	pGP = new ICOPS::SGeoCoord2[count];
	for (inx=0; inx<count; inx++)
	{
#if _MSC_VER > 1200
#if defined(_UNICODE)
		pGP[inx].lon = ::_wtof(straTokens.GetAt(2*inx));
		pGP[inx].lat = ::_wtof(straTokens.GetAt(2*inx + 1));
#else
		pGP[inx].lon = ::atof(straTokens.GetAt(2*inx));
		pGP[inx].lat = ::atof(straTokens.GetAt(2*inx + 1));
#endif
#else
		pGP[inx].lon = ::atof(_bstr_t(straTokens.GetAt(2*inx)));
		pGP[inx].lat = ::atof(_bstr_t(straTokens.GetAt(2*inx + 1)));
#endif
	}

	m_piSym->SetCoordinates(NULL, pGP, count);
	delete[] pGP;

	return TRUE;
}


void CTest_OverItem::MakeXMLDOM(MSXML2::IXMLDOMDocument2Ptr &spXMLDoc, MSXML2::IXMLDOMElementPtr &spIOV)
{
	CString		strAsm, strCoord;
	BOOL		bAsm, bCoord;
	MSXML2::IXMLDOMElementPtr	spElem = NULL;

	bAsm = GetAssembleStream(strAsm);
	bCoord = GetCoordStream(strCoord, -1);

	spElem = spXMLDoc->createElement(_bstr_t("item"));

	if (bAsm && bCoord)
	{
		spElem->setAttribute(_bstr_t("symbol"), _bstr_t(strAsm));
		spElem->setAttribute(_bstr_t("coord"), _bstr_t(strCoord));
		SaveItemProperty(spElem);
		spIOV->appendChild(spElem);
	}
}


void CTest_OverItem::LoadItemProperty(MSXML2::IXMLDOMNodePtr &spNode)
{
	ICOPS::IWarSymPrpTacSymbolPtr	piPrpSym;
	ICOPS::SWarSymDrawMode		*pDM;
	BOOL	bRes, bVal;
	Gdiplus::REAL	rTmp;
	CString			strTmp;

	bRes = CXmlUtil::GetAttributeValue(spNode, _bstr_t("symsize"), &rTmp);
	if (bRes) m_piSym->SetSymbolSize(rTmp);

	int nTp;
	bRes = CXmlUtil::GetAttributeValue(spNode, _bstr_t("transparency"), &nTp);
	if (bRes) m_piSym->SetTransparency((BYTE) nTp);

	// 도시모드
	pDM = m_piSym->GetDrawMode();
	if (pDM)
	{
		bRes = CXmlUtil::GetAttributeBooleanValue(spNode, _bstr_t("bgWhite"), &bVal);
		pDM->isBgWhite = (bRes ? bVal : TRUE);
		bRes = CXmlUtil::GetAttributeBooleanValue(spNode, _bstr_t("colorMode"), &bVal);
		pDM->isColorMode = (bRes ? bVal : TRUE);
		bRes = CXmlUtil::GetAttributeBooleanValue(spNode, _bstr_t("fillFrame"), &bVal);
		pDM->fillFrame = (bRes ? bVal : TRUE);
		bRes = CXmlUtil::GetAttributeBooleanValue(spNode, _bstr_t("showframe"), &bVal);
		pDM->showFrame = (bRes ? bVal : TRUE);
		bRes = CXmlUtil::GetAttributeBooleanValue(spNode, _bstr_t("showIcon"), &bVal);
		pDM->showIcon = (bRes ? bVal : TRUE);
		bRes = CXmlUtil::GetAttributeBooleanValue(spNode, _bstr_t("civ"), &bVal);
		pDM->civ = (bRes ? bVal : FALSE);

		// 도시모드 : 외형 채움 색상 밝음 정도
		bRes = CXmlUtil::GetAttributeValue(spNode, _bstr_t("luminance"), &strTmp);
		if (bRes)
		{
			if (strTmp.CompareNoCase(_T("light"))==0) pDM->luminance = WarSymLuminance_Light;
			else if (strTmp.CompareNoCase(_T("medium"))==0) pDM->luminance = WarSymLuminance_Medium;
			else if (strTmp.CompareNoCase(_T("dark"))==0) pDM->luminance = WarSymLuminance_Dark;
			else if (strTmp.CompareNoCase(_T("custom"))==0) pDM->luminance = WarSymLuminance_Custom;
		}
		else
		{
			pDM->luminance = WarSymLuminance_Light;
		}

		// 도시모드 : 도시 유형
		bRes = CXmlUtil::GetAttributeValue(spNode, _bstr_t("dispType"), &strTmp);
		if (bRes)
		{
			if (strTmp.CompareNoCase(_T("display"))==0) pDM->dispType = WarSymDisplayType_AsDisplay;
			else if (strTmp.CompareNoCase(_T("editor"))==0) pDM->dispType = WarSymDisplayType_AsEditor;
			else if (strTmp.CompareNoCase(_T("template"))==0) pDM->dispType = WarSymDisplayType_AsTemplate;
		}
		else
		{
			pDM->dispType = WarSymDisplayType_AsDisplay;
		}

		// 도시모드 : 부호 상태구분 코드
		bRes = CXmlUtil::GetAttributeValue(spNode, _bstr_t("symState"), &strTmp);
		if (bRes && !strTmp.IsEmpty())
		{
			switch ((ICOPS::EWarSymStateCode) strTmp.GetAt(0))
			{
			case ICOPS::WarSymState_Present :
			default :
				pDM->symState = WarSymState_Present;
				break;
			case ICOPS::WarSymState_Anticipated :
				pDM->symState = WarSymState_Anticipated;
				break;
			case ICOPS::WarSymState_Normal :
				pDM->symState = WarSymState_Normal;
				break;
			case ICOPS::WarSymState_Damaged :
				pDM->symState = WarSymState_Damaged;
				break;
			case ICOPS::WarSymState_Destroyed :
				pDM->symState = WarSymState_Destroyed;
				break;
			case ICOPS::WarSymState_FullToCapacity :
				pDM->symState = WarSymState_FullToCapacity;
				break;
			}
		}
		else
		{
			pDM->symState = WarSymState_Present;
		}
	}

	// 색상

	// 전술기호 속성
	bRes = m_piSym->GetPropInterface(_T("IWarSymPrpTacSymbol"), (void **) &piPrpSym);
	if (bRes)
	{
		// 운용조건 표시
		bRes = CXmlUtil::GetAttributeValue(spNode, _bstr_t("opMode"), &strTmp);
		if (bRes && !strTmp.IsEmpty())
		{
			if (strTmp.CompareNoCase(_T("center"))==0) piPrpSym->SetOpCondDisplayMode(WarSymOpCondDispMode_Center);
			else if (strTmp.CompareNoCase(_T("bottom"))==0) piPrpSym->SetOpCondDisplayMode(WarSymOpCondDispMode_Bottom);
			else piPrpSym->SetOpCondDisplayMode(WarSymOpCondDispMode_None);
		}
		else
		{
			 piPrpSym->SetOpCondDisplayMode(WarSymOpCondDispMode_None);
		}
	}

	_bstr_t nodeName;
	MSXML2::IXMLDOMElementPtr spElem = NULL;
	spElem = spNode->firstChild;
	while (spElem != NULL) {
		nodeName = spElem->nodeName;
		if (nodeName == _bstr_t("lbsl")) {
			LoadXMLDOM_LabelStateList(spElem);
		}
		spElem = spElem->nextSibling;
	}
}


void CTest_OverItem::SaveItemProperty(MSXML2::IXMLDOMElementPtr &spElem)
{
	ICOPS::IWarSymPrpTacSymbolPtr	piPrpSym;
	ICOPS::SWarSymDrawMode		*pDM;
	ICOPS::EWarSymOpCondDispMode	eOpMode;
	BOOL	bRes;
	Gdiplus::REAL	rTmp;
	CString		strTmp;

	rTmp = m_piSym->GetSymbolSize();
	spElem->setAttribute(_bstr_t("symsize"), rTmp);

	int nTp = (int) m_piSym->GetTransparency();
	spElem->setAttribute(_bstr_t("transparency"), nTp);

	// 도시모드
	pDM = m_piSym->GetDrawMode();
	if (pDM!=NULL)
	{
		if (!pDM->isBgWhite) spElem->setAttribute(_bstr_t("bgWhite"), (long) 0);
		if (!pDM->isColorMode) spElem->setAttribute(_bstr_t("colorMode"), (long) 0);
		if (!pDM->fillFrame) spElem->setAttribute(_bstr_t("fillFrame"), (long) 0);
		if (!pDM->showFrame) spElem->setAttribute(_bstr_t("showframe"), (long) 0);
		if (!pDM->showIcon) spElem->setAttribute(_bstr_t("showIcon"), (long) 0);
		if (pDM->civ) spElem->setAttribute(_bstr_t("civ"), (long) 1);

		// 도시모드 : 외형 채움 색상 밝음 정도
		switch (pDM->luminance)
		{
		//case ICOPS::WarSymLuminance_Light :
		//	// 기본값이므로 저장하지 않음
		//	//spElem->setAttribute(_bstr_t("luminance"), _bstr_t("light"));
		//	break;
		case ICOPS::WarSymLuminance_Medium :
			spElem->setAttribute(_bstr_t("luminance"), _bstr_t("medium"));
			break;
		case ICOPS::WarSymLuminance_Dark :
			spElem->setAttribute(_bstr_t("luminance"), _bstr_t("dark"));
			break;
		case ICOPS::WarSymLuminance_Custom :
			spElem->setAttribute(_bstr_t("luminance"), _bstr_t("custom"));
			break;
		}

		// 도시모드 : 도시 유형
		switch (pDM->dispType)
		{
		//case ICOPS::WarSymDisplayType_AsDisplay :
		//	// 기본값이므로 저장하지 않음
		//	//spElem->setAttribute(_bstr_t("dispType"), _bstr_t("display"));
		//	break;
		case ICOPS::WarSymDisplayType_AsEditor :
			spElem->setAttribute(_bstr_t("dispType"), _bstr_t("editor"));
			break;
		case ICOPS::WarSymDisplayType_AsTemplate :
			spElem->setAttribute(_bstr_t("dispType"), _bstr_t("template"));
			break;
		}

		// 도시모드 : 부호 상태구분 코드
		if (pDM->symState!=ICOPS::WarSymState_Present)
		{
			strTmp.Format(_T("%c"), pDM->symState);
			spElem->setAttribute(_bstr_t("symState"), _bstr_t(strTmp));
		}
	}

	// 색상

	// 전술기호 속성
	bRes = m_piSym->GetPropInterface(_T("IWarSymPrpTacSymbol"), (void **) &piPrpSym);
	if (bRes)
	{
		// 운용조건 표시
		bRes = piPrpSym->GetOpCondDisplayMode(&eOpMode);
		if (bRes)
		{
			switch (eOpMode)
			{
			//case ICOPS::WarSymOpCondDispMode_None :
			//	// 기본값이므로 저장하지 않음
			//	spElem->setAttribute(_bstr_t("opMode"), _bstr_t("none"));
			//	break;
			case ICOPS::WarSymOpCondDispMode_Center :
				spElem->setAttribute(_bstr_t("opMode"), _bstr_t("center"));
				break;
			case ICOPS::WarSymOpCondDispMode_Bottom :
				spElem->setAttribute(_bstr_t("opMode"), _bstr_t("bottom"));
				break;
			}
		}
	}

	// 레이블 전시 상태
	MakeXMLDOM_LabelStateList(spElem);
}


void CTest_OverItem::MakeXMLDOM_LabelStateList(MSXML2::IXMLDOMElementPtr &spElem)
{
	ICOPS::IWarSymView *piSymView = m_piSym->GetSymbolView();
	CStringArray labels;
	piSymView->EnumLabels(labels);

	MSXML2::IXMLDOMDocument2Ptr spXMLDoc = spElem->GetownerDocument();
	MSXML2::IXMLDOMElementPtr spLbsl = NULL;
	MSXML2::IXMLDOMElementPtr spLbl;

	BOOL bShowLbl;
	BOOL bLblSzRateChanged;
	Gdiplus::REAL lblSzRate;

	CString label;
	INT_PTR count = labels.GetCount();
	INT_PTR idx;

	for (idx = 0; idx < count; idx++) {
		bLblSzRateChanged = FALSE;
		label = labels.GetAt(idx);
		lblSzRate = piSymView->GetLabelSizeRate(label, &bLblSzRateChanged);
		bShowLbl = piSymView->IsShowLabel(label);

		if (!bShowLbl || bLblSzRateChanged) {
			if (spLbsl == NULL) {
				spLbsl = spXMLDoc->createElement(_bstr_t("lbsl"));
				spElem->appendChild(spLbsl);
			}

			spLbl = spXMLDoc->createElement(_bstr_t("lbl"));
			spLbsl->appendChild(spLbl);

			spLbl->setAttribute(_bstr_t("nm"), _bstr_t(label));
			if (!bShowLbl) {
				spLbl->setAttribute(_bstr_t("sh"), FALSE);
			}
			if (bLblSzRateChanged) {
				spLbl->setAttribute(_bstr_t("ls"), lblSzRate);
			}
		}
	}
}


BOOL CTest_OverItem::LoadXMLDOM_LabelStateList(MSXML2::IXMLDOMElementPtr &spLbsl)
{
	ICOPS::IWarSymView *piSymView = m_piSym->GetSymbolView();
	_bstr_t nodeName;
	MSXML2::IXMLDOMElementPtr spLbl;
	BOOL bName;
	BOOL bShow;
	BOOL bSize;
	CString strNm;
	BOOL bSh;
	float fSize;

	spLbl = spLbsl->firstChild;
	while (spLbl != NULL) {
		nodeName = spLbl->nodeName;
		if (nodeName == _bstr_t("lbl")) {
			bName = CXmlUtil::GetAttributeValue(spLbl, _bstr_t("nm"), &strNm);
			if (bName) {
				bSh = TRUE;
				fSize = 1.0f;

				bShow = CXmlUtil::GetAttributeBooleanValue(spLbl, _bstr_t("sh"), &bSh);
				bSize = CXmlUtil::GetAttributeValue(spLbl, _bstr_t("ls"), &fSize);

				if (bShow) {
					piSymView->ShowLabel(strNm, bSh);
				}
				if (bSize) {
					piSymView->SetLabelSizeRate(strNm, fSize);
				}
			}
		}
		spLbl = spLbl->nextSibling;
	}

	return TRUE;
}


BOOL CTest_OverItem::GetOverlayXmlString(CPtrArrayCTest_OverItem &itemList, CString &strXml)
{
	HRESULT		hr;
	MSXML2::IXMLDOMDocument2Ptr		spXMLDoc = NULL;
	MSXML2::IXMLDOMElementPtr		spIOV = NULL;
	CTest_OverItem	*pOverItem;
	INT_PTR		inx, count;

	strXml.Empty();

	// Create XML DOM for data
	hr = spXMLDoc.CreateInstance(__uuidof(MSXML2::DOMDocument60));
	if (FAILED(hr))
	{
		ICOPS::CLog::AddLog(__FILE__, __LINE__, ICOPS::CLog::Log_Error, _T("XML DOM 객체 생성 실패"));
		return FALSE;
	}


	try
	{
		spIOV = spXMLDoc->createElement(_bstr_t("iov"));
		if (spIOV!=NULL)
		{
			spXMLDoc->appendChild(spIOV);

			count = itemList.GetSize();
			for (inx=0; inx<count; inx++)
			{
				pOverItem = itemList.GetAt(inx);
				pOverItem->MakeXMLDOM(spXMLDoc, spIOV);
			}
		}

		strXml = (LPCTSTR) spXMLDoc->xml;
		strXml = CString(_T("<?xml version=\"1.0\" encoding=\"utf-8\" ?>")) + strXml;

		return TRUE;
	}
	catch (_com_error &e)
	{
		ICOPS::CLog::AddComErrorlog(__FILE__, __LINE__, ICOPS::CLog::Log_Error, e);
		return FALSE;
	}
}


BOOL CTest_OverItem::SetOverlayXmlString(CPtrArrayCTest_OverItem &itemList, CString &strXml, BOOL bAdd)
{
	BOOL	bAsm, bCoord;
	CString	strAsm, strCoord;
	CTest_OverItem	*pItem;
	_bstr_t			nodeName;
	MSXML2::IXMLDOMDocument2Ptr		spXMLDoc = NULL;
	MSXML2::IXMLDOMParseErrorPtr	spError = NULL;
	MSXML2::IXMLDOMElementPtr		spDocElem = NULL;
	MSXML2::IXMLDOMNodePtr			spNodeIOV = NULL;
	HRESULT			hr;
	VARIANT_BOOL	vbRes = VARIANT_FALSE;

	// Create XML DOM for data
	hr = spXMLDoc.CreateInstance(__uuidof(MSXML2::DOMDocument60));
	if (FAILED(hr))
	{
		CLog::AddLog(__FILE__, __LINE__, CLog::Log_Error, _T("XML DOM 객체 생성 오류로 인하여 부호자료 처리를 할 수 없음"));
		return FALSE;
	}

	try
	{
		spXMLDoc->async = VARIANT_FALSE;			// Synchronize loading
		spXMLDoc->validateOnParse = VARIANT_TRUE;	// Validate
		spXMLDoc->resolveExternals = VARIANT_TRUE;	// Resolve externals

		// Load data
		vbRes = spXMLDoc->loadXML(_bstr_t(strXml));

		// Check if error occurred while parsing xml data
		spError = spXMLDoc->parseError;
		if (spError->errorCode != S_OK)
		{
			CLog::AddXmlParseErrorLog(__FILE__, __LINE__, CLog::Log_Error, spError);
			return FALSE;
		}
		if (vbRes!=VARIANT_TRUE)
		{
			CLog::AddLog(__FILE__, __LINE__, CLog::Log_Error, _T("XML 읽기 실패 (%s)"), (LPCTSTR) strXml);
			return FALSE;
		}

		spDocElem = spXMLDoc->documentElement;	// tag : iov
		nodeName = spDocElem->nodeName;
		if (nodeName!=_bstr_t("iov"))
		{
			CLog::AddLog(__FILE__, __LINE__, CLog::Log_Warning, _T("ICOPS Overlay XML 자료가 아님 (%s)"), (LPCTSTR) strXml);
			return FALSE;
		}

		if (!bAdd)
		{
			// 기존 자료 삭제
			INT_PTR		inx, count;

			count = itemList.GetSize();
			for (inx=0; inx<count; inx++)
			{
				pItem = itemList.GetAt(inx);
				delete pItem;
			}
			itemList.RemoveAll();
		}

		spNodeIOV = spDocElem->firstChild;
		while (spNodeIOV!=NULL)
		{
			nodeName = spNodeIOV->nodeName;
			if (nodeName==_bstr_t("item"))
			{
				bAsm = CXmlUtil::GetAttributeValue(spNodeIOV, _bstr_t("symbol"), &strAsm);
				bCoord = CXmlUtil::GetAttributeValue(spNodeIOV, _bstr_t("coord"), &strCoord);

				if (bAsm && bCoord)
				{
					pItem = new CTest_OverItem;
					bAsm = pItem->SetAssembleStream(strAsm);
					bCoord = pItem->SetCoordStream(strCoord);

					if (bAsm && bCoord)
					{
						pItem->LoadItemProperty(spNodeIOV);
						pItem->AddRef();
						itemList.Add(pItem);
					}
					else
					{
						delete pItem;
					}
				}
			}
			else if (nodeName!=_bstr_t("#comment"))
			{
				CLog::AddLog(__FILE__, __LINE__, CLog::Log_Error, _T("XML 태그 오류 : item : '%s'"), (LPCTSTR) nodeName);
			}

			spNodeIOV = spNodeIOV->nextSibling;
		}
		return TRUE;
	}
	catch (_com_error &e)
	{
		CLog::AddComErrorlog(__FILE__, __LINE__, CLog::Log_Error, e);
		return FALSE;
	}
}


// 연결선 도시 지점 설정
void CTest_OverItem::AddPairingPoint(Gdiplus::PointF pairingPoint)
{
	m_pairingPoints.Append(&pairingPoint, 1, FALSE);
}


// 연결선 도시 지점 전체 삭제
void CTest_OverItem::ResetPairingPoints()
{
	m_pairingPoints.SetCount(0);
}


void CTest_OverItem::Draw(Gdiplus::Graphics *pGrpx, const Gdiplus::PointF *pPoints, INT_PTR count, double mpw)
{
	if (m_piSym==NULL) {
		return;
	}

	INT_PTR pairingCount = m_pairingPoints.GetCount();
	if (pairingCount<=0) {
		m_piSym->Draw(pGrpx, pPoints, count, mpw);
	} else {
		Gdiplus::PointF *pairingPoints = m_pairingPoints.GetBuffer();
		Gdiplus::Color pairingColor(255, 0, 0, 0);
		Gdiplus::REAL pairingLineWidth = 1.0f;
		m_piSym->Draw(pGrpx, pPoints, count, mpw, pairingPoints, pairingCount, pairingColor, pairingLineWidth);
	}
}


INT_PTR CTest_OverItem::GetCoordCount()
{
	ASSERT(m_piSym!=NULL);
	if (m_piSym==NULL) return 0;

	return m_piSym->GetCoordCount();
}


const SGeoCoord2 *CTest_OverItem::GetCoordData(IMapProjection *piMapProj)
{
	ASSERT(m_piSym!=NULL);
	if (m_piSym==NULL) return NULL;

	return m_piSym->GetCoordData(piMapProj);
}


void CTest_OverItem::SetCoordinates(IMapProjection *piMapProj, const SGeoCoord2 *pCoord, INT_PTR count)
{
	ASSERT(m_piSym!=NULL);
	if (m_piSym==NULL) return;

	m_piSym->SetCoordinates(piMapProj, pCoord, count);
}


EHitStatus CTest_OverItem::HitTest(Gdiplus::Graphics *pGrpx, IMapProjection *piMapProj, UINT nFlags, const Gdiplus::PointF &testDP, BOOL bSelected, BOOL bSingle, int devGravity, double mpw, SHitResult *pRes)
{
	ASSERT(m_piSym!=NULL);
	if (m_piSym==NULL) return HitStatus_None;

	EHitStatus	eRes;

	eRes = m_piSym->HitTest(pGrpx, piMapProj, nFlags, testDP, bSelected, bSingle, devGravity, mpw, pRes);
	if (eRes!=HitStatus_None) pRes->pObj = dynamic_cast<IWarSymEditable *>(this);

	return eRes;
}


CString CTest_OverItem::HitTestLabel(Gdiplus::Graphics *pGrpx, IMapProjection *piMapProj, const Gdiplus::PointF &testDP, int devGravity, double mpw)
{
	ASSERT(m_piSym!=NULL);
	if (m_piSym == NULL) {
		CString ret;
		ret.Empty();
		return ret;
	}

	return m_piSym->HitTestLabel(pGrpx, piMapProj, testDP, devGravity, mpw);
}


BOOL CTest_OverItem::GetBoundingRect(Gdiplus::Graphics *pGrpx, IMapProjection *piMapProj, double mpw, Gdiplus::RectF *pRet, BOOL bRecalcDPbuf)
{
	ASSERT(m_piSym!=NULL);
	if (m_piSym==NULL) return FALSE;

	return m_piSym->GetBoundingRect(pGrpx, piMapProj, mpw, pRet, bRecalcDPbuf);
}

// Versioned container stores WGS84 coordinates separately from the SDK screen projection.
void CTest_OverItem::SerializeSymbol(CArchive& archive)
{
    try
    {
        MSXML2::IXMLDOMDocument2Ptr document;
        if(FAILED(document.CreateInstance(__uuidof(MSXML2::DOMDocument60))))
            AfxThrowArchiveException(CArchiveException::genericException);
        if(archive.IsStoring())
        {
            if(!m_piSym) AfxThrowArchiveException(CArchiveException::badSchema);
            auto element=document->createElement(_bstr_t("item"));
            document->appendChild(element);
            SaveItemProperty(element);
            CString assembly;
            if(!GetAssembleStream(assembly)) AfxThrowArchiveException(CArchiveException::badSchema);
            CString properties((LPCTSTR)_bstr_t(document->xml));
            archive << layer << group << m_piSym->GetSymbolCode() << assembly << properties;
        }
        else
        {
            CString code,assembly,properties;
            archive >> layer >> group >> code >> assembly >> properties;
            if(layer<0 || group<0) AfxThrowArchiveException(CArchiveException::badSchema);
            auto app=static_cast<CC4IMapApp*>(AfxGetApp());
            if(!app || !app->m_piSymFac || !app->m_piSymFac->CreateSymbol(code,&m_piSym) || !m_piSym)
                AfxThrowArchiveException(CArchiveException::badSchema);
            if(!SetAssembleStream(assembly)) AfxThrowArchiveException(CArchiveException::badSchema);
            document->async=VARIANT_FALSE;
            document->validateOnParse=VARIANT_FALSE;
            document->resolveExternals=VARIANT_FALSE;
            if(document->loadXML(_bstr_t(properties))!=VARIANT_TRUE)
                AfxThrowArchiveException(CArchiveException::badSchema);
            MSXML2::IXMLDOMNodePtr node=document->documentElement;
            if(node==NULL) AfxThrowArchiveException(CArchiveException::badSchema);
            LoadItemProperty(node);
            if(layer<0 || group<0) AfxThrowArchiveException(CArchiveException::badSchema);
        }
    }
    catch(const _com_error&)
    {
        AfxThrowArchiveException(CArchiveException::badSchema);
    }
}
#pragma pack(pop)
