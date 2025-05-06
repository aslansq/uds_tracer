#!/bin/bash

thisPath=$(realpath "$0")
thisDirPath=$(dirname "$thisPath")

echoerr() { echo "$@" 1>&2; }

ungracefulExit()
{
    echoerr "$@"
    echoerr ERROR!! $(pwd)
    exit 1
}

cd "$thisDirPath" || ungracefulExit changing directory to $thisDirPath failed

rm -rf ./output
mkdir -p ./output

python3 uds.py || ungracefulExit running uds.py failed

echo -e "#include \"uds_def.h\"\n int main(void) { return 0; }" > ./output/main.cpp || ungracefulExit creating main.cpp failed

rm -rf build
mkdir -p build
cd build || ungracefulExit cd to build failed

qmake .. || ungracefulExit creating qmake files failed
make || ungracefulExit building project failed

echo "uds_py build completed successfully."