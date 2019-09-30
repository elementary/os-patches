#!/bin/sh
#
# usage: ./download-unicode-files.sh DIRECTORY
# downloads following files from unicode.org to DIRECTORY or unicode/ (if
# DIRECTORY is not presented):
#  - UnicodeData.txt
#  - Unihan.zip
#  - NamesList.txt
#  - Blocks.txt
#  - Scripts.txt
#  - DerivedAge.txt
#

FILES='UnicodeData.txt Unihan.zip NamesList.txt Blocks.txt Scripts.txt DerivedAge.txt'

mkdir -p ${1:-unicode} 

for x in $FILES; do
	wget "http://www.unicode.org/Public/UNIDATA/$x" -O "${1:-unicode}/$x"
done

echo 'Done.'

