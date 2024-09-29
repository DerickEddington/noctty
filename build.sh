#!/usr/bin/env sh
set -o errexit -o nounset

readonly self="$0"
selfDir=$(dirname "$self") || exit
selfDir=$(realpath "$selfDir") || exit
readonly selfDir

commit=${1:-}
if [ -z "$commit" ]; then
    commit=$(cd "$selfDir" && git rev-parse --short HEAD) || exit
fi
readonly commit

set -o xtrace

${CC:-gcc} -Wall -Wextra -Wpedantic -pedantic-errors \
           -Og -ggdb \
           -D __COMMIT__="\"$commit\"" \
           -o noctty "$selfDir"/noctty.c
