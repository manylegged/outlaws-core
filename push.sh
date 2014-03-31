#!/bin/sh

# hg-git push script

/opt/local/bin/hg bookmark -r default master # make a bookmark of master for default, so a ref gets created
/opt/local/bin/hg push
