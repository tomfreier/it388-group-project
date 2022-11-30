#include <stdio.h>
#include <stdlib.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb/stb_image_write.h"

#include <omp.h>


int main(int argc, char *argv[]) {
    int nThreads;
    char* originalFileName = (char *) malloc(100*sizeof(char));
    char* compressedFileName = (char *) malloc(100*sizeof(char));
    char* greyscaleFileName = (char *) malloc(100*sizeof(char));

    /* handling command line args*/
    if(argc!=5){
        fprintf(stderr,"Usage ./grayscale <threads> <image_file> <compressed_image_dest> <greyscale_image_dest>\n");
        exit(EXIT_FAILURE);
    }
    else{
        nThreads= atoi(argv[1]);
        strcpy(originalFileName,argv[2]);
        strcpy(compressedFileName,argv[3]);
        strcpy(greyscaleFileName,argv[4]);
    }
    omp_set_num_threads(nThreads);
    


    int width, height, channels;
    unsigned char *img = stbi_load(originalFileName, &width, &height, &channels, 0);
    // stbi_write_jpg("test.png", width, height, channels, img, 100);

    if(img == NULL) {
        printf("Error in loading the image\n");
        exit(1);
    }
    
    printf("\n\nLoaded image with a width of %dpx, a height of %dpx and %d channels\n", width, height, channels);
    printf("Number threads: %d\n", nThreads);
    //IMAGE COMPRESSION
    size_t img_size = width * height * channels;
    int comp_width = width/2, comp_height = height/2;
    size_t comp_img_size = comp_width * comp_height * channels;
    unsigned char *comp_img = malloc(comp_img_size);
   
    unsigned char *cpg=comp_img;
    unsigned char *p=img;
    
    double start = omp_get_wtime();

    #pragma omp parallel for
    for(int i=0; i<height-1; i+=2){
        for(int j=0; j<width-1; j+=2){
            int redAvg = (p[channels*(i*width + j)] + p[channels*(i*width + j+1)] + p[channels*((i+1)*width + j)] + p[channels*((i+1)*width + j+1)])/4;
            int greenAvg = (p[channels*(i*width + j) + 1] + p[channels*(i*width + j+1) + 1] + p[channels*((i+1)*width + j) + 1] + p[channels*((i+1)*width + j+1) + 1])/4;
            int blueAvg = (p[channels*(i*width + j) + 2] + p[channels*(i*width + j+1) + 2] + p[channels*((i+1)*width + j) + 2] + p[channels*((i+1)*width + j+1) + 2])/4;
            cpg[channels*((i/2)*comp_width + j/2)] = redAvg;
            cpg[channels*((i/2)*comp_width + j/2) + 1] = greenAvg;
            cpg[channels*((i/2)*comp_width + j/2) + 2] = blueAvg;
            // printf("index: %d\tred_avg:%d, green_avg:%d, blue_avg:%d\n",(channels*((i/2)*comp_width + j/2)),redAvg,greenAvg,blueAvg);
        }
    }

    double finish = omp_get_wtime();
    double elapsed = finish - start;
    printf("Compression Time: %f seconds\n", elapsed);
    
    
    stbi_write_jpg(compressedFileName, comp_width, comp_height, channels, comp_img, 100);
    printf("Image compression complete\n\n");

    //GRAY SCALE
    int gray_channels = 1;
    size_t gray_img_size = comp_width * comp_height * gray_channels;
    unsigned char *gray_img = malloc(gray_img_size);
    unsigned char *pg=gray_img;
    

    

    #pragma omp parallel for
    for(int i=0; i<comp_height; i++){
        for(int j=0; j<comp_width; j++){
            pg[i*comp_width + j] = (uint8_t)((cpg[channels*(i*comp_width + j)] + cpg[channels*(i*comp_width + j) + 1] + cpg[channels*(i*comp_width + j) + 2])/3.0);

        }
    }




    #pragma omp parallel 
    for(int i=0; i<comp_height; i++){
        #pragma omp for nowait
        for(int j=0; j<comp_width; j++){
            pg[i*comp_width + j] = (uint8_t)((cpg[channels*(i*comp_width + j)] + cpg[channels*(i*comp_width + j) + 1] + cpg[channels*(i*comp_width + j) + 2])/3.0);
        }
    }

    finish = omp_get_wtime();
    elapsed = finish - start;
    printf("Threads: %d  Total Time: %f seconds\n", nThreads, elapsed);


    stbi_write_jpg(greyscaleFileName, comp_width, comp_height, gray_channels, gray_img, 100); //1-100 image quality
    printf("Image grayscale complete\n");
    
    /* cleaning up memory*/
    free(originalFileName);
    free(compressedFileName);
    free(greyscaleFileName);
    return 0;
}
