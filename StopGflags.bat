
set gflags_10_path="C:\Program Files (x86)\Windows Kits\10\Debuggers\x64\gflags.exe"
set gflags_8_path="C:\Program Files (x86)\Windows Kits\8.1\Debuggers\x64\gflags.exe"
if exist %gflags_10_path% (         
	call %gflags_10_path% /p /disable DemoApp.exe 
) else (  
	call %gflags_10_path% /p /disable DemoApp.exe 

)
