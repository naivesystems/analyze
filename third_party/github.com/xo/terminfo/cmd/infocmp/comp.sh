#!/bin/bash

set -e

NAME=$1

TERM=$NAME infocmp -1 -L -x > a.txt

go build && TERM=$NAME ./infocmp > b.txt

$HOME/src/misc/icdiff/icdiff a.txt b.txt
