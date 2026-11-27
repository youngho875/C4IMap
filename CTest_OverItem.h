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

#pragma once
#pragma pack(push, 8)
#include "ICOPS_Framework/ICOPS_struct.h"
#include "ICOPS_Framework/CTypedBuffer.h"
#include "ICOPS_WarSym/IWarSymbol.h"
#include "ICOPS_WarSym/IWarSymEditable.h"

#pragma pack(8)
using namespace ICOPS;
class CTest_OverItem;

typedef CTypedPtrArray<CPtrArray, CTest_OverItem *> CPtrArrayCTest_OverItem;


class CTest_OverItem : public ICOPS::IWarSymEditable
{
public:
	CTest_OverItem(void);
	virtual ~CTest_OverItem(void);

// Attributes
public:
	ICOPS::IWarSymbolPtr	m_piSym;
	CTypedBuffer<Gdiplus::PointF> m_pairingPoints;

    int layer = 0;
    int group = 0;
    void SerializeSymbol(CArchive& archive);

// Operations
public:
	virtual BOOL	Clone(CTest_OverItem **pNew);
	virtual void	DrawControl(IMapProjection *piMapProj, Gdiplus::Graphics *pGrpx, EWarSymEditControlStyle eStyle, Gdiplus::Color clrCtrl);
	virtual BOOL	GetAssembleStream(CString &strAsm);
	virtual BOOL	SetAssembleStream(const CString &strAsm);
	virtual BOOL	GetCoordStream(CString &strCoord, int nResolution = 3);
	virtual BOOL	SetCoordStream(const CString &strCoord);
	virtual void	MakeXMLDOM(MSXML2::IXMLDOMDocument2Ptr &spXMLDoc, MSXML2::IXMLDOMElementPtr &spIOV);

	virtual void	LoadItemProperty(MSXML2::IXMLDOMNodePtr &spNode);
	virtual void	SaveItemProperty(MSXML2::IXMLDOMElementPtr &spElem);

	static	BOOL	GetOverlayXmlString(CPtrArrayCTest_OverItem &itemList, CString &strXml);
	static	BOOL	SetOverlayXmlString(CPtrArrayCTest_OverItem &itemList, CString &strXml, BOOL bAdd = FALSE);

	virtual void	AddPairingPoint(Gdiplus::PointF pairingPoint);	// 연결선 도시 지점 추가
	virtual void	ResetPairingPoints();	// 연결선 도시 지점 전체 삭제

	virtual void MakeXMLDOM_LabelStateList(MSXML2::IXMLDOMElementPtr &spElem);
	virtual BOOL LoadXMLDOM_LabelStateList(MSXML2::IXMLDOMElementPtr &spLbsl);

// IcopsUnknown implementation
public:
	CIcopsUnknown<CTest_OverItem>	m_Unk;
	virtual ULONG	AddRef(void) { return m_Unk.AddRef(); }
	virtual ULONG	Release(void) { return m_Unk.Release(); }
	virtual BOOL	QueryInterface(const CString &ifName, void **ppv);

// IWarSymEditable implementation
	// 편집
	virtual BOOL	AddPoint(CwsSymEditContext *pEditCxt, UINT nFlags, const SGeoCoord2 *gp);		// 점 추가입력. 작성 완료되면 TRUE 리턴
	virtual void	SetAddingPoint(CwsSymEditContext *pEditCxt, const SGeoCoord2 *gp);	// 다음 추가 예상점 설정
	virtual BOOL	DeleteLastPoint(CwsSymEditContext *pEditCxt);							// 마지막으로 입력한 점 취소. 모든 입력점이 취소되면 TRUE 리턴
	virtual BOOL	CompleteAdding(CwsSymEditContext *pEditCxt);							// 도형 작성 완료. 성공하면 TRUE 리턴
	virtual void	GetDragScale(CwsSymEditContext *pEditCxt, Gdiplus::Graphics *pGrpx, Gdiplus::REAL dx, Gdiplus::REAL dy, Gdiplus::REAL *sx, Gdiplus::REAL *sy);
	virtual void	OffsetControl(CwsSymEditContext *pEditCxt, Gdiplus::Graphics *pGrpx, Gdiplus::PointF pfDragCur, Gdiplus::REAL dx, Gdiplus::REAL dy, Gdiplus::REAL sx, Gdiplus::REAL sy);
	virtual void	SimpleDraw(Gdiplus::Graphics *pGrpx, const Gdiplus::PointF *pPoints, INT_PTR nCount, double mpw, int nRop2);

	virtual void		Draw(Gdiplus::Graphics *pGrpx, const Gdiplus::PointF *pPoints, INT_PTR count, double mpw);
	virtual INT_PTR		GetCoordCount();
	virtual const SGeoCoord2	*GetCoordData(IMapProjection *piMapProj);
	virtual void		SetCoordinates(IMapProjection *piMapProj, const SGeoCoord2 *pCoord, INT_PTR count);
	virtual EHitStatus	HitTest(Gdiplus::Graphics *pGrpx, IMapProjection *piMapProj, UINT nFlags, const Gdiplus::PointF &testDP, BOOL bSelected, BOOL bSingle, int devGravity, double mpw, SHitResult *pRes);
	virtual CString HitTestLabel(Gdiplus::Graphics *pGrpx, IMapProjection *piMapProj, const Gdiplus::PointF &testDP, int devGravity, double mpw);
	virtual BOOL	GetBoundingRect(Gdiplus::Graphics *pGrpx, IMapProjection *piMapProj, double mpw, Gdiplus::RectF *pRet, BOOL bRecalcDPbuf = TRUE);
};

typedef Ifptr<CTest_OverItem> CTest_OverItemPtr;

#pragma pack(pop)
