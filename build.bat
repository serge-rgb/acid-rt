::set target=chicken
::set target=ph_test
set target=samples
set target=ocl_hello

set config=RelWithDebInfo
set config=Debug
::set config=Release

:: /nr prevents creation of persitent msbuild processes
msbuild build\acid.sln /nologo /v:q /m /nr:true /t:%target% /p:Configuration=%config%

if %errorlevel% neq 0 goto End


copy Windows\glew32.dll build\%target%\%config%\glew32.dll
copy build\third_party\bdwgc\%config%\gcmt-dll.dll build\%target%\%config%\gcmt-dll.dll
copy build\third_party\glfw\src\%config%\glfw3.dll build\%target%\%config%\glfw3.dll
copy build\third_party\lua\%config%\lua.dll build\%target%\%config%\lua.dll
copy build\third_party\yajl\yajl-2.1.1\lib\%config%\yajl.dll build\%target%\%config%\yajl.dll

build\%target%\%config%\%target%.exe

:End

