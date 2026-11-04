# 지도 빌드 선택

사용자가 지정할 전처리 옵션은 `C4MAP_BUILD_MODE` 하나입니다.

| 값 | 프로젝트 | 데이터 | Release 실행 파일 |
|---|---|---|---|
| 1 | C4IMap.Local.vcxproj | 원본 SHP, 래스터, CIB | dist/Local/C4IMap.exe |
| 2 | C4IMap.Tiles.vcxproj | 벡터 타일, 래스터 XYZ, CIB XYZ | dist/Tiles/C4IMap.exe |
| 3 | C4IMap.Network.vcxproj | WMS, WFS, WMTS, terrain 서버 | dist/Network/C4IMap.exe |

세 모드 모두 기본지도를 지원합니다. 각 실행 파일 옆 base-map.xml에 정의한
소축척 지도, 세계지도, Natural Earth, 지명을 표시합니다. 네트워크 모드의
기본지도도 XML이 가리키는 로컬 파일이 필요합니다. 데이터 자체는 빌드에 포함하지 않습니다.
XML에서 주석 처리한 기본지도는 트리에서 제외됩니다.

## Visual Studio 2019

C4IMap.BuildModes.sln을 열고 Release / x64를 선택합니다.
원하는 프로젝트를 마우스 오른쪽 버튼으로 눌러 빌드합니다.
실행하려는 프로젝트를 시작 프로젝트로 설정합니다. 솔루션 전체 빌드는 세 모드를 모두 빌드합니다.
이번 검증은 Release / x64 기준입니다. Debug는 기존 디버그 SDK·라이브러리 설치가 필요합니다.

## 명령 프롬프트 또는 PowerShell

    .\Build-Maps.cmd Local
    .\Build-Maps.cmd Tiles
    .\Build-Maps.cmd Network
    .\Build-Maps.cmd All

기존 C4IMap.sln의 일반 프로젝트는 기본값이 타일 모드입니다.
MSBuild에서는 /p:C4MapBuildMode=1, 2 또는 3으로 선택할 수 있습니다.
일반 프로젝트의 vcxproj에서 C4MapBuildMode 값을 변경할 수도 있습니다.
모드별 프로젝트는 각각 1/2/3을 고정하므로 별도 전처리 옵션을 수정할 필요가 없습니다.
기존 C4MAP_ENABLE_LOCAL, C4MAP_ENABLE_OGC, C4MAP_SHAPE_MODE,
C4MAP_RASTER_MODE, C4MAP_CIB_MODE, C4MAP_TILE_TREE_ONLY는 내부 파생 값이며
개별 설정은 무시됩니다. 설정 충돌을 피하려면 기존 사용자 정의에서 제거하십시오.

각 배포 폴더에 필요한 DLL과 XML, 기본지도 파일을 함께 준비해야 합니다.
