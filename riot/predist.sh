#!/bin/sh

pwd=`pwd`
basedir=`basename "$pwd"`

date=`date +%Y%m%d`

echo "Setting release to $date in Riot/Version.hs"
perl -p -i -e "s/release = \"\d+\"/release = \"$date\"/" Riot/Version.hs
perl -p -i -e "s/version:            \d+/version:            1.$date/" riot.cabal
