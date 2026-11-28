#include "pch.h"
#include "C4IMap.h"
#include "MainFrm.h"
#include "C4IMapView.h"   // mbc 상호참조
#include "XMLControl.h"
#include <RpcDce.h>
#include <string>

using namespace std;

CXMLControl::CXMLControl()
{
	strNewGuid = _T("");
	m_strLastSaveTM = _T("");
}

CXMLControl::~CXMLControl()
{
}

CString CXMLControl::GetGUID()
{
	UUID uuid = {0};
	unsigned char* pszUuid = NULL;
	::UuidCreate(&uuid);
	::UuidToStringA(&uuid, &pszUuid);
	strNewGuid = (const char*)pszUuid;
	::RpcStringFreeA(&pszUuid);

	return strNewGuid;
}

CString CXMLControl::GetSaveTime()
{
	char szDateTime[_MAX_PATH];
	SYSTEMTIME tmCreate = {0};
	int nDateLen = 0;
	*szDateTime = 0x00;
	::GetLocalTime(&tmCreate);
		
	::GetDateFormat(LOCALE_USER_DEFAULT, 0, &tmCreate, "yyyy'-'MM'-'dd' '", szDateTime, _MAX_PATH);
	nDateLen = lstrlenA(szDateTime);
	::GetTimeFormat(LOCALE_USER_DEFAULT, 0, &tmCreate, "HH':'mm",(szDateTime+nDateLen), _MAX_PATH-nDateLen);
	m_strLastSaveTM = szDateTime;

	return m_strLastSaveTM;
}

 
CString CXMLControl::MultiLinePoints(OverlayFileRecordCommon *data)
{
	CString list, list1, list2;

	if(data->m_nPointIndex == 0)
	{
		list1.Format("%f,%f|%f,%f", data->m_pt[0].Lng(), data->m_pt[0].Lat(), 
									data->m_pt[1].Lng(), data->m_pt[1].Lat());
		list += list1;
	}
	else
	{
		for(int i=0; i<data->m_nPointIndex; i++)
		{
			if(i%2 == 0)
			{
				if(i == data->m_nPointIndex-1)
					list1.Format("%f,%f", data->m_pt[i].Lng(), data->m_pt[i].Lat());
				else
					list1.Format("%f,%f|", data->m_pt[i].Lng(), data->m_pt[i].Lat());
				list += list1;
			}
			else if(i%2)
			{
				if(i == data->m_nPointIndex-1)
					list2.Format("%f,%f", data->m_pt[i].Lng(), data->m_pt[i].Lat());
				else
					list2.Format("%f,%f|", data->m_pt[i].Lng(), data->m_pt[i].Lat());
				list += list2;
			}
		}
	}
	return list;
}

CString CXMLControl::CuverPoints(OverlayFileRecordCommon *data)
{
	CString list, list1, list2;

	if(data->m_nPointIndex == 0)
	{
		list1.Format("%f,%f|%f,%f", data->m_pt[0].Lng(), data->m_pt[0].Lat(), 
									data->m_pt[1].Lng(), data->m_pt[1].Lat());
		list += list1;
	}
	else
	{
		for(int i=0; i<data->m_nPointIndex; i++)
		{
			if(i%2 == 0)
			{
				if(i == data->m_nPointIndex-1)
					list1.Format("%f,%f", data->m_pt[i].Lng(), data->m_pt[i].Lat());
				else
					list1.Format("%f,%f|", data->m_pt[i].Lng(), data->m_pt[i].Lat());
				list += list1;
			}
			else if(i%2)
			{
				if(i == data->m_nPointIndex-1)
					list2.Format("%f,%f", data->m_pt[i].Lng(), data->m_pt[i].Lat());
				else
					list2.Format("%f,%f|", data->m_pt[i].Lng(), data->m_pt[i].Lat());
				list += list2;
			}
		}
	}
	return list;
}

