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

################################################################################
cd "$thisDirPath" || ungracefulExit "Failed to change directory to $thisDirPath"
rm -rf build
mkdir -p build
cd build || ungracefulExit "Failed to change gui build directory"
qmake ../*.pro || ungracefulExit "Failed to run qmake for gui"
make || ungracefulExit "Failed to build gui"

################################################################################
cd "$thisDirPath" || ungracefulExit "Failed to change directory to $thisDirPath"
rm -rf output
mkdir -p output
cp build/uds_tracer output || ungracefulExit "copying binaries failed"

echo "All successful."
