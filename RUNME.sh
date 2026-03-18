#!/bin/bash
# artifact list
sat4j_solver="./sat4j/target\
/org.sat4j.moco.threeAlgorithms-0.0.1-SNAPSHOT-jar-with-dependencies.jar"
openwbo_solver="./open-wbo"
conf_budget=300
wl_type=0
function print_help() {
    if [ -z $1 ] || [ -z $2 ]
then
    cat <<EOF
usage:
./run_me.sh <algorithm> <instance> [budget of conflicts] [waiting list type]
<algorithm> is one of the following,
	pmcs, pmin, hs, us, us-pmin, usl, usl-pmin;

<instance> is the path for a valid instance. If the path terminates in
.xz or .tar.gz, the file will be decompressed; 

The third and fourth arguments are optional and only meaningful for
some algortihms.  The third controls the budget of conflicts, the
fourth the type of the waiting list.

For instance, run me like this:
    ./RUNME.sh sd examples/DAL1.pbmo
or:
    ./RUNME.sh lbr examples/DAL1.pbmo 100 0
EOF
    exit 1;
fi
}


function main(){

    algorithm=$1
    instance=$2
    unset '$2'

    if [ ! -z $3 ] 
    then
	conf_budget=$3
    fi

    if [ ! -z $4 ] 
    then
       wl_type=$4
    fi

    case $instance in
	*.xz)
	    echo "c decompressing xz file..."
	    tmp=$(mmktemp ${instance/.xz//})
	    unxz --stdout $instance > $tmp
	    instance=$tmp
	    echo "c done: instance in $tmp"
	    ;;
	*.gz)
	    
	    tmp=$(mmktemp ${instance/.gz//})
	    echo "decompressing gz file..."
	    zcat $instance > $tmp
	    instance=$tmp
	    echo "c done: instance in $tmp"
	    ;;
	*)
	    echo "c no decompression required"
	    ;;
    esac
    case $1 in
	pmcs)
	    java -jar $sat4j_solver -alg 0 $instance ;;
	pmin)
	    $openwbo_solver -cardinality=1 -pb=2 -no-bmo \
			    -formula=1 -algorithm=8 -pbobjf=4 -eps=1\
			    -apmode=1 -no-cubounds -no-clbounds $instance ;;
	hs)
	    $openwbo_solver -cardinality=1 -pb=2 -no-bmo \
			    -formula=1 -algorithm=9 -pbobjf=4 -eps=1\
			    -apmode=1 -no-cubounds -no-clbounds $instance ;;
	us)
	    $openwbo_solver -cardinality=1 -pb=2 -no-bmo \
			    -formula=1 -algorithm=7 -pbobjf=4 -eps=1\
			    -apmode=1 -no-cubounds -no-clbounds $instance ;;
	uspmin)
	    $openwbo_solver -cardinality=1 -pb=2 -no-bmo \
			    -formula=1 -algorithm=17 -pbobjf=4 -eps=1\
			    -apmode=1 -no-cubounds -no-clbounds  -conf_budget=$conf_budget $instance ;;
	sd)
	    $openwbo_solver -cardinality=1 -pb=2 -no-bmo \
			    -formula=1 -algorithm=18 -pbobjf=4 -eps=1 -conf_budget=$conf_budget -wl_type=$wl_type\
			    -apmode=1 -no-cubounds -no-clbounds -no-ascend $instance ;;
	lbr)
	    $openwbo_solver -cardinality=1 -pb=2  -no-bmo \
			    -formula=1 -geo_p=1 -no-block_below -algorithm=23 -pbobjf=4 -eps=1  -core_optim=2 \
			    -apmode=1 -no-cubounds -no-clbounds -conf_budget=$conf_budget -wl_type=$wl_type $instance ;;
	e-lbr)
	    $openwbo_solver -cardinality=1 -pb=2 -no-bmo \
			    -formula=1 -geo_p=1 -no-block_below -algorithm=23 -pbobjf=4 -eps=1  -core_optim=2 \
			    -apmode=1 -no-cubounds -no-clbounds -conf_budget=$conf_budget -wl_type=$wl_type $instance ;;
	portfolio)
#		gdb --args \
	    $openwbo_solver -cardinality=1 -pb=2 -no-bmo \
			    -formula=1 -geo_p=1 -no-block_below -algorithm=29 -pbobjf=4 -eps=1  -core_optim=2 \
			    -apmode=1 -no-cubounds -no-clbounds -conf_budget=$conf_budget -wl_type=$wl_type $instance ;;

   *)
	echo "Check name of the algorithm to run: \"$1\" is not valid."
	exit 1;;
esac
      if [ ! -z $tmp ] && [ -f $tmp ]; then
	  rm $tmp;
      fi
}

function check_artifacts() {
if  [ ! -f $openwbo_solver ]
   then
   echo "please compile openwbo."
   return 1;
fi
if  [ ! -f $sat4j_solver ]
   then
   echo "please compile sat4j."
   return 1;
fi


}
function mmktemp(){
    # creates tmp file name. Avoids thrashing over existent files
    local result=$(basename $1)
    local n=""
    while [ -f "/tmp/$n$result" ]; do
	n=$((n+1))
    done
    echo "/tmp/$n$result"
}

# live section: 
check_artifacts
print_help $@
main $@
