@echo on
set MYDIR=CBTHooker-1.3-bin
if not exist %MYDIR% mkdir %MYDIR%
copy /Y dll32.dll %MYDIR%
copy /Y dll64.dll %MYDIR%
copy /Y watcher32.exe %MYDIR%
copy /Y watcher64.exe %MYDIR%
copy /Y CBTHooker.exe %MYDIR%
copy /Y README.txt %MYDIR%
copy /Y LICENSE.txt %MYDIR%
copy /Y HISTORY.txt %MYDIR%
copy /Y README_ja.txt %MYDIR%
