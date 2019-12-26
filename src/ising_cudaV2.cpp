#include "ising.h"
#include <stdio.h>
#include <math.h>
#include <cuda_runtime.h>
#include <cuda.h>
#include <cuda_runtime_api.h>

#define BLK_DIM_SZ 32

#define WGHT_SZ 25
#define WGHT_DIM_SZ 5
#define MAX_OFFSET 2

#define epsilon 1e-6f

__device__ __forceinline__ 
int mod(int x, int y)
{
    return (y + x%y)%y;
}

__device__  __forceinline__ 
int calcLatticePos(int i, int j, int n, int xOffset, int yOffset)
{
    /* Finds the index in the lattice, according to
     *  i, j   : Current Position (in which we calculate spin)
     *  xOffset: Offset in the x-axis
     *  yOffset: Offset in the y-axis
     */

    // Perform Modular Arithmetic
    return mod(i + yOffset, n)*n + mod(j + xOffset, n);  
}

void swapPtr(int** ptr1, int** ptr2)
{
    int *tempPtr = *ptr1;
    *ptr1 = *ptr2;
    *ptr2 = tempPtr;
}

__global__ void calculateSpin(int *current, int *next, float *w, int n)
{
    size_t index = blockIdx.y*BLK_DIM_SZ*n + threadIdx.y*n + blockIdx.x*BLK_DIM_SZ + threadIdx.x ;

    if(index < (size_t)n*n)
    {
        float result = 0.0f;
        int i,j;
        for(i=-MAX_OFFSET; i<=MAX_OFFSET; i++)
            for(j=-MAX_OFFSET; j<=MAX_OFFSET; j++)
                result += w[ (i+MAX_OFFSET)*WGHT_DIM_SZ + (j+MAX_OFFSET) ] * current[ calcLatticePos(index/n, index%n, n, j, i) ];
        
        if(fabsf(result) < epsilon )
            next[index] = current[index];
        else if(result < 0)
            next[index] = -1;
        else
            next[index] = 1;

    }
}

/* For this version of CUDA Ising:
 * Each CUDA block of threads calculates
 * one BLK_DIM_SZ x BLK_DIM_SZ block in the spin lattice.
 */
void ising(int *G, float *w, int k, int n)
{
    int *dev_G, *dev_G2;
    float *dev_w;
    size_t latticeSize = (size_t)n*n*sizeof(int);

    // Data Transfer and Memory Alloc on Device
    cudaMalloc(&dev_G, latticeSize);
    cudaMalloc(&dev_G2, latticeSize);
    cudaMalloc(&dev_w, WGHT_SZ*sizeof(float));

    cudaMemcpy(dev_G, G, latticeSize, cudaMemcpyHostToDevice);
    cudaMemcpy(dev_w, w, WGHT_SZ*sizeof(float), cudaMemcpyHostToDevice);

    // Kernel Launch - Calculations
    int gridDimSz = (n + BLK_DIM_SZ - 1)/BLK_DIM_SZ;
    dim3 grid2D(gridDimSz, gridDimSz);
    dim3 block2D(BLK_DIM_SZ, BLK_DIM_SZ);
    for(int iter=0; iter<k; iter++)
    {
        calculateSpin<<<grid2D, block2D>>>(dev_G, dev_G2, dev_w, n);
        cudaDeviceSynchronize();
        swapPtr(&dev_G, &dev_G2);
    }

    // Send Result back to CPU
    cudaMemcpy(G, dev_G, latticeSize, cudaMemcpyDeviceToHost);

    // Clear Resources
    cudaFree(dev_G);
    cudaFree(dev_G2);
}