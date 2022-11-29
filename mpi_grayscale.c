#include <stdio.h>
#include <stdlib.h>

#include "log/log.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb/stb_image_write.h"

#include <mpi.h>

#include <sys/types.h>
#include <sys/stat.h>

#define BUFFER 100
#define MANAGER_CORE 0

#define __DEBUG__ 1

int main(int argc, char *argv[])
{
  typedef enum filetype
  {
    PNG,
    JPG,
    BMP
  } filetype_t;

#if __DEBUG__ == 1
  FILE *logFile = fopen("log.txt", "w");
  log_add_fp(logFile, LOG_TRACE);
  log_set_quiet(true);
#endif

  // * Variables *
  double start, elapsed;
  int nproc, rank;
  int width, height, channels;
  int readHeight, readWidth;
  char *originalFileName, *compressedFileName, *grayscaleFileName;
  struct stat preCompSb, postCompSb;

  uint8_t *readImg, *img, *cImg, *gImg;
  filetype_t ftype;

  MPI_Init(&argc, &argv); /* Intialize MPI*/
  MPI_Comm comm = MPI_COMM_WORLD;

  MPI_Comm_size(comm, &nproc);
  MPI_Comm_rank(comm, &rank);


  // /* handling command line args*/

  if (rank == 0)
  {
    if (argc < 4)
    {
      fprintf(stderr, "Usage mpiexec -n <# of processes> ./mpi_grayscale <image_file> <compressed_image_dest> <greyscale_image_dest>\n");
      exit(EXIT_FAILURE);
    }
    else
    {
      originalFileName = argv[1];
      compressedFileName = argv[2];
      grayscaleFileName = argv[3];
    }
    char *dot = strrchr(originalFileName, '.');
    if (strcmp(dot, ".png") == 0)
    {
      ftype = PNG;
    }
    else if (strcmp(dot, ".jpg") == 0)
    {
      ftype = JPG;
    }
    else if (strcmp(dot, ".bmp") == 0)
    {
      ftype = BMP;
    }

    // *** TODO: Handle odd dimensions
    // *** TODO: Handle dimensions that don't play nicely with nproc and comp_value values
    stat(originalFileName, &preCompSb);
    readImg = stbi_load(originalFileName, &readWidth, &readHeight, &channels, 0);
    printf("\n\nLoaded image with a width of %dpx, a height of %dpx and %d channels\n", readWidth, readHeight, channels);
    printf("Number procs: %d\n", nproc);

    height = (readHeight/2)*2;
    width = (readWidth/2)*2;




    if (readImg == NULL)
    {
      printf("Error reading image, exiting...");
#if __DEBUG__ == 1
      log_error("Error reading image - Filename: %s", originalFileName);
#endif
      MPI_Abort(comm, EXIT_FAILURE);
      return EXIT_FAILURE;
    }




#if __DEBUG__ == 1
    log_trace("rank 0 - immediately after reading image - height: %d, width: %d, channels: %d\n", height, width, channels);
#endif
  }






  // * Wait for 0 to finish reading and allocating memory *
  MPI_Barrier(comm);

  // * Broadcast metadata *
  MPI_Bcast(&readHeight, 1, MPI_INT, 0, comm);
  MPI_Bcast(&readWidth, 1, MPI_INT, 0, comm);
  MPI_Bcast(&width, 1, MPI_INT, 0, comm); // shouldn't hurt perf even though it's message passing since it's such little data
  MPI_Bcast(&height, 1, MPI_INT, 0, comm);
  MPI_Bcast(&channels, 1, MPI_INT, 0, comm);
  MPI_Barrier(comm);

  // * Declare windows
  MPI_Win imgWindow, cImgWindow, gImgWindow;

  // * size calculations
  int disp_unit = sizeof(uint8_t); // will need to make this dyanmic if we decide to take non-8-bit colors
  int imgSize = width * height * channels, cImgSize = imgSize / 4, gImgSize = imgSize / (channels * 4);
  MPI_Aint aintImg, aintCImg, aintGImg;
  // TODOS: Need to handle case where images have an odd*odd, odd*(evenNotDivisibleBy4) where left over pixels will result from calculation and
  // TODOS: as such will screw with size calculations as well.

  // * create windows *

  // printf("%d %d %d\n", imgSize, cImgSize, gImgSize);
  MPI_Win_allocate_shared((rank == 0) ? imgSize : 0, disp_unit, MPI_INFO_NULL, comm, &img, &imgWindow);
  MPI_Win_allocate_shared((rank == 0) ? cImgSize : 0, disp_unit, MPI_INFO_NULL, comm, &cImg, &cImgWindow);
  MPI_Win_allocate_shared((rank == 0) ? gImgSize : 0, disp_unit, MPI_INFO_NULL, comm, &gImg, &gImgWindow);
  
  MPI_Barrier(comm);
  if (rank != 0)
  {
    MPI_Win_shared_query(imgWindow, 0, &aintImg, &disp_unit, &img);
    MPI_Win_shared_query(cImgWindow, 0, &aintCImg, &disp_unit, &cImg);
    MPI_Win_shared_query(gImgWindow, 0, &aintGImg, &disp_unit, &gImg);
  }

  MPI_Barrier(comm);

  if (rank == 0)
  {

    for(int i = 0; i<height; i++){
      for(int j = 0; j<width; j++){
        for(int k = 0; k<channels; k++){
          img[i*width*channels + j*channels + k] = readImg[i*readWidth*channels + j*channels + k];
        }
      }
    }
    free(readImg);

#if __DEBUG__ == 1
    log_trace("imgSize: %d bytes\tcImgSize %d bytes\tgImgSize: %d bytes", imgSize, cImgSize, gImgSize);
#endif
  }
  
  MPI_Barrier(comm);

  int hmod = height % nproc;
  int hdiv = height / nproc;
  int work_height_start = (rank >= hmod) ? ((hmod) * (hdiv + 1) + (rank - hmod) * hdiv) : rank * (hdiv + 1);
  int work_height_end = work_height_start + ((rank >= hmod) ? hdiv : hdiv + 1);

  int cImgWidth = width/2;
  int cImgHeight = height/2;

  uint32_t *ch_temp = calloc(channels, sizeof(uint32_t));

  int index = 0;

  
  

  start = MPI_Wtime();

  // for (int i = work_height_start/comp_value; i < work_height_end/comp_value; i++)
  // { // height where work is being done at in incremenets of comp_value
  //   for (int j = 0; j < width/comp_value; j++)
  //   { // width where work is being done at in increments of comp_value
  //     for (int k = 0; k < comp_value; k++)
  //     { // iterate over y axis of comp_value
  //       for (int a = 0; a < comp_value; a++)
  //       { // iterate over x axis of comp_value
  //         for (int ch = 0; ch < channels; ch++)
  //         { // iterate over each channel
  //           index = i * comp_value * width * channels + j*comp_value*channels + k*width*channels + a*channels + ch;
  //           ch_temp[ch] += img[index];
  //         }
  //       }
  //     }
  //     for (int ch = 0; ch < channels; ch++)
  //     {
  //       cImg[(i*(width/comp_value) + j)*channels + ch] = (uint8_t)(ch_temp[ch] / comp_value_block);
  //       ch_temp[ch] = 0;
  //     }
  //   }
  // }

  if(channels == 3){
    for (int i = work_height_start/2; i < work_height_end/2; i++)
    { // height where work is being done at in incremenets of comp_value
      for (int j = 0; j < width/2; j++)
      { // width where work is being done at in increments of comp_value
        cImg[(i*(cImgWidth) + j)*channels] = (img[2*channels*((i*width) + j)] + img[2*channels*((i*width) + j + 1)] + img[2*channels*(((i+1)*width) + j)] + img[2*channels*(((i+1)*width) + (j+1))])/4; 
        cImg[(i*(cImgWidth) + j)*channels + 1] = (img[2*channels*((i*width) + j) + 1] + img[2*channels*((i*width) + j + 1) + 1] + img[2*channels*(((i+1)*width) + j) + 1] + img[2*channels*(((i+1)*width) + (j+1)) + 1])/4; 
        cImg[(i*(cImgWidth) + j)*channels + 2] = (img[2*channels*((i*width) + j) + 2] + img[2*channels*((i*width) + j + 1) + 2] + img[2*channels*(((i+1)*width) + j) + 2] + img[2*channels*(((i+1)*width) + (j+1)) + 2])/4;
      }
    }
  } else if (channels == 2){
    for (int i = work_height_start/2; i < work_height_end/2; i++)
    { // height where work is being done at in incremenets of comp_value
      for (int j = 0; j < width/2; j++)
      { // width where work is being done at in increments of comp_value
        cImg[(i*(cImgWidth) + j)*channels] = (img[2*channels*((i*width) + j)] + img[2*channels*((i*width) + j + 1)] + img[2*channels*(((i+1)*width) + j)] + img[2*channels*(((i+1)*width) + (j+1))])/4; 
        cImg[(i*(cImgWidth) + j)*channels + 1] = (img[2*channels*((i*width) + j) + 1] + img[2*channels*((i*width) + j + 1) + 1] + img[2*channels*(((i+1)*width) + j) + 1] + img[2*channels*(((i+1)*width) + (j+1)) + 1])/4; 
      }
    }
  }
  else if (channels == 1){
  for (int i = work_height_start/2; i < work_height_end/2; i++)
    { // height where work is being done at in incremenets of comp_value
    for (int j = 0; j < width/2; j++)
      { // width where work is being done at in increments of comp_value
        cImg[(i*(cImgWidth) + j)*channels] = (img[2*channels*((i*width) + j)] + img[2*channels*((i*width) + j + 1)] + img[2*channels*(((i+1)*width) + j)] + img[2*channels*(((i+1)*width) + (j+1))])/4; 
      }
    }
  }
  


  hmod = cImgHeight % nproc;
  hdiv = cImgHeight / nproc;
  work_height_start /= 2;
  work_height_end /= 2;

  for (int i = work_height_start; i < work_height_end; i++)
  {
    for (int j = 0; j < cImgWidth; j++)
    {
      int sum = 0;
      for (int ch = 0; ch < channels; ch++)
      {
        sum += cImg[(i * cImgWidth + j) * channels + ch];
      }
      gImg[i * cImgWidth + j] = sum / channels;
    }
  }

  elapsed = MPI_Wtime();
  




  // write resulting images
  if (rank == 0)
  {

    
   
    if (ftype == PNG)
    {
      stbi_write_png(compressedFileName, cImgWidth, cImgHeight, channels, cImg, 0);
      stbi_write_png(grayscaleFileName, cImgWidth, cImgHeight, 1, gImg, 0);
    }
    else if (ftype == JPG)
    {
      stbi_write_jpg(compressedFileName, cImgWidth, cImgHeight, channels, cImg, 100);
      stbi_write_jpg(grayscaleFileName, cImgWidth, cImgHeight, 1, gImg, 100);
    }
    else if (ftype == BMP)
    {
      stbi_write_bmp(compressedFileName, cImgWidth, cImgHeight, channels, cImg);
      stbi_write_bmp(grayscaleFileName, cImgWidth, cImgHeight, 1, gImg);
    }
    stat(grayscaleFileName, &postCompSb);
    printf("Filename: %s\nPre-compression size: %d B\nPost-compression size: %d B\nTime: %f\n", originalFileName, preCompSb.st_size, postCompSb.st_size, elapsed);
  }
  MPI_Win_free(&imgWindow);
  MPI_Win_free(&cImgWindow);
  MPI_Win_free(&gImgWindow);
  MPI_Finalize();

  return 0;
}