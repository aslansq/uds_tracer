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

cd "$thisDirPath" || ungracefulExit "Failed to change directory to $thisDirPath"

# Execute the gen.sh script
./uds_py/build_run.sh || ungracefulExit "Failed to execute build_run.sh for uds_py"
# Remove existing files in the uds/gen directory
rm -rf ./uds/gen/*
mkdir -p ./uds/gen
# Copy the generated files to the uds directory
cp ./uds_py/output/uds*  ./uds/gen

echo  Copied the generated files to the uds directory, successfully