@echo off
setlocal
setlocal enabledelayedexpansion

set ref_sources=^
bitstream.c ^
lzw.c ^
main.c

for %%a in (%ref_sources%) do (
  set sources=!sources! ../src/lzw/%%a
)
for %%a in (%ref_sources%) do (
  set objects=!objects! %%~na.o
)

pushd intermediate

gcc -c -Wall -std=gnu99 %sources% 
gcc -o ..\bin\lzw.exe %objects%
popd

endlocal
