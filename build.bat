@echo off

::set target=ph_test
set target=samples

set config=Release
set config=RelWithDebInfo
set config=Debug

:: /nr prevents creation of persitent msbuild processes
msbuild build\acid.sln /nologo /v:q /m /nr:true /t:%target% /p:Configuration=%config%

if %errorlevel% neq 0 goto End

copy Windows\glew32.dll build\%target%\%config%\glew32.dll
copy build\third_party\glfw\src\%config%\glfw3.lib build\%target%\%config%\glfw3.dll

:: build\%target%\%config%\%target%.exe

:End

