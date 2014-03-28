#!/bin/sh
hg bookmark -r default master # make a bookmark of master for default, so a ref gets created
hg push