CString CXMLControl::OutLinePoints(OverlayFileRecordCommon *data)
{
	CString list, list1, list2;

	for(int i=0; i<OVERLAY_OBJECTS::POINTS_OF_OBJECT_OUTLINE; i++)
	{
		if(i%2 == 0)
		{
			if(i == OVERLAY_OBJECTS::POINTS_OF_OBJECT_OUTLINE-1)
				list1.Format("%f,%f", data->m_pt_out_line[i].Lng(), data->m_pt_out_line[i].Lat());
			else
				list1.Format("%f,%f|", data->m_pt_out_line[i].Lng(), data->m_pt_out_line[i].Lat());
			list += list1;
		}
		else if(i%2)
		{
			if(i == OVERLAY_OBJECTS::POINTS_OF_OBJECT_OUTLINE-1)
				list2.Format("%f,%f", data->m_pt_out_line[i].Lng(), data->m_pt_out_line[i].Lat());
			else
				list2.Format("%f,%f|", data->m_pt_out_line[i].Lng(), data->m_pt_out_line[i].Lat());
			list += list2;
		}
	}	
	return list;
}

CString CXMLControl::LinePoints(OverlayFileRecordCommon *data)
{
	CString list, list1, list2;

	for(int i=0; i<data->m_nPoints; i++)
	{
		if(i%2 == 0)
		{
			if(i == data->m_nPoints-1)
				list1.Format("%f,%f", data->m_pt[i].Lng(), data->m_pt[i].Lat());
			else
				list1.Format("%f,%f|", data->m_pt[i].Lng(), data->m_pt[i].Lat());
			list += list1;
		}
		else if(i%2)
		{
			if(i == data->m_nPoints-1)
				list2.Format("%f,%f", data->m_pt[i].Lng(), data->m_pt[i].Lat());
			else
				list2.Format("%f,%f|", data->m_pt[i].Lng(), data->m_pt[i].Lat());
			list += list2;
		}
	}	
	return list;
}

/*******************************************************************
	***** 투명도 문서 저장(OVD) *****

	did : document ID - 투명도 문서 식별자			mandatory
*******************************************************************/
CString CXMLControl::SetOVD()
{
	CString did, uuid;
	uuid = GetGUID();

	did.Format("<ovd did=\"%s\">\n", uuid);
	
	return did;
}

/*******************************************************************
	***** 투명도 문서에 대한 정보 저장(OVI) *****

	nm : name - 투명도 문서명				type string						mandatory					
	au : author - 작성자						type string
	ex : exercise - 훈련용 구분				type bool		default: 0
	sec : security - 비밀등급				type			default: 0
	si : SI - SI 구분						type bool		default: 0
	ovc : overlay category - 투명도 분류		type string
	dc : date time created - 생성일시		type datetime					mandatory
	de : date time edited - 수정일시			type datetime					mandatory
	src : data source - 원천자료 출처		type string
*******************************************************************/
CString CXMLControl::SetOVI(OverlayFileRecordCommon *data, LayerStructure* pLayer)
{
	CString ovi, savetime;

	savetime = GetSaveTime();
	ovi.Format("<ovi nm=\"%s\" au=\"\" ex=\"\" sec=\"\" si=\"\" ovc=\"\" dc=\"%s\" de=\"%s\" src=\"\">\n", 
		pLayer->GetLayerName(), savetime, savetime);

	return ovi;
}

/*******************************************************************
	***** 투명도 부호 자료 저장(ovsd) *****

	did : document ID - 투명도 문서 식별자
*******************************************************************/
CString CXMLControl::SetOVSD()
{
	CString did, uuid;
	uuid = GetGUID();

	did.Format("<ovd did=\"%s\">\n", uuid);
	
	return did;
}

/*******************************************************************
	***** 투명도 부호 그룹 객체(ovg) *****

	oid : object ID - 객체 식별자
	gid : group ID - 객체가 속한 그룹객체의 식별자
	nm : name - 그룹화된 객체명
	dc : date time created - 객체 생성 일시
	de : date time edited - 객체 수정 일시
	sh : show - 그룹화된 부호의 도시여부
	lck : lock symbol editing - 부호 수정금지 설정 상태
*******************************************************************/
CString CXMLControl::SetOVG(OverlayFileRecordCommon *data)
{
	CString ovg, savetime;
	
	savetime = GetSaveTime();
	ovg.Format("<ovg oid=\"\" gid=\"0\" nm=\"\" dc=\"%s\" de=\"%s\" sh=\"\" lck=\"\" st=\"\" />\n",
		savetime, savetime); 
	return ovg;
}

