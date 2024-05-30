script="scratch/selcan/main"  # Your NS-3 script path without the .cc extension

nDevices=50                   # Number of devices


folder="/home/selcan/ns-allinone-3.41/ns-3.41/scratch/experiments/Test_${nDevices}/run"

# Move to the NS-3 directory
cd /home/selcan/ns-allinone-3.41/ns-3.41/scratch/build

# Configure and build NS-3 using CMake and Make
cmake ..
make

echo -n "Running experiments: "

for r in $(seq 1 10); do

  echo -n " $r"
  # nDevices=$((initial_nDevices + r * 40))
  mkdir -p "${folder}${r}"
  # Run the NS-3 script with the specified parameters
  ./selcan --RngRun="$r" --nDevices="$nDevices" --OutputFolder="${folder}${r}" > "${folder}${r}/log.txt" 2>&1
done

echo " END"
