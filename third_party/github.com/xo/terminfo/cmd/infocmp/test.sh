#!/bin/bash

set -e

go build

SI=$(which infocmp)
BI=./infocmp

OUT=.out

mkdir -p $OUT

TERMS=$(find /usr/share/terminfo -type f)
for i in $TERMS; do
  NAME=$(basename $i)

  TERM=$NAME $SI -1 -L -x > $OUT/$NAME-orig.txt
  TERM=$NAME $BI -x > $OUT/$NAME-test.txt

  if [[ "$1" == "--no-acs-chars" ]]; then
    perl -pi -e 's/^\tacs_chars=.*\n//' $OUT/$NAME-orig.txt $OUT/$NAME-test.txt
  fi
done

md5sum $OUT/*-orig.txt > $OUT/orig.md5

cp $OUT/orig.md5 $OUT/test.md5

perl -pi -e 's/-orig\.txt$/-test.txt/g' $OUT/test.md5

SED=sed
if [[ "$(uname)" == "Darwin" ]]; then
  SED=gsed
fi

for i in $(md5sum -c $OUT/test.md5 |grep FAILED|$SED -s 's/: FAILED//'); do
  $HOME/src/misc/icdiff/icdiff $($SED -e 's/-test\.txt$/-orig.txt/' <<< "$i") $i
done
