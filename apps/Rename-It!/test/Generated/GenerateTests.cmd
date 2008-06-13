@echo off

rem Settings.
set cxxtest=..\..\lib\cxxtest\cxxtestgen.py
set input_dir=..
set output_dir=.
set runner_cpp=%output_dir%\Runner.cpp

rem Initialize.
set errors=0

rem For each unit test file...
for %%i in ("%input_dir%\*.h") do call :GenerateTestFile "%%i" "%output_dir%\%%~ni.cpp"

rem Generate global test runner.
%cxxtest% -o "%runner_cpp%" --runner=ParenPrinter --gui=Win32Gui --root --include=StdAfx.h

rem Exit
if not %errors% == 0 exit /B %errors%
goto :eof

rem -------------------------------------------------------------------------------
:GenerateTestFile

rem Generate the test.
%cxxtest% -o "%~2.tmp" --part --include=StdAfx.h "%~1"
if errorlevel 1 echo Failed to generate test for: "%~1" & set errors=%errorlevel% & exit /B %errorlevel%

rem Compare with the existing file.
echo N | comp "%~2.tmp" "%~2" >NUL 2>NUL

rem If the files don't match, replace the existing one.
if errorlevel 1 copy "%~2.tmp" "%~2" /y >NUL & echo Generated test for: "%~1"

rem Remove the temporary file.
del "%~2.tmp"
goto :eof
