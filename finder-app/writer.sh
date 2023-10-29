#!/bin/bash

set +e

if [ $# -ne 2 ]; then
  echo "Usage: $0 <write_file> <write_str>" >&2
  exit 1
fi

writefile=$1
writestr=$2
writedir=$(dirname $writefile)

mkdir -p "$writedir"
if [ $? -ne 0 ]; then
  echo "Failed to create directory $writedir" >&2
  exit 1
fi

echo "$writestr" > "$writefile"
if [ $? -ne 0 ]; then
  echo "Failed to write file $writefile" >&2
  exit 1
fi

exit 0