/*******************************************************************
	***** 투명도 부호 객체 저장(OVS) *****

	oid : object ID - 객체 식별자 
	gid : group ID - 객체가 속한 그룹객체의 식별자
	nm : name - 부호 객체명
	dc : date time created - 객체 생성 일시
	de : date time edited - 객체 수정 일시
	sh : show - 부호의 도시여부
	lck : lock symbol editing - 부호 수정금지 설정 상태
	st : symbol type - 부호 종류 
*******************************************************************/
CString CXMLControl::SetOVS(OverlayFileRecordCommon *data, LayerStructure *layer)
{
	CString ovs, savetime, name;

	savetime = GetSaveTime();
	switch(data->m_eType)
	{
	case 0:		name = "NULL_TYPE";			break;
	case 1:		name = "CURVE"; 			break;
	case 2:		name = "POINT"; 			break;
	case 3:		name = "LINE";  			break;
	case 4:		name = "TRIANGLE";			break;
	case 5:		name = "RECT";				break;
	case 6:		name = "DIAMOND";			break;
	case 7:		name = "ELLIPSE";			break;
	case 8:		name = "POLYGON";			break;
	case 9:		name = "PIE";				break;
	case 10:	name = "POLYLINE";			break;
	case 11:	name = "ROUND_RECT";		break;
	case 12:	name = "TEXT";				break;
	case 13:	name = "PENTAGON";			break;
	case 14:	name = "SIX_ANGLE";			break;
	case 15:	name = "EIGHT_ANGLE";		break;
	case 16:	name = "IMAGE";				break;
	case 17:	name = "ARC";				break;
	case 18:	name = "LINKED_LINE";		break;
	case 19:	name = "LINKED_MULTI_LINE";	break;
	case 20:	name = "LINKED_MULTI_CURVE";break;
	case 21:	name = "OUTLINE";			break;
	}
	
	ovs.Format("<ovs oid=\"\" gid=\"%s\" nm=\"%s\" dc=\"%s\" de=\"%s\" sh=\"\" lck=\"\" st=\"\">\n",
		layer->GetLayerName(), name, savetime, savetime); 
	return ovs;
}

/*******************************************************************
	***** 체계 정의 부호 객체(ovx) *****

	oid : object ID - 객체 식별자                                                                            
	gid : group ID - 객체가 속한 그룹객체의 식별자                                                            
	nm : name - 부호 객체명                                                                             
	dc : date time created - 객체 생성 일시                                                                 
	de : date time edited - 객체 수정 일시                                                                
	sh : show - 부호의 도시여부                                                                             
	lck : lock symbol editing - 부호 수정금지 설정 상태                                                      
	sid : system ID - 부호를 생성 및 사용하는 체계 식별자. 체계 약어 사용 (예: KJCCS, MIMS, ATCIS, KNCCS, AFCCS 등)
	st : symbol type - 체계에서 사용하는 부호 종류. 체게에서 정한 식별번호 사용                          
	sd : symbol data - 체계에서 사용하는 포맷의 부호자료를 base64로 인코딩하여 저장                       
*******************************************************************/
CString CXMLControl::ExtraSymbol(OverlayFileRecordCommon *data)
{
	CString extraSym, savetime;

	savetime = GetSaveTime();
	extraSym.Format("<ovx oid=\"\" gid=\"\" nm=\"\" dc=\"%d\" de=\"%d\" sh=\"\" lck=\"\" sid=\"\"		\
						st=\"\" sd=\"\" />\n", savetime, savetime); 
	return extraSym;

}

/**************************************************************
	***** 부가속성 저장 *****
	
	did : overlay document ID - 투명도 문서 식별자
***************************************************************/
 CString CXMLControl::SetOVP()
 {
	CString ovp, uuid;

	uuid = GetGUID();
	ovp.Format("<ovp did=\"%d\" >\n");

	return ovp;
 }

 /**************************************************************
	***** 부가속성 그룹 *****
	
	nm : name - 부가속성 그룹명
	roid : 참조 객체 식별자
	rnm : 참조 객체명
***************************************************************/
 CString CXMLControl::PropertyGroup()
 {
	CString spl;
	spl.Format("<sts nm=\"%f\" roid=\"0\" rnm=\"\" />\n");

	return spl;
 }

