set config=Debug
:: set config=Release

msbuild build\nuwen.sln /nologo /v:q /m /t:cubes /p:Configuration=%config%

if %errorlevel% neq 0 goto End


copy Windows\glew32.dll build\test\%config%\glew32.dll
copy build\third_party\bdwgc\%config%\gcmt-dll.dll build\test\%config%\gcmt-dll.dll
copy build\third_party\glfw\src\%config%\glfw3.dll build\test\%config%\glfw3.dll
copy build\third_party\lua\%config%\lua.dll build\test\%config%\lua.dll
copy build\third_party\yajl\yajl-2.1.1\lib\%config%\yajl.dll build\test\%config%\yajl.dll

build\test\%config%\cubes.exe

:End

