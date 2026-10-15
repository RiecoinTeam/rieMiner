// (c) 2018-2019, NVIDIA CORPORATION. Based on MIT Licensed https://github.com/NVlabs/CGBN/tree/master/samples/sample_03_powm
// (c) 2026-present The Riecoin Developers (https://riecoin.xyz/rieMiner)

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <vector>
#include <cuda.h>
#include <gmp.h>
#include <gmpxx.h>
#include "cgbn.h"
#include "../utility/support.h"

// For this example, there are quite a few template parameters that are used to generate the actual code.
// In order to simplify passing many parameters, we use the same approach as the CGBN library, which is to
// create a container class with static constants and then pass the class.

// The CGBN context uses the following three parameters:
//	 TBP						 - threads per block (zero means to use the blockDim.x)
//	 MAX_ROTATION		- must be small power of 2, imperically, 4 works well
//	 SHM_LIMIT			 - number of bytes of dynamic shared memory available to the kernel
//	 CONSTANT_TIME	 - require constant time algorithms (currently, constant time algorithms are not available)

// Locally it will also be helpful to have several parameters:
//	 TPI						 - threads per instance
//	 BITS						- number of bits per instance
//	 WINDOW_BITS		 - number of bits to use for the windowed exponentiation

template<uint32_t tpi, uint32_t bits, uint32_t window_bits> class powm_params_t {
public:
	// parameters used by the CGBN context
	static const uint32_t TPB=0;										 // get TPB from blockDim.x	
	static const uint32_t MAX_ROTATION=4;						// good default value
	static const uint32_t SHM_LIMIT=0;							 // no shared mem available
	static const bool CONSTANT_TIME=false;			 // constant time implementations aren't available yet
	
	// parameters used locally in the application
	static const uint32_t TPI=tpi;									 // threads per instance
	static const uint32_t BITS=bits;								 // instance size
	static const uint32_t WINDOW_BITS=window_bits;	 // window size
};

