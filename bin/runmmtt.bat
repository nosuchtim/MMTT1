c:/windows/system32/taskkill /f /im mmtt.exe
c:/windows/system32/taskkill /f /im mmtt1.exe
rem set PUBLIC=c:\local\manifold\Public
if x%1 == x start mmtt1.exe -r -cdefault
if not x%1 == x start mmtt1.exe -r -c%1
