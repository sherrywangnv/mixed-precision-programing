//
// Created by binbin on 18-5-14.
//

#include <unistd.h>
#include <iostream>
#include <stdlib.h>
#include <assert.h>
#include <cuda_runtime.h>
#include <cublas_v2.h>
#include "fp16_conversion.h"

using namespace std;

const char* cublasGetErrorString(cublasStatus_t status)
{
    switch(status)
    {
        case CUBLAS_STATUS_SUCCESS: return "CUBLAS_STATUS_SUCCESS";
        case CUBLAS_STATUS_NOT_INITIALIZED: return "CUBLAS_STATUS_NOT_INITIALIZED";
        case CUBLAS_STATUS_ALLOC_FAILED: return "CUBLAS_STATUS_ALLOC_FAILED";
        case CUBLAS_STATUS_INVALID_VALUE: return "CUBLAS_STATUS_INVALID_VALUE"; 
        case CUBLAS_STATUS_ARCH_MISMATCH: return "CUBLAS_STATUS_ARCH_MISMATCH"; 
        case CUBLAS_STATUS_MAPPING_ERROR: return "CUBLAS_STATUS_MAPPING_ERROR";
        case CUBLAS_STATUS_EXECUTION_FAILED: return "CUBLAS_STATUS_EXECUTION_FAILED"; 
        case CUBLAS_STATUS_INTERNAL_ERROR: return "CUBLAS_STATUS_INTERNAL_ERROR"; 
    }
    return "unknown error";
}

// Convenience function for checking CUDA runtime API results
// can be wrapped around any runtime API call. No-op in release builds.
inline
cudaError_t checkCuda(cudaError_t result)
{
  if (result != cudaSuccess) {
    fprintf(stderr, "CUDA Runtime Error: %s\n", cudaGetErrorString(result));
    assert(result == cudaSuccess);
  }
  return result;
}

inline
cublasStatus_t checkCublas(cublasStatus_t result)
{
  if (result != CUBLAS_STATUS_SUCCESS) {
    fprintf(stderr, "CUDA Runtime Error: %s\n", cublasGetErrorString(result));
    assert(result == CUBLAS_STATUS_SUCCESS);
  }
  return result;
}

// Fill the array A(nr_rows_A, nr_cols_A) with random numbers on CPU
void CPU_fill_rand(float *A, int nr_rows_A, int nr_cols_A) {
	int a=1;

    for(int i = 0; i < nr_rows_A * nr_cols_A; i++){
		A[i] = (float)rand()/(float)(RAND_MAX/a);
	}
}

int main(int argc, char ** argv){


  int min_m_k_n = 2;
  int max_m_k_n = 4096*8;
  int repeats = 10;
  int verbose = 1;

  cout << "\nrunning cublasSgemm test\n" << endl;
  
  if(verbose) 
    cout << "running with" 
	 << " min_m_k_n: " << min_m_k_n
	 << " max_m_k_n: " << max_m_k_n
	 << " repeats: " << repeats
	 << endl;

  cublasStatus_t stat;
  cublasHandle_t handle;

  checkCublas(cublasCreate(&handle));

  if(verbose) cout << "allocating device variables" << endl;
  
  // Allocate 3 arrays on CPU
  
  float *h_A = (float *)malloc(max_m_k_n * max_m_k_n * sizeof(float));
  float *h_B = (float *)malloc(max_m_k_n * max_m_k_n * sizeof(float));
  float *h_C = (float *)malloc(max_m_k_n * max_m_k_n * sizeof(float));
  
  CPU_fill_rand(h_A, max_m_k_n, max_m_k_n);
  CPU_fill_rand(h_B, max_m_k_n, max_m_k_n);
  CPU_fill_rand(h_C, max_m_k_n, max_m_k_n);

    // Allocate 3 arrays on GPU
    float *d_A, *d_B, *d_C;
    checkCuda(cudaMallocManaged(&d_A, max_m_k_n * max_m_k_n * sizeof(float)));
    checkCuda(cudaMallocManaged(&d_B, max_m_k_n * max_m_k_n * sizeof(float)));
    checkCuda(cudaMallocManaged(&d_C, max_m_k_n * max_m_k_n * sizeof(float)));
    
    checkCuda(cudaMemcpy(d_A,h_A,max_m_k_n * max_m_k_n * sizeof(float),cudaMemcpyHostToDevice));
    checkCuda(cudaMemcpy(d_B,h_B,max_m_k_n * max_m_k_n * sizeof(float),cudaMemcpyHostToDevice));
    checkCuda(cudaMemcpy(d_C,h_C,max_m_k_n * max_m_k_n * sizeof(float),cudaMemcpyHostToDevice));
    
    int lda, ldb, ldc, m, n, k;
    const float alf = 1.0f;
    const float bet = 0.0f;
    const float *alpha = &alf;
    const float *beta = &bet;
  

  
  cudaEvent_t start, stop;
  cudaEventCreate(&start);
  cudaEventCreate(&stop);

  for(int size = min_m_k_n; size <= max_m_k_n; size=size*2){

    m=n=k=size;
    lda = m;
    ldb = k;
    ldc = m;
    
    cudaEventRecord(start, 0);


    for(int rep = 0; rep < repeats; rep++){

        stat = cublasSgemm(handle, CUBLAS_OP_N, CUBLAS_OP_N, m, n, k, alpha, d_A, lda, d_B, ldb, beta, d_C, ldc);

        cudaEventSynchronize(stop);
    }

    cudaEventRecord(stop,0);


    float elapsed;
    cudaEventElapsedTime(&elapsed, start, stop);
    elapsed /= 1000.0f;

    if(stat != CUBLAS_STATUS_SUCCESS){
      cerr << "cublasSgemmBatched failed" << endl;
      exit(1);
    }

    assert(!cudaGetLastError());

    cout << "float32; size "

         << size << " average: " << elapsed/repeats << " s "<< endl;

  }

  //Free GPU memory
  cudaFree(d_A);
  cudaFree(d_B);
  cudaFree(d_C);

  // Free CPU memory
  free(h_A);
  free(h_B);
  free(h_C);
      
  return 0;
}