template<class params> class powm_odd_t {
public:
	static const uint32_t window_bits = params::WINDOW_BITS;	// used a lot, give it an instance variable
	
	// define the instance structure
	typedef struct {
		cgbn_mem_t<params::BITS> modulus;
		cgbn_mem_t<params::BITS> result;
	} instance_t;

	typedef cgbn_context_t<params::TPI, params>	 context_t;
	typedef cgbn_env_t<context_t, params::BITS>	 env_t;
	typedef typename env_t::cgbn_t bn_t;
	typedef typename env_t::cgbn_local_t bn_local_t;

	context_t _context;
	env_t		 _env;
	int32_t	 _instance;
	
	__device__ __forceinline__ powm_odd_t(cgbn_monitor_t monitor, cgbn_error_report_t *report, int32_t instance) : _context(monitor, report, (uint32_t)instance), _env(_context), _instance(instance) {}

	__device__ __forceinline__ void fixed_window_powm_odd(bn_t &result, const bn_t &modulus) {
		bn_t t;
		bn_local_t window[1 << window_bits];
		int32_t		index, position, offset;
		uint32_t	 np0;
		
		bn_t x, power;
		cgbn_set_ui32(_env, x, 2);
		cgbn_sub_ui32(_env, power, modulus, 1);

		// conmpute x^power mod modulus, using the fixed window algorithm
		// requires:	x<modulus,	modulus is odd

		// compute x^0 (in Montgomery space, this is just 2^BITS - modulus)
		cgbn_negate(_env, t, modulus);
		cgbn_store(_env, window+0, t);
		
		// convert x into Montgomery space, store into window table
		np0=cgbn_bn2mont(_env, result, x, modulus);
		cgbn_store(_env, window+1, result);
		cgbn_set(_env, t, result);
		
		// compute x^2, x^3, ... x^(2^window_bits-1), store into window table
		#pragma nounroll
		for(index=2;index<(1<<window_bits);index++) {
			cgbn_mont_mul(_env, result, result, t, modulus, np0);
			cgbn_store(_env, window+index, result);
		}

		// find leading high bit
		position=params::BITS - cgbn_clz(_env, power);

		// break the exponent into chunks, each window_bits in length
		// load the most significant non-zero exponent chunk
		offset=position % window_bits;
		if (offset==0)
			position=position-window_bits;
		else
			position=position-offset;
		index=cgbn_extract_bits_ui32(_env, power, position, window_bits);
		cgbn_load(_env, result, window+index);
		
		// process the remaining exponent chunks
		while(position>0) {
			// square the result window_bits times
			#pragma nounroll
			for(int sqr_count=0;sqr_count<window_bits;sqr_count++)
				cgbn_mont_sqr(_env, result, result, modulus, np0);
			
			// multiply by next exponent chunk
			position=position-window_bits;
			index=cgbn_extract_bits_ui32(_env, power, position, window_bits);
			cgbn_load(_env, t, window+index);
			cgbn_mont_mul(_env, result, result, t, modulus, np0);
		}
		
		// we've processed the exponent now, convert back to normal space
		cgbn_mont2bn(_env, result, result, modulus, np0);
	}
	
	__device__ __forceinline__ void sliding_window_powm_odd(bn_t &result, const bn_t &x, const bn_t &power, const bn_t &modulus) {
		bn_t t, starts;
		int32_t index, position, leading;
		uint32_t mont_inv;
		bn_local_t odd_powers[1<<window_bits-1];

		// conmpute x^power mod modulus, using Constant Length Non-Zero windows (CLNZ).
		// requires:	x<modulus,	modulus is odd
				
		// find the leading one in the power
		leading = params::BITS - 1 -cgbn_clz(_env, power);
		if (leading>=0) {
			// convert x into Montgomery space, store in the odd powers table
			mont_inv=cgbn_bn2mont(_env, result, x, modulus);
			
			// compute t=x^2 mod modulus
			cgbn_mont_sqr(_env, t, result, modulus, mont_inv);
			
			// compute odd powers window table: x^1, x^3, x^5, ...
			cgbn_store(_env, odd_powers, result);
			#pragma nounroll
			for(index=1;index<(1<<window_bits-1);index++) {
				cgbn_mont_mul(_env, result, result, t, modulus, mont_inv);
				cgbn_store(_env, odd_powers+index, result);
			}
	
			// starts contains an array of bits indicating the start of a window
			cgbn_set_ui32(_env, starts, 0);
	
			// organize p as a sequence of odd window indexes
			position=0;
			while(true) {
				if(cgbn_extract_bits_ui32(_env, power, position, 1)==0)
					position++;
				else {
					cgbn_insert_bits_ui32(_env, starts, starts, position, 1, 1);
					if(position+window_bits>leading)
						break;
					position=position+window_bits;
				}
			}
	
			// load first window.	Note, since the window index must be odd, we have to
			// divide it by two before indexing the window table.	Instead, we just don't
			// load the index LSB from power
			index=cgbn_extract_bits_ui32(_env, power, position+1, window_bits-1);
			cgbn_load(_env, result, odd_powers+index);
			position--;
			
			// Process remaining windows 
			while (position>=0) {
				cgbn_mont_sqr(_env, result, result, modulus, mont_inv);
				if (cgbn_extract_bits_ui32(_env, starts, position, 1)==1) {
					// found a window, load the index
					index=cgbn_extract_bits_ui32(_env, power, position+1, window_bits-1);
					cgbn_load(_env, t, odd_powers+index);
					cgbn_mont_mul(_env, result, result, t, modulus, mont_inv);
				}
				position--;
			}
			
			// convert result from Montgomery space
			cgbn_mont2bn(_env, result, result, modulus, mont_inv);
		}
		else {
			// p=0, thus x^p mod modulus=1
			cgbn_set_ui32(_env, result, 1);
		}
	}
};

