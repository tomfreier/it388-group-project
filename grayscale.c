#include <stdio.h>
#include <stdlib.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb/stb_image_write.h"

#include "mpi.h"
#include <omp.h>


int main(int argc, char *argv[]) {
    int width, height, channels;
    unsigned char *img = stbi_load("sky.jpg", &width, &height, &channels, 0);
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

    printf("%u, %zu", *(img+1), img_size);
    double start = omp_get_wtime();

    unsigned char *p, *pg=gray_img;

    #pragma omp parallel for private(pg) num_threads(1)
    for(p=img; p < img + img_size; p += channels){
        *pg = (uint8_t)((*p + *(p + 1) + *(p + 2))/3.0);
        if(channels == 4) {
            *(pg + 1) = *(p + 3);
        }
        pg += gray_channels;
    }
    // p = [1,2,3,4]
    // *p = 1
    // *(p+1) 2
    // p+1 = [2,3,4]
    // #pragma omp parallel for num_threads(4)
    // for(int i=0;i<(img + img_size);i++){
    //     printf("i hate cs\n");
    //     *pg = (uint8_t)((*p+i + *(p+i + 1) + *(p+i + 2))/3.0);
    //     printf("%u\n",*pg);
    //     if(channels == 4) {
    //         *(pg+ i+ 1) = *(p +i+ 3);
    //     }
    //     pg += gray_channels;
    // }
    double finish = omp_get_wtime();
    double elapsed = finish - start;

    printf("Time: %f\n", elapsed);


    stbi_write_jpg("sky_gray.jpg", width, height, gray_channels, gray_img, 100); //1-100 image quality



    return 0;
}
