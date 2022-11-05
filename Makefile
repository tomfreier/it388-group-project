all: grayscale

grayscale: grayscale.c
	mpicc grayscale.c -o grayscale -g -Wall -lm -fopenmp

clean:
	rm grayscale output.jpg sky_gray.jpg