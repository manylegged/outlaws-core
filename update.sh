#!/bin/sh

# update this github repo from the central game repo

outlaws=~/Documents/outlaws
cp -v $outlaws/core/* .
mkdir -p os/win32 os/osx os/sdl os/linux
cp -v $outlaws/win32/Outlaws2/Main/{Main.cpp,platformincludes.h} os/win32/
cp -v $outlaws/osx/Outlaws/*.{m,h} os/osx/
cp -v $outlaws/linux/src/* os/linux/
cp -v $outlaws/sdl_os/* os/sdl/

hg status
