#!/bin/sh -xe

HOSTNAME=$1

if [ -z ${HOSTNAME} ]; then
    echo "No hostname defined."
    echo
    echo "Usage:"
    echo
    echo "     $0 <hostname-inside-docker>"
    exit 1
fi

CWD=`dirname $0`
PWD=`pwd`
WORKSPACE=`realpath $PWD`

BUILD_ARGS="--build-arg uid=`id -u` --build-arg gid=`id -g` --build-arg USER=${USER}"
RUN_ARGS="-u `id -u`"

IMAGE_TAG="$HOSTNAME:devel"

docker build -f $CWD/Dockerfile -t $IMAGE_TAG $BUILD_ARGS "$CWD/artifacts/"

docker run --rm=true -it $RUN_ARGS -e LANG=en_US.UTF-8 \
    -h $HOSTNAME \
    -v $WORKSPACE:$WORKSPACE:rw \
    -v $HOME:$HOME:rw \
    -w $WORKSPACE \
	--cap-add=SYS_PTRACE \
    $IMAGE_TAG /bin/bash