// kernel implementation using cgbn
// Unfortunately, the kernel must be separate from the powm_odd_t class
template<class params> __global__ void kernel_powm_odd(cgbn_error_report_t *report, typename powm_odd_t<params>::instance_t *instances, uint32_t count) {
	int32_t instance;
	
	// decode an instance number from the blockIdx and threadIdx
	instance=(blockIdx.x*blockDim.x + threadIdx.x)/params::TPI;
	if (instance>=count)
		return;
	
	powm_odd_t<params> po(cgbn_report_monitor, report, instance);
	typename powm_odd_t<params>::bn_t r, p, m;
	
	// the loads and stores can go in the class, but it seems more natural to have them
	// here and to pass in and out bignums
	//cgbn_load(po._env, p, &(instances[instance].power));
	cgbn_load(po._env, m, &(instances[instance].modulus));
	
	// this can be either fixed_window_powm_odd or sliding_window_powm_odd.	
	// when TPI<32, fixed window runs much faster because it is less divergent, so we use it here
	po.fixed_window_powm_odd(r, m);
	//	 OR
	// po.sliding_window_powm_odd(r, x, p, m);
	
	cgbn_store(po._env, &(instances[instance].result), r);
}

template<int bits> void fermatCudaTest(const std::vector<mpz_class> &candidates, uint32_t* is_prime, const uint16_t device) {
	auto count(candidates.size());
	typedef powm_params_t<8, bits, 5> params;
	typedef typename powm_odd_t<params>::instance_t instance_t;

	instance_t *instances = (instance_t *) malloc(sizeof(instance_t)*count), *gpuInstances;
	cgbn_error_report_t *report;
	int32_t TPB=(params::TPB==0) ? 128 : params::TPB; // default threads per block to 128
	int32_t TPI=params::TPI, IPB=TPB/TPI; // IPB is instances per block
	
	for (int i(0); i < count; i++) {
		mpz_class candidate(candidates[i]);
		from_mpz(candidate.get_mpz_t(), instances[i].modulus._limbs, params::BITS/32);
	}
	
	CUDA_CHECK(cudaSetDevice(device));
	CUDA_CHECK(cudaMalloc((void **)&gpuInstances, sizeof(instance_t)*count));
	CUDA_CHECK(cudaMemcpy(gpuInstances, instances, sizeof(instance_t)*count, cudaMemcpyHostToDevice));
	
	// create a cgbn_error_report for CGBN to report back errors
	CUDA_CHECK(cgbn_error_report_alloc(&report));
	
	// launch kernel with blocks=ceil(instance_count/IPB) and threads=TPB
	kernel_powm_odd<params><<<(count+IPB-1)/IPB, TPB>>>(report, gpuInstances, count);

	// error report uses managed memory, so we sync the device (or stream) and check for cgbn errors
	CUDA_CHECK(cudaDeviceSynchronize());
	CGBN_CHECK(report);
		
	// copy the instances back from gpuMemory
	CUDA_CHECK(cudaMemcpy(instances, gpuInstances, sizeof(instance_t)*count, cudaMemcpyDeviceToHost));
	
	mpz_class r;
	for (int i(0); i < count; i++) {
		to_mpz(r.get_mpz_t(), instances[i].result._limbs, params::BITS/32);
		is_prime[i] = r == 1;
	}
	
	// clean up
	free(instances);
	CUDA_CHECK(cudaFree(gpuInstances));
	CUDA_CHECK(cgbn_error_report_free(report));
}

void fermatCudaTest768(const std::vector<mpz_class> &candidates, uint32_t* is_prime, const uint16_t device) {
	fermatCudaTest<768>(candidates, is_prime, device);
}

void fermatCudaTest1024(const std::vector<mpz_class> &candidates, uint32_t* is_prime, const uint16_t device) {
	fermatCudaTest<1024>(candidates, is_prime, device);
}

void fermatCudaTest1280(const std::vector<mpz_class> &candidates, uint32_t* is_prime, const uint16_t device) {
	fermatCudaTest<1280>(candidates, is_prime, device);
}

void fermatCudaTest1536(const std::vector<mpz_class> &candidates, uint32_t* is_prime, const uint16_t device) {
	fermatCudaTest<1536>(candidates, is_prime, device);
}

void fermatCudaTest1792(const std::vector<mpz_class> &candidates, uint32_t* is_prime, const uint16_t device) {
	fermatCudaTest<1792>(candidates, is_prime, device);
}

void fermatCudaTest2048(const std::vector<mpz_class> &candidates, uint32_t* is_prime, const uint16_t device) {
	fermatCudaTest<2048>(candidates, is_prime, device);
}
