#!/bin/bash

if [ -z "$1" ]; then
    echo "Usage: ./gen_tar.sh <your_student_id>"
    echo "Example: ./gen_tar.sh 2024123456"
    exit 1
fi

STUDENT_ID=$1
TAR_NAME="${STUDENT_ID}.tar"

tar -cf "$TAR_NAME" Manager.cpp Manager.h
echo "Generated $TAR_NAME"
echo "Upload this file to LearnUs."
