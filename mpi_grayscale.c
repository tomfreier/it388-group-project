#include <stdio.h>
#include <stdlib.h>

#include "log/log.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb/stb_image_write.h"

#include <mpi.h>
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
  int width, height, channels, comp_value = 2, comp_value_block = comp_value * comp_value;
  char *originalFileName, *compressedFileName, *grayscaleFileName;
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

    readImg = stbi_load(originalFileName, &width, &height, &channels, 0);

    height = (height / comp_value) * comp_value;
    width = (width / comp_value) * comp_value;



    if (height % (nproc * comp_value) != 0)
    {
      printf("Unable to process image with given dimensions");
      MPI_Abort(comm, EXIT_FAILURE);
      return EXIT_FAILURE;
    }






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
  MPI_Bcast(&width, 1, MPI_INT, 0, comm); // shouldn't hurt perf even though it's message passing since it's such little data
  MPI_Bcast(&height, 1, MPI_INT, 0, comm);
  MPI_Bcast(&channels, 1, MPI_INT, 0, comm);
  MPI_Bcast(&comp_value, 1, MPI_INT, 0, comm);
  MPI_Bcast(&comp_value_block, 1, MPI_INT, 0, comm);
  MPI_Barrier(comm);

  // * Declare windows
  MPI_Win imgWindow, cImgWindow, gImgWindow;

  // * size calculations
  int disp_unit = sizeof(uint8_t); // will need to make this dyanmic if we decide to take non-8-bit colors
  int imgSize = width * height * disp_unit * channels, cImgSize = imgSize / comp_value_block, gImgSize = imgSize / (channels * comp_value_block);
  MPI_Aint aintImg, aintCImg, aintGImg;
  // TODOS: Need to handle case where images have an odd*odd, odd*(evenNotDivisibleBy4) where left over pixels will result from calculation and
  // TODOS: as such will screw with size calculations as well.

  // * create windows *


  MPI_Win_allocate_shared(imgSize, disp_unit, MPI_INFO_NULL, comm, &img, &imgWindow);
  MPI_Win_allocate_shared(cImgSize, disp_unit, MPI_INFO_NULL, comm, &cImg, &cImgWindow);
  MPI_Win_allocate_shared(gImgSize, disp_unit, MPI_INFO_NULL, comm, &gImg, &gImgWindow);

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

    memcpy(img, readImg, imgSize);
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

  int cImgWidth = width / comp_value;
  int cImgHeight = height / comp_value;

  uint32_t *ch_temp = calloc(channels, sizeof(uint32_t));

  int index = 0;

  
  
  MPI_Barrier(comm);
  start = MPI_Wtime();

  for (int i = work_height_start/comp_value; i < work_height_end/comp_value; i++)
  { // height where work is being done at in incremenets of comp_value
    for (int j = 0; j < width/comp_value; j++)
    { // width where work is being done at in increments of comp_value
      for (int k = 0; k < comp_value; k++)
      { // iterate over y axis of comp_value
        for (int a = 0; a < comp_value; a++)
        { // iterate over x axis of comp_value
          for (int ch = 0; ch < channels; ch++)
          { // iterate over each channel
            index = i * comp_value * width * channels + j*comp_value*channels + k*width*channels + a*channels + ch;
            ch_temp[ch] += img[index];
          }
        }
      }
      for (int ch = 0; ch < channels; ch++)
      {
        cImg[(i*(width/comp_value) + j)*channels + ch] = (uint8_t)(ch_temp[ch] / comp_value_block);
        ch_temp[ch] = 0;
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

  MPI_Barrier(comm);
  elapsed = MPI_Wtime();
  




  // write resulting images
  if (rank == 0)
  {
    printf("Time: %f\n", elapsed);
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
  }
  MPI_Win_free(&imgWindow);
  MPI_Win_free(&cImgWindow);
  MPI_Win_free(&gImgWindow);
  MPI_Finalize();

  return 0;
}