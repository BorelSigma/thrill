rm /tmp/thrillGroup2
cd build_manpen
make -j1

if [ $? -ne 0 ] ; then
	echo "Error: Building failed"
	exit 1
fi

echo "Nach kompilieren"

cd ..

mpirun -n 1 -hostfile hostfile build_manpen/examples/list_rank/list_rank_run
#build_manpen/examples/list_rank/list_rank_run