/**************************************************************
	***** 부가 속성 항목 *****
	
	nm : name - 속성명
	val : value - 속성값 
***************************************************************/
 CString CXMLControl::PropertyItem()
 {
	CString spl;
	spl.Format("<sts nm=\"%f\" val=\"\" />\n");

	return spl;
 }

 //////////////////////////////////////////////////////////////////////////////////////
 //
 //		투명도 부호 공통 속성
 //
 /////////////////////////////////////////////////////////////////////////////////////

/*******************************************************************
	***** 투명도 부호 공간정보(GEO) *****

	pt : points - 부호 좌표                     
	ct : coordinate type - 적용 좌표 종류         
	ha : horizontal alignment - 수평기준             
	va : vertical alignment - 수직기준               
	as : auto show - 축척/배울에 따른 도시상태 자동조절 여부
	asl : auto show lower scale limit - 자동조절 축척 하한 
	asu : auto show upper scale limit - 자동조절 축척 상한
*******************************************************************/
CString CXMLControl::SetGEO(OverlayFileRecordCommon *data)
{
	CString geo, pointList;

	if(data->m_eType == 8)
		pointList = CuverPoints(data);
	else
		pointList = LinePoints(data);
	geo.Format("<geo pt=\"%s\" ct=\"0\" ha=\"\" va=\"\" as=\"\" as1=\"\" asu=\"\"/>\n", pointList); 

	return geo;
}

/*******************************************************************
	***** 선 속성 정보(LP) *****

	la : line alignment - 선 치우침 상태
	lc : line color - 선 색상
	ls : line stripe - 겹선 정보
	lj : line join - 선 연결 모양
	ml : miter limit - 선 연결 부위 처리 길이 
	ft : line fill type - 선 채움 방법
	lw : line width - 선 굵기
*******************************************************************/
CString CXMLControl::LineProperty(OverlayFileRecordCommon *data)
{
	CString lineProperty;
	lineProperty.Format("<lp la=\"0\" lc=\"#%x\" ls=\"\" lj=\"0\" ml=\"\" ft=\"%d\" lw=\"%f\">\n",
		data->m_Property.LineColor, data->m_Property.FillStyle, data->m_Property.dLineWidth);

	return lineProperty;

}

/*******************************************************************
	***** 부호 채움 속성(FP) *****

	ft : fill type - 채움 방법 
	fc : fill color - 채움 색상
*******************************************************************/
CString CXMLControl::FillProperty(OverlayFileRecordCommon *data)
{
	CString _Fill;
	_Fill.Format("<fp ft=\"%d\" fc=\"#%x\">\n", data->m_Property.FillStyle, data->m_Property.FillColor); 
	return _Fill;

}

/*******************************************************************
	***** 부호문자 속성(TP) *****

	tx : text - 문자열                 
	ha : horizontal alignment - 좌우정렬   
	va : vertical alignment - 상하정렬          
	fn : font name - 글꼴명                     
	fs : font size - 문자크기                 
	tc : text color - 문자 색상                 
	bd : bold - 굵게              
	it : italic - 기울임                          
	drl : direction right to left - 우에서 좌로 문자 나열
	dv : direction vertical - 세로 방향으로        
*******************************************************************/
CString CXMLControl::TextProperty(OverlayFileRecordText *data)
{
	CString txtProperty;
	char *text = new char;
	WCHAR *wtext = new WCHAR;
	wtext = data->m_Text;

	USES_CONVERSION;
	text = W2A(wtext);
	txtProperty.Format("<tp tx=\"%s\" ha=\"\" va=\"\" fn=\"%d\" fs=\"%f\" tc=\"#%x\" bd=\"%d\" it=\"%d\" \
drl=\"\" dv=\"\" />\n", text, data->m_eFamilyName, data->m_dFontSize, data->m_FontColor, data->m_eFontStyle, 
						data->m_eFontStyle); 

	//delete wtext;
	//delete text;

	return txtProperty;

}


