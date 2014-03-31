#!/bin/sh
/opt/logal/bin/hg bookmark -r default master # make a bookmark of master for default, so a ref gets created
/opt/local/bin/hg push
