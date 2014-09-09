msbuild build\nuwen.sln /nologo /v:q /t:cubes

if %errorlevel% neq 0 goto End

build\test\Debug\cubes.exe

:End

