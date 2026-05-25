#!/usr/bin/env bash
set -e

if [[ "$GITHUB_REF" == refs/tags/* ]]
then
    # Tag push: always extract BUILD_NUMBER from tag, regardless of
    # workflow_dispatch input defaults (which would be "9999").
    BUILD_NUMBER="${GITHUB_REF#refs/tags/}"
    YEAR="$(date +"%Y")"
else
    : "${YEAR:=$(date +"%Y")}"
    : "${BUILD_NUMBER:=$GITHUB_RUN_NUMBER}"
fi

{
    echo "BUILD_NUMBER=$BUILD_NUMBER"
    echo "BUILD=$BUILD_NUMBER"
    echo "YEAR=$YEAR"
    echo "MONTH=$(date +"%-m")"
    echo "DAY=$(date +"%-d")"
 } >> "$GITHUB_ENV"
