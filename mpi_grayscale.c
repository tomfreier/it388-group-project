#include <stdio.h>
#include <stdlib.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb/stb_image_write.h"

#include <mpi.h>
#define BUFFER 100
#define MANAGER_CORE 0
int main(int argc, char *argv[])
{
    int nproc, rank, work, localFirstRow,localLastRow;
    int width, height, channels;
    char originalFileName[100];
    char compressedFileName[100];
    char greyscaleFileName[100];
    unsigned char *img;
    unsigned char *comp_img;
    
    MPI_Init(&argc, &argv); /* Intialize MPI*/
    /* handling command line args*/
    if (argc != 4)
    {
        fprintf(stderr, "Usage mpiexec -n <# of processes> ./mpi_grayscale <image_file> <compressed_image_dest> <greyscale_image_dest>\n");
        exit(EXIT_FAILURE);
    }
    else
    {
        strcpy(originalFileName, argv[1]);
        strcpy(compressedFileName, argv[2]);
        strcpy(greyscaleFileName, argv[3]);
    }
    printf("%d\n",nproc);
    MPI_Comm comm = MPI_COMM_WORLD;
    MPI_Comm_size(comm, &nproc);
    MPI_Comm_rank(comm, &rank);
    img = stbi_load(originalFileName, &width, &height, &channels, 0);
    if (img == NULL)
    {
            printf("Error in loading the image\n");
            MPI_Abort(comm,1);
    }
    printf("Loaded image with a width of %dpx, a height of %dpx and %d channels\n", width, height, channels);
    work = height/nproc; // # of rows each core reasonable for
    localFirstRow = rank*work;
    localLastRow = localFirstRow+work;
    
    // IMAGE COMPRESSION
    
    size_t img_size = width * height * channels;
    int comp_width = width / 2, comp_height = height / 2;
    size_t comp_img_size = comp_width * comp_height * channels;
    unsigned char * local_comp_img = (unsigned char *)malloc((comp_img_size/nproc) + BUFFER);
    unsigned char *p = img;
    if(rank == MANAGER_CORE){
        comp_img = (unsigned char *)malloc(comp_img_size);
    }
    MPI_Barrier(comm);
    int start = MPI_Wtime();

    printf("work: %d rank: %d\n",work,rank);
    for(int i=0; i<work;i+=2){
        for(int j=0; j<width; j+=2){
            int index = channels * (((localFirstRow + i) / 2) * comp_width + j / 2);
            int redAvg = (p[channels * (localFirstRow+i * width + j)] + p[channels * (localFirstRow+i * width + j + 1)] + p[channels * ((localFirstRow+i + 1) * width + j)] + p[channels * ((localFirstRow+i + 1) * width + j + 1)]) / 4;
            int greenAvg = (p[channels * (localFirstRow+i * width + j) + 1] + p[channels * (localFirstRow+i * width + j + 1) + 1] + p[channels * ((localFirstRow+i + 1) * width + j) + 1] + p[channels * ((localFirstRow+i + 1) * width + j + 1) + 1]) / 4;
            int blueAvg = (p[channels * (localFirstRow+i * width + j) + 2] + p[channels * (localFirstRow+i * width + j + 1) + 2] + p[channels * ((localFirstRow+i + 1) * width + j) + 2] + p[channels * ((localFirstRow+i + 1) * width + j + 1) + 2]) / 4;
            printf("Rank: %d index: %d\tred_avg:%d, green_avg:%d, blue_avg:%d\n",rank,index,redAvg,greenAvg,blueAvg);
            local_comp_img[index] = redAvg;
            local_comp_img[index+1] = greenAvg;
            local_comp_img[index+2] = blueAvg;
        }
    }
    MPI_Gather(local_comp_img , comp_img_size/nproc , MPI_UNSIGNED_CHAR, comp_img, comp_img_size/nproc, MPI_UNSIGNED_CHAR , MANAGER_CORE ,comm);
    MPI_Barrier(comm);
    int finish = MPI_Wtime();
    int elapsed = finish - start;
    
    if(rank == MANAGER_CORE){
        printf("\nCompression Time: %d seconds\n", elapsed);
        if(stbi_write_jpg(compressedFileName, comp_width, comp_height, channels, comp_img, 100) < 0){
            perror("stbi");
        }
        printf("Image compression complete\n\n");
        free(comp_img);
    }
    
    MPI_Finalize();
    
    return 0;
}