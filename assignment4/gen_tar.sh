#!/bin/bash
if [ -z "$1" ]; then
	echo "gen_tar.sh requires 1 argument!"
	echo "Usage: ./gen_tar.sh {studentID}"
	echo "ex) ./gen_tar.sh 2023314102"
	exit 1
fi
make clean
USER_ID="$1"
if [ -d "${USER_ID}" ]; then
	echo "Folder: \"${USER_ID}\" exists. Please rename or remove it"
	exit 1
fi
if [ -e "${USER_ID}.tar" ]; then
	rm "${USER_ID}.tar"
fi
mkdir "${USER_ID}"
cp Search.h "${USER_ID}"
cp Search.cpp "${USER_ID}"
tar -cvf "${USER_ID}.tar" "${USER_ID}"
rm -r "${USER_ID}"
