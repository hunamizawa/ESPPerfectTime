#!/bin/bash

# The MIT License (MIT)
#
# Copyright (c) 2015 tzapu
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

function build_examples()
{
  excludes=("$@")
  # track the exit code for this platform
  local exit_code=0
  # loop through results and add them to the array
  examples=($(find $PWD/examples/ -name "*.pde" -o -name "*.ino"))

  # get the last example in the array
  local last="${examples[@]:(-1)}"

  # loop through example sketches
  for example in "${examples[@]}"; do

    # store the full path to the example's sketch directory
    local example_dir=$(dirname $example)

    # store the filename for the example without the path
    local example_file=$(basename $example)

    # skip files listed as excludes
    for exclude in "${excludes[@]}"; do
        if [ "${example_file}" == "${exclude}" ] ; then
            echo ">>>>>>>>>>>>>>>>>>>>>>>> Skipping ${example_file} <<<<<<<<<<<<<<<<<<<<<<<<<<"
            continue 2
        fi
    done
    
    echo "$example_file: "
    local sketch="$example_dir/$example_file"
    echo "$sketch"
    #arduino -v --verbose-build --verify $sketch

    # verify the example, and save stdout & stderr to a variable
    # we have to avoid reading the exit code of local:
    # "when declaring a local variable in a function, the local acts as a command in its own right"
    local build_stdout
    build_stdout=$(arduino --verify $sketch 2>&1)

    # echo output if the build failed
    if [ $? -ne 0 ]; then
      # heavy X
      echo -e "\xe2\x9c\x96"
      echo -e "----------------------------- DEBUG OUTPUT -----------------------------\n"
      echo "$build_stdout"
      echo -e "\n------------------------------------------------------------------------\n"

      # mark as fail
      exit_code=1

    else
      # heavy checkmark
      echo -e "\xe2\x9c\x93"
    fi
  done

  return $exit_code
}
