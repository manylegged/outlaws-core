#!/bin/sh

# update this github repo from the central game repo

outlaws=~/Documents/outlaws
cp -v $outlaws/core/* .
mkdir -p os/win32 os/osx os/sdl os/linux
cp -v $outlaws/win32/Outlaws2/Main/{Win32Main.cpp,platformincludes.h} os/win32/
cp -v $outlaws/osx/Outlaws/*.{m,h} os/osx/
cp -v $outlaws/linux/src/* os/linux/
cp -v $outlaws/sdl_os/* os/sdl/
mkdir -p data
cp $outlaws/data/shaders.lua data/
cp -rv $outlaws/data/shaders/ data/
cp ~/.lldbinit .

mkdir -pv scripts
cp -v $outlaws/scripts/ccount.py scripts/
cp -v $outlaws/scripts/decluster.py scripts/
cp -v $outlaws/scripts/etags.py scripts/
cp -v $outlaws/scripts/filter_*.py scripts/
cp -v $outlaws/scripts/langtool.py scripts/
cp -v $outlaws/scripts/stack_lookup.py scripts/
cp -v $outlaws/scripts/nmsize.py scripts/

/opt/local/bin/hg status
