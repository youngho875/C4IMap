# 그리드 기준 투영 선택

지도 리본 → 그리드 → **그리드 기준 투영**에서 선택합니다.

- 지도 투영과 동일: 기존 방식 유지
- Geodetic (경위도)
- UTM
- Mercator
- Lambert Conformal Conic
- Stereographic
- Equidistant Cylindrical
- TM

지도와 다른 그리드를 선택할 수 있습니다. 예를 들어 지도 투영을 TM으로 유지하면서 Geodetic 경위도 그리드를 표시할 수 있습니다.
Geodetic은 도(°), 나머지는 미터(m) 간격을 사용합니다. 색상과 표시 여부도 기존처럼 설정합니다.

**그리드용 TM 기준**은 독립 TM 그리드에만 적용합니다.
**지도용 TM 기준**은 지도 투영 메뉴의 TM에 적용합니다.
‘지도 투영과 동일’을 선택하면 지도에서 사용하는 원점도 그대로 따릅니다.

UTM·LCC·Stereographic 그리드는 기준 경도/위도를 입력합니다.
UTM의 존과 북/남반구는 해당 경위도로 결정합니다.
LCC의 표준위선은 지도 투영과 동일하게 북반구 30°/60°, 남반구 -30°/-60°를 사용합니다.
기준 경위도는 설정값으로 고정되며 지도 이동 때마다 바뀌지 않습니다.

사용 중인 base-map.xml에 다음 설정이 저장됩니다. 기존 Grid 필드도 유지합니다.

```xml
<Grid>
  <Visible>true</Visible>
  <Projection>Geodetic</Projection>
  <MetreInterval>10000</MetreInterval>
  <DegreeInterval>0.1</DegreeInterval>
  <Color>#80A0C0</Color>
  <TMEpsg>5186</TMEpsg>
  <OriginLatitude>38</OriginLatitude>
  <OriginLongitude>127</OriginLongitude>
</Grid>
```

Projection 값: SameAsMap, Geodetic, UTM, Mercator, LCC, Stereographic, EquidistantCylindrical, TM.
기존 XML에 Projection이 없으면 SameAsMap을 사용합니다.
지도용 TM 원점은 별도 Projection/TMEpsg에 저장되어 그리드용 TM과 구분됩니다.
