#include <stdio.h>
#include <stdlib.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb/stb_image_write.h"

#include "mpi.h"
#include <omp.h>


int main(int argc, char *argv[]) {
    int nThreads;

    if (argc < 2){
        printf("Using default nThread=1;");
        nThreads= 1;
        
    }else{
        nThreads= atoi(argv[1]);
    }

    omp_set_num_threads(nThreads);


    int width, height, channels;
    unsigned char *img = stbi_load("dog.jpg", &width, &height, &channels, 0);
    
    if(img == NULL) {
        printf("Error in loading the image\n");
        exit(1);
    }
    
    printf("Loaded image with a width of %dpx, a height of %dpx and %d channels\n", width, height, channels);



    size_t img_size = width * height * channels;
    int gray_channels = (channels == 4 ? 2 : 1);

    size_t gray_img_size = width * height * gray_channels;
    unsigned char *gray_img = malloc(gray_img_size);
    
    if(gray_img == NULL) {
        printf("Unable to allocate memory for the gray image.\n");
        exit(1);
    }

    printf("%u, %d\n", *(img+1), img_size);
    printf("width: %d\t height: %d\t channels: %d\n", width, height, channels);
    double start = omp_get_wtime();

    unsigned char *p=img;
    unsigned char *pg=gray_img;
    int count=0;
    
    #pragma omp parallel 
    for(int i=0; i!=height; i++){
        #pragma omp for nowait 
        for(int j=0; j!=width; j++){
            pg[i*width + j] = (uint8_t)((p[channels*(i*width + j)] + p[channels*(i*width + j) + 1] + p[channels*(i*width + j) + 2])/3.0);

            if(channels == 4) {
                *(pg + 1) = *(p + 3);
            }
        }
    }

    
    double finish = omp_get_wtime();
    double elapsed = finish - start;

    printf("Time: %f\n", elapsed);


    stbi_write_jpg("dog_gray.jpg", width, height, gray_channels, gray_img, 100); //1-100 image quality

    return 0;
}