/*******************************************************************
	***** 음영 *****

	hs : hatch style - 음영 모양 
	fg : foreground color - 전경색
	bg : background color - 배경색
*******************************************************************/
CString CXMLControl::HatchBrush(OverlayFileRecordCommon *data)
{
	CString Hatchtype;
	Hatchtype.Format("<hb hs=\"\" fg=\"\" bg=\"\" />\n"); 
	return Hatchtype;

}

/*******************************************************************
	***** 패스 그라데이션 *****

	wm : wrap mode - 패턴 반복 방식
	fsx : focus scale x - 중앙 색상 가로 크기
	fsy : focus scale x - 중앙 색상 세로 크기
	cc : center color - 중앙 색상
	ic : interpolation colors - 그라데이션 적용 색상
	bf : blend factors - 색변화 조절 인자
	bp : blend positions - 색변화 조절 위치
	cpx ; center point x - 중앙 위치 이동 비율 
	cpy ; center point y - 중앙 위치 이동 비율
	gmc : gamma correction - 색상 보정
*******************************************************************/
CString CXMLControl::PathGradient(OverlayFileRecordCommon *data)
{
	CString pathGradient;
	pathGradient.Format("<pgb wm=\"\" fsx=\"\" fsy=\"\" cc=\"\" ic=\"\" bf=\"\" bp=\"\" cpx=\"\" cpy=\"\" gmc=\"\"/>\n"); 
	return pathGradient;

}

/*******************************************************************
	***** 선형 그라데이션 *****

	lgm ; linear gradation mode - 선형 그라데이션 방향 
	ic : interpolation colors - 그라데이션 색상
	bf : blend factors - 색 변화 조절 인자 
	bp : blend positions - 색 변화 조절 위치
*******************************************************************/
CString CXMLControl::LinerGradient(OverlayFileRecordCommon *data)
{
	CString linerGradient;
	linerGradient.Format("<lgb lgm=\"\" ic=\"\" bf=\"\" bp=\"\" />\n"); 
	return linerGradient;

}

/*******************************************************************
	***** 선 끝 모양 *****

	cs : line cap style - 선 끝 모양
	cl : line cap length - 선 끝 모양 길이
	cw : line cap widtht - 선 끝 모양 폭
*******************************************************************/
CString CXMLControl::StartLineCap(OverlayFileRecordCommon *data)
{
	CString LineCap;
	LineCap.Format("<lcs cs=\"%d\" cl=\"1.0\" cw=\"1.0\"/>\n", data->m_Property.nStartLineArrowStyle); 
	return LineCap;
}

CString CXMLControl::EndLineCap(OverlayFileRecordCommon *data)
{
	CString LineCap;
	LineCap.Format("<lce cs=\"%d\" cl=\"1.0\" cw=\"1.0\"/>\n", data->m_Property.nEndLineArrowStyle); 
	return LineCap;
}

/*******************************************************************
	***** 점 선의 모양 속성 *****

	cd : dash line cap - 점선 사이의 선 끝 모양
	ds : dash style - 점선 모양
	dp : dash pattern - 점선 패턴
	do : dash offset - 점선 시작 위치
*******************************************************************/
CString CXMLControl::LineDash(OverlayFileRecordCommon *data)
{
	CString DotLine;
	DotLine.Format("<ds cd=\"%d\" ds=\"%d\" dp=\"\" do=\"\" />\n", 
				data->m_Property.eDashStyle, data->m_Property.eLineStyle); 
	return DotLine;

}


 //////////////////////////////////////////////////////////////////////////////////////
 //
 //		투명도 부호 종류별 객체 속성
 //
 /////////////////////////////////////////////////////////////////////////////////////

/*******************************************************************
	***** 점 부호 객체 *****

	sz : size - 부호크기
	ps : point style - 부호 종류
*******************************************************************/
CString CXMLControl::SetSPT(OverlayFileRecordCommon *data)
{
	CString spl;
	spl.Format("<spt sz=\"\" ps=\"\" />\n");

	return spl;
}


/*******************************************************************
	***** 사각형 부호 객체 *****

	rot : rotate - 회전값
	rad : radius - 둥근사각형 꼭지점 반경
*******************************************************************/
CString CXMLControl::SetSRT(OverlayFileRecordCommon *data)
{
	CString spl;
	spl.Format("<srt rot=\"%f\" rad=\"\" />\n", data->m_Property.dAngle);

	return spl;
}

