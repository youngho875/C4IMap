# C4IMap TM 및 그리드 설정

지도 리본의 **투영 → TM**을 선택하면 TM으로 표시합니다.
기본 기준은 **Korea 2000 중부원점 2010 (EPSG:5186)**입니다.
원본 SHP·래스터·타일 파일을 변환하거나 수정하지 않고 화면의 투영을 변경합니다.

지도 리본의 **그리드** 버튼에서 다음을 설정합니다.

- 지도에 그리드 표시: 켜기/끄기
- 투영 그리드 간격(m): TM, UTM, Mercator, LCC, Stereographic, Equidistant Cylindrical에서 사용
- 경위도 그리드 간격(°): Geodetic에서 사용
- 그리드 색 선택: 색상 선택 창에서 변경
- TM 기준 좌표계: Korea 2000 서부/중부/동부/동해원점과 통합좌표계, Korean 1985 수정원점 선택

확인을 누르면 적용하고 설정을 저장합니다. 취소하면 변경하지 않습니다.
TM을 표시하는 동안 원점을 바꾸면 현재 지도 중심을 유지한 채 새 TM 원점으로 갱신합니다.
그리드는 지도 이동·확대·축소·회전을 따라갑니다.
기본 상태는 그리드 숨김, 투영 간격 10,000m, 경위도 간격 0.1°입니다.
너무 조밀한 그리드는 화면 성능을 위해 지정 간격의 정수 배수로 간략히 표시합니다.

설정은 `c4imap-paths.xml`의 `BaseMapXml`이 지정한 파일(기본: 실행 파일 옆 base-map.xml)에 저장합니다.
기존 지도 레이어 설정은 유지됩니다.

```xml
<Grid>
  <Visible>true</Visible>
  <MetreInterval>1000</MetreInterval>
  <DegreeInterval>0.1</DegreeInterval>
  <Color>#80A0C0</Color>
</Grid>
<Projection>
  <TMEpsg>5186</TMEpsg>
</Projection>
```

투영 간격의 허용 범위는 0.1~10,000,000m, 경위도 간격은 0.000001~180°입니다.
TM은 해당 원점 주변의 지역 지도용입니다. 현재 구현은 극지방과 중앙경선에서 지나치게 먼 영역을 제외합니다.

구현 파일:
- DisplayProjection.h/.cpp: TM 정·역변환과 화면 중심 유지
- MapGrid.h/.cpp: 설정 저장 및 그리드 생성·표시
- GridSettingsDialog.h/.cpp: 사용자 설정 창
- C4IMapView.cpp, MainFrm.cpp: 기존 메뉴 연결
- MapDecoration.cpp: 지도 합성 후 그리드 표시
