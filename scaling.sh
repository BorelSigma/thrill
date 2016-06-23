# cd build_manpen
# make -j2

if [ $? -ne 0 ] ; then
	echo "Error: Building failed"
	exit 1
fi

# cd ..

n0=20
n=1000000
d=5

pus=(1 2 3 4 5 6 7 8 9 10)

for i in ${pus[*]}
do
	before=$(date +%s)
	mpirun -n $i -hostfile build_manpen/examples/graphgen2/hostfile build_manpen/examples/graphgen2/ba_generator -g 3 -s $n0 -n $n -d $d -o "BAClique.txt" >/dev/null
	after=$(date +%s)	
	elapsed_time[i]=$(($after-$before))
	echo $i "PUs needed:" ${elapsed_time[i]} "seconds"
done

echo "" > "strong_scaling.txt"

for i in ${pus[*]}
do
	#echo $i "PUs needed " ${elapsed_time[i]} "seconds"
	echo $i ${elapsed_time[i]} >> "strong_scaling.txt" 
done


gnuplot -e "set terminal png size 640,480; set output'strong_scaling.png'; set title'strong scaling'; set xrange[1:11]; set xlabel'number of PUs';set ylabel'time [s]';plot 'strong_scaling.txt' using 1:2 w lines title'';"

#elements=(1000000 2000000 3000000 4000000 5000000 6000000 7000000 8000000 9000000 10000000 11000000)

for i in ${pus[*]}
do
	before=$(date +%s)
	elements=$(($n*$i))
	mpirun -n $i -hostfile build_manpen/examples/graphgen2/hostfile build_manpen/examples/graphgen2/ba_generator -g 3 -s $n0 -n $elements -d $d -o "BAClique.txt" >/dev/null
	after=$(date +%s)	
	elapsed_time[i]=$(($after-$before))
	echo $i "PUs needed:" ${elapsed_time[i]} "seconds"
done

echo "" > "weak_scaling.txt"

for i in ${pus[*]}
do
	#echo $i "PUs needed " ${elapsed_time[i]} "seconds"
	echo $i ${elapsed_time[i]} >> "weak_scaling.txt" 
done


gnuplot -e "set terminal png size 640,480; set output'weak_scaling.png'; set title'weak scaling'; set xrange[1:11]; set xlabel'number of PUs';set ylabel'time [s]';plot 'weak_scaling.txt' using 1:2 w lines title'';"
