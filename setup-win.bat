@echo off

ECHO Initializing git submodules...
git submodule init
git submodule update

ECHO Starting setup application...
start .\setup\wowpp-setup.exe
