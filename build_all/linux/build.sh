#!/bin/bash
#set -o pipefail
#

set -e

script_dir=$(cd "$(dirname "$0")" && pwd)
build_root=$(cd "${script_dir}/../.." && pwd)
run_unit_tests=ON
use_wsio=OFF
run_valgrind=0
build_folder=$build_root"/cmake/uamqp_linux"
run_unittests=OFF

usage ()
{
    echo "build.sh [options]"
    echo "options"
    echo " -cl, --compileoption <value>  specify a compile option to be passed to gcc"
    echo "   Example: -cl -O1 -cl ..."
	echo " --use-websockets              Enables the support for AMQP over WebSockets."
	echo "-rv, --run_valgrind will execute ctest with valgrind"
    echo "--run-unittests run the unit tests"
    echo ""
    exit 1
}

sync_dependencies ()
{
    sharedutildir=/usr/include/azuresharedutil
    # check to see if the file exists
    if [ ! -d "$sharedutildir" ]; 
    then
        read -p "The required Azure Shared Utility does not exist would you like to install the component  (y/n)?" input_var
        # download the shared file
        if [ "$input_var" == "y" ] || [ "$input_var" == "Y" ]
        then
            echo "preparing Azure Shared Utility"
        else
            exit 1
        fi
        
        rm -r -f ~/azure-c-shared-utility
        git clone https://github.com/Azure/azure-c-shared-utility.git ~/azure-c-shared-utility
        bash ~/azure-c-shared-utility/build_all/linux/build.sh -i
    fi
}

process_args ()
{
    save_next_arg=0
    extracloptions=" "

    for arg in $*
    do      
      if [ $save_next_arg == 1 ]
      then
        # save arg to pass to gcc
        extracloptions="$arg $extracloptions"
        save_next_arg=0
      else
          case "$arg" in
              "-cl" | "--compileoption" ) save_next_arg=1;;
			  "--use-websockets" ) use_wsio=ON;;
			  "-rv" | "--run_valgrind" ) run_valgrind=1;;
              "--run-unittests" ) run_unittests=ON;;
              * ) usage;;
          esac
      fi
    done
}

process_args $*

rm -r -f $build_folder
mkdir -p $build_folder
pushd $build_folder
cmake -DcompileOption_C:STRING="$extracloptions" -Duse_wsio:BOOL=$use_wsio -Drun_valgrind:BOOL=$run_valgrind $build_root -Drun_unittests:BOOL=$run_unittests
make --jobs=$(nproc)

if [[ $run_valgrind == 1 ]] ;
then
	#use doctored openssl
	export LD_LIBRARY_PATH=/usr/local/ssl/lib
	ctest -j $(nproc) --output-on-failure
	export LD_LIBRARY_PATH=
else
	ctest -j $(nproc) -C "Debug" --output-on-failure
fi

popd
