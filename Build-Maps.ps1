param(
    [ValidateSet('Local','Tiles','Network','All')][string]$Mode='All',
    [ValidateSet('Debug','Release')][string]$Configuration='Release'
)
$ErrorActionPreference='Stop'
$vswhere="${env:ProgramFiles(x86)}/Microsoft Visual Studio/Installer/vswhere.exe"
$msbuild=& $vswhere -latest -products '*' -requires Microsoft.Component.MSBuild -find 'MSBuild/**/Bin/MSBuild.exe' | Select-Object -First 1
if(!$msbuild){throw 'Visual Studio C++ / MSBuild is required.'}
$modes=if($Mode -eq 'All'){@('Local','Tiles','Network')}else{@($Mode)}
foreach($selected in $modes){
    & $msbuild "$PSScriptRoot/C4IMap.$selected.vcxproj" /t:Build "/p:Configuration=$Configuration" /p:Platform=x64 /m /v:minimal /nologo
    if($LASTEXITCODE -ne 0){exit $LASTEXITCODE}
}
