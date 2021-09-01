declare -a arr=("AvgTopLvlPtsSize" "TotalNode" "TotalEdge")
for d in vfs-bench/bitcode/*; do
  if [[ $d == *"bc"* ]]; then
    echo "$d"
    wpa -fspta -alias-check=false /mnt/c/Users/User/Desktop/capstone2/lukes_forks/SVF/$d > file1
    wpa -fspta -alias-check=false /mnt/c/Users/User/Desktop/capstone2/originalSVF/SVF/$d > file2
    for i in "${arr[@]}"; do
      #   echo ">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>$i"
        grep -h "$i" file1 > output1
        grep -h "$i" file2 > output2
        #   cat output1
        #   echo ">>>>>>>>>>>>>>>>>>>>>>>"
        #   cat output2
        diff output1 output2
        rm output1 output2
    done
    rm file1 file2 
  fi
done
