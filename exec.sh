#!/bin/bash



baseImageDir="images/base"
compImageDir="images/comp"
grayImageDir="images/gray"


shopt -s nullglob
baseImageFilenames=(images/base/*)
shopt -u nullglob

app_path="./mpi_grayscale"
num_procs=$1




# mpiexec -n "$num_procs" "$app_path" "./images/base/test.png" "./images/comp/test.png" "./images/gray/test.png"
# mpiexec -n "$num_procs" "$app_path" "./images/base/test2.png" "./images/comp/test2.png" "./images/gray/test2.png"
# mpiexec -n "$num_procs" "$app_path" "./images/base/mountain.jpg" "./images/comp/mountain.jpg" "./images/gray/mountain.jpg"
for image in ${baseImageFilenames[@]}; do
  # echo "${image//$baseImageDir/$compImageDir}" "${image/$baseImageDir/$grayImageDir}"
  mpiexec -n "$num_procs" "$app_path" "${image}" "${image//$baseImageDir/$compImageDir}" "${image/$baseImageDir/$grayImageDir}"
done
