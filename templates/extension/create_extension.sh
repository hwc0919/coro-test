#!/bin/bash

if [ $# -ne 2 ]; then
    echo "Usage: ./create_extension.sh <name> <output_dir>"
    exit 1
fi

NAME=$(echo "$1" | tr -cd '[:alnum:]-_')
if [ "$NAME" != "$1" ] || [ -z "$NAME" ]; then
    echo "Error: Invalid name. Use only letters, numbers, hyphens, and underscores."
    exit 1
fi
OUTPUT_DIR=$2
CLASS=$(echo ${NAME:0:1} | tr '[:lower:]' '[:upper:]')${NAME:1}
DIR="$OUTPUT_DIR/nitrocoro-$NAME"

mkdir -p "$DIR/include/nitrocoro/$NAME"
mkdir -p "$DIR/src"
mkdir -p "$DIR/tests"
mkdir -p "$DIR/cmake"

SCRIPT_DIR=$(dirname "$0")
sed "s/{{NAME}}/$NAME/g; s/{{CLASS}}/$CLASS/g" "$SCRIPT_DIR/CMakeLists.txt.template" > "$DIR/CMakeLists.txt"
sed "s/{{NAME}}/$NAME/g; s/{{CLASS}}/$CLASS/g" "$SCRIPT_DIR/README.md.template" > "$DIR/README.md"
sed "s/{{NAME}}/$NAME/g; s/{{CLASS}}/$CLASS/g" "$SCRIPT_DIR/Extension.h.template" > "$DIR/include/nitrocoro/$NAME/$CLASS.h"
sed "s/{{NAME}}/$NAME/g; s/{{CLASS}}/$CLASS/g" "$SCRIPT_DIR/Extension.cc.template" > "$DIR/src/$CLASS.cc"
sed "s/{{NAME}}/$NAME/g; s/{{CLASS}}/$CLASS/g" "$SCRIPT_DIR/test.cc.template" > "$DIR/tests/${NAME}_test.cc"
sed "s/{{NAME}}/$NAME/g; s/{{CLASS}}/$CLASS/g" "$SCRIPT_DIR/cmake/nitrocoro-{{NAME}}Config.cmake.in" > "$DIR/cmake/nitrocoro-${NAME}Config.cmake.in"
cp "$SCRIPT_DIR/../../.gitignore" "$DIR/.gitignore"

echo "Created template project $DIR/"
