#!/usr/bin/env sh

RELEASE_BUILD=false
NIGHTLY_BUILD=false
BUILD_DEPLOYMENT=false
BUILD_CONFIGS="Release FastDebug"
VERSION_NAME=""
PACKAGE_NAME=""

RELEASE_PATTERN="^release_(.*)$"
NIGHTLY_PATTERN="^nightly_(.*)$"

# These are for testing
NIGHTLY_TEST=false
RELEASE_TEST=false

if [ "$RELEASE_TEST" = true ] || [[ "$TRAVIS_TAG" =~ $RELEASE_PATTERN ]]; then
    echo "This is a release tag!";
    RELEASE_BUILD=true;
	PACKAGE_NAME="fs2_open_${BASH_REMATCH[1]}";
fi
if [ "$NIGHTLY_TEST" = true ] || [[ "$TRAVIS_TAG" =~ $NIGHTLY_PATTERN ]]; then
    echo "This is a nightly tag!";
    NIGHTLY_BUILD=true;
    VERSION_NAME="${BASH_REMATCH[1]}";
	PACKAGE_NAME="nightly_${BASH_REMATCH[1]}";
fi

if ([ "$RELEASE_BUILD" = true ] || [ "$NIGHTLY_BUILD" = true ]); then
    BUILD_DEPLOYMENT=true;
fi

if [ "$BUILD_DEPLOYMENT" = true ]; then
    if ([ "$TRAVIS_OS_NAME" = "linux" ] && (! [[ "$CC" =~ ^gcc.*$ ]])); then
        echo "Skipping non-release compiler";
        exit 0;
    fi

    if ([[ "$CONFIGURATION" == "Debug" ]]); then
        echo "Skipping non-release configuration";
        exit 0;
    fi
fi

export RELEASE_BUILD
export NIGHTLY_BUILD
export BUILD_DEPLOYMENT
export BUILD_CONFIGS
export VERSION_NAME
export PACKAGE_NAME
