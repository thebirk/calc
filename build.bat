@echo off
cl /nologo /Z7 /Od calc.c /Fe:calc.exe && buildnum CALC_BUILD.h CALC
rem cl /nologo /O2 calc.c /Fe:calc.exe