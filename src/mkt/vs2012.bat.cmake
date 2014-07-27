@echo off

call "C:\Program Files (x86)\Microsoft Visual Studio 11.0\VC\vcvarsall.bat" amd64
set PATH=${BOOST_LOCATION};%PATH%
devenv trans-mkt.sln
