@echo off
rem Debug info and opt
cl /nologo /Z7 /Od calc.c /Fe:calc.exe && buildnum CALC_BUILD.h CALC

rem Fast opt
rem cl /nologo /O2 calc.c /Fe:calc.exe

rem Small size opt
rem cl /nologo /O1 calc.c /Fe:calc.exe