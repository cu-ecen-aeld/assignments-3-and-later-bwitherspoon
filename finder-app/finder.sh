#!/bin/bash

set +e

if [ $# -ne 2 ]; then
  echo "Usage: $0 <files_dir> <search_str>" >&2
  exit 1
fi

if ! [ -d "$1" ]; then
  echo "Error: $1 is not a directory" >&2
  exit 1
fi

numfiles=$(grep --recursive --only-matching --no-filename $2 $1 | wc -l)
numlines=$(grep --recursive --files-with-matches $2 $1 | wc -l)

echo "The number of files are $numfiles and the number of matching lines are $numlines"

exit 0
