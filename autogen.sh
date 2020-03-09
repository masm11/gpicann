#!/bin/bash

set -ex

aclocal
automake --add-missing
./configure "$@"