/*******************************************************************
	***** 타원 부호 객체 *****

	rot : rotate - 회전값 
*******************************************************************/
CString CXMLControl::SetSEL(OverlayFileRecordCommon *data)
{
	CString spl;
	spl.Format("<sel rot=\"%f\" />\n", data->m_Property.dAngle);

	return spl;
}

/*******************************************************************
	***** 호 부호 객체 *****

	rot : rotate - 회전값 -->         
	alt : arc line type - 호 선 모양
	aft : arc fill type - 호 채움 모양
*******************************************************************/
CString CXMLControl::SetSAR(OverlayFileRecordCommon *data)
{
	CString spl;
	spl.Format("<sar rot=\"%f\" alt=\"\" aft=\"\" />\n", data->m_Property.dAngle);

	return spl;
}

/*******************************************************************
	***** 다각형 부호 객체 *****

	rot : rotate - 회전값            
	plt : polyline type - 직선/곡선 종류
	cls : closed - 닫힘 여부 
*******************************************************************/
CString CXMLControl::SetSPL(OverlayFileRecordCommon *data)
{
	CString spl;
	spl.Format("<spl rot=\"%f\" it=\"0\" cls=\"0\"/>\n", data->m_Property.dAngle);

	return spl;
}


/*******************************************************************
	***** 전술 부호 객체 *****

	sym : symbol assembly string - 부호 조립정보 문자열(부호코드 + 수식정보) 
	sz : symbol size - 부호크기
	cm : color mode - 색상 설정 방법
	lc : line/icon color - 사용자 지정 선색상 또는 아이콘 색상
	fc : fill/frame color - 사용자 지정 채움색상 또는 외형채움 색상
	shi : show icon - 기능부호 표시 여부
	shf : show frame - 외형부호 표시 여부
	shc : show CIV - 민간부호 색상 채움 여부
	ff : fill frame - 외형부호 채움 여부
	ocp : operation condition position - 운용조건 표시 위치
*******************************************************************/
CString CXMLControl::SetSTS(OverlayFileRecordCommon *data)
{
	CString spl;
	CString strAsm, strXml;
	BOOL bRes;
	CHAR		*pszMB;
	int			nLen;

	CC4IRTDSDoc *pDoc = (CC4IRTDSDoc *)((CMainFrame *)AfxGetMainWnd())->GetActiveDocument();
	bRes = CTest_OverItem::GetOverlayXmlString(pDoc->m_parOverItems, strXml);
	if (!bRes) return _T("");

	_bstr_t	bstrXml(strXml);

	nLen = ::WideCharToMultiByte(CP_UTF8, 0, bstrXml, -1, NULL, 0, NULL, NULL);
	if (nLen>0)
	{
		pszMB = new CHAR[nLen + 2];

#if _MFC_VER < 0x0800
		::ZeroMemory(pszMB, sizeof(CHAR) * (nLen+2));
#else
		::SecureZeroMemory(pszMB, sizeof(CHAR) * (nLen+2));
#endif

		nLen = ::WideCharToMultiByte(CP_UTF8, 0, bstrXml, -1, pszMB, nLen, NULL, NULL);


	}
	//spl.Format("<sts sym=\"%s\" sz=\"0\" cm=\"\" lc=\"\" fc=\"\" shi=\"\" shf=\"\" shc=\"\" ff=\"\" ocp=\"\" />\n",
	//			strAsm);
	spl = strXml;

	return spl;
}



/**************************************************************
	***** 부가 속성 항목 *****

	ps : point shape - 점 모양 종류, 투명도 점 부호의 점 종류 구분 식별자
	st : shape type - 점 모양 도형, 점 모양을 도시하는데 사용할 도형 종류
	pt : points - 좌표
***************************************************************/
CString CXMLControl::PointType(OverlayFileRecordCommon *data)
{
	CString pointList, pointType, list, list1, list2;

	pointList.Format("<pointTypeList>\n");
	pointType.Format("\t\t<ol_pointType ps=\"\" st=\"\" pt=\"\" />\n");
	pointList += pointType;
	pointList += "\t</pointTypeList>\n";

	return pointList;
}
