/* ----------------------------------------------------------------------
 * Project:      PULP DSP Library
 * Title:        plp_dwt_q8_xpulpv2.c
 * Description:  Floating-point Discret Wavelet Transform on real input data for XPULPV2
 *
 * $Date:        10. Juli 2021
 * $Revision:    V1
 *
 * Target Processor: PULP cores with "F" support (wolfe)
 * -------------------------------------------------------------------- */
/*
 * Copyright (C) 2021 ETH Zurich and University of Bologna. All rights reserved.
 *
 * Author: Jakub Mandula, ETH Zurich
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the License); you may
 * not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an AS IS BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "plp_math.h"
#include "plp_const_structs.h"

/* HELPER FUNCTIONS */


#define HAAR_COEF (0x5b)

#define MAC_SHIFT 7U;
// #define __MAC_8x8(Acc, A, B) Acc = __MACS(Acc, A, B); (Slower?)
#define __MAC_8x8(Acc, A, B) Acc += ((int16_t)A * (int16_t)B);
#define __MSU_8x8(Acc, A, B) Acc -= ((int16_t)A * (int16_t)B);

/********************************************************************************
 *  Left Edge Cases
 * *****************************************************************************/
#define CONSTANT_EDGE_LEFT(SUM_LO, SUM_HI, SRC, LENGTH, WAVELET, J, OFFSET)     \
    for(; J < WAVELET.length ; J++){                                            \
        __MAC_8x8(SUM_LO, WAVELET.dec_lo[J], SRC[0]);                      \
        __MAC_8x8(SUM_HI, WAVELET.dec_hi[J], SRC[0]);                      \
    }                                                                           \


#define SYMMETRIC_EDGE_LEFT(SUM_LO, SUM_HI, SRC, LENGTH, WAVELET, J, OFFSET)    \
    while(J < WAVELET.length){                                                  \
        int32_t k;                                                              \
        for(k=0; k < length && J < WAVELET.length; k++, J++) {                  \
            __MAC_8x8(SUM_LO, WAVELET.dec_lo[J], SRC[k]);                  \
            __MAC_8x8(SUM_HI, WAVELET.dec_hi[J], SRC[k]);                  \
        }                                                                       \
        for(k=0; k < LENGTH && J < WAVELET.length; k++, J++) {                  \
            __MAC_8x8(SUM_LO, WAVELET.dec_lo[J], SRC[LENGTH-1-k]);         \
            __MAC_8x8(SUM_HI, WAVELET.dec_hi[J], SRC[LENGTH-1-k]);         \
        }                                                                       \
    }                                                                           \


#define REFLECT_EDGE_LEFT(SUM_LO, SUM_HI, SRC, LENGTH, WAVELET, J, OFFSET)      \
    while(J < WAVELET.length){                                                  \
        int32_t k;                                                              \
        for(k=1; k < LENGTH && J < WAVELET.length; k++, J++) {                  \
            __MAC_8x8(SUM_LO, WAVELET.dec_lo[J], SRC[k]);                  \
            __MAC_8x8(SUM_HI, WAVELET.dec_hi[J], SRC[k]);                  \
        }                                                                       \
        for(k=1; k < LENGTH && J< WAVELET.length; k++, J++) {                   \
                                                                                \
            __MAC_8x8(SUM_LO, WAVELET.dec_lo[J], SRC[LENGTH-1-k]);         \
            __MAC_8x8(SUM_HI, WAVELET.dec_hi[J], SRC[LENGTH-1-k]);         \
        }                                                                       \
    }                                                                           \


#define ANTISYMMETRIC_EDGE_LEFT(SUM_LO, SUM_HI, SRC, LENGTH, WAVELET, J, OFFSET)\
    while(J < WAVELET.length){                                                  \
        int32_t k;                                                              \
        for(k=0; k < LENGTH && J < WAVELET.length; k++, J++) {                  \
            __MSU_8x8(SUM_LO, WAVELET.dec_lo[J], SRC[k]);                  \
            __MSU_8x8(SUM_HI, WAVELET.dec_hi[J], SRC[k]);                  \
        }                                                                       \
        for(k=0; k < LENGTH && J< WAVELET.length; k++, J++) {                   \
                                                                                \
            __MAC_8x8(SUM_LO, WAVELET.dec_lo[J], SRC[LENGTH-1-k]);         \
            __MAC_8x8(SUM_HI, WAVELET.dec_hi[J], SRC[LENGTH-1-k]);         \
        }                                                                       \
    }                                                                           \

#define ANTIREFLECT_EDGE_LEFT(SUM_LO, SUM_HI, SRC, LENGTH, WAVELET, J, OFFSET)  \
{                                                                               \
    int16_t left_edge = SRC[0];                                                 \
    int16_t tmp = 0;                                                            \
    while(J < WAVELET.length){                                                  \
        int32_t k;                                                              \
        for(k=1; k < LENGTH && J < WAVELET.length; k++, J++) {                  \
            tmp = left_edge - (SRC[k] - SRC[0]);                                \
            __MAC_8x8(SUM_LO, WAVELET.dec_lo[J], tmp);                     \
            __MAC_8x8(SUM_HI, WAVELET.dec_hi[J], tmp);                     \
        }                                                                       \
        left_edge = tmp;                                                        \
        for(k=1; k < LENGTH && J< WAVELET.length; k++, J++) {                   \
            tmp = left_edge + (SRC[LENGTH-1-k] - SRC[LENGTH-1]);                \
            __MAC_8x8(SUM_LO, WAVELET.dec_lo[J], tmp);                     \
            __MAC_8x8(SUM_HI, WAVELET.dec_hi[J], tmp);                     \
        }                                                                       \
        left_edge = tmp;                                                        \
    }                                                                           \
}                                                                               \


/********************************************************************************
 *  Right Edge Cases
 * *****************************************************************************/
#define CONSTANT_EDGE_RIGHT(SUM_LO, SUM_HI, SRC, LENGTH, WAVELET, J, OFFSET)    \
    for(; OFFSET - J >= LENGTH ; J++){                                          \
        __MAC_8x8(SUM_LO, WAVELET.dec_lo[J], SRC[LENGTH-1]);               \
        __MAC_8x8(SUM_HI, WAVELET.dec_hi[J], SRC[LENGTH-1]);               \
    }                                                                           \


#define SYMMETRIC_EDGE_RIGHT(SUM_LO, SUM_HI, SRC, LENGTH, WAVELET, J, OFFSET)   \
    while(OFFSET - J >= LENGTH){                                                \
        int32_t k;                                                              \
        for(k=0; k < LENGTH && OFFSET - J >= LENGTH; k++, J++) {                \
                                                                                \
            __MAC_8x8(SUM_LO, WAVELET.dec_lo[OFFSET - LENGTH - J], SRC[LENGTH - 1 - k]);\
            __MAC_8x8(SUM_HI, WAVELET.dec_hi[OFFSET - LENGTH - J], SRC[LENGTH - 1 - k]);\
        }                                                                       \
        for(k=0; k < LENGTH && OFFSET - J >= LENGTH; k++, J++) {                \
                                                                                \
            __MAC_8x8(SUM_LO, WAVELET.dec_lo[OFFSET - LENGTH - J], SRC[k]);\
            __MAC_8x8(SUM_HI, WAVELET.dec_hi[OFFSET - LENGTH - J], SRC[k]);\
        }                                                                       \
    }                                                                           \


#define REFLECT_EDGE_RIGHT(SUM_LO, SUM_HI, SRC, LENGTH, WAVELET, J, OFFSET)     \
    while(OFFSET - J >= LENGTH){                                                \
        int32_t k;                                                              \
        for(k=1; k < LENGTH && OFFSET - J >= LENGTH; k++, J++) {                \
                                                                                \
            __MAC_8x8(SUM_LO, WAVELET.dec_lo[OFFSET - LENGTH - J], SRC[LENGTH - 1 - k]);\
            __MAC_8x8(SUM_HI, WAVELET.dec_hi[OFFSET - LENGTH - J], SRC[LENGTH - 1 - k]);\
        }                                                                       \
        for(k=1; k < LENGTH && OFFSET - J >= LENGTH; k++, J++) {                \
                                                                                \
            __MAC_8x8(SUM_LO, WAVELET.dec_lo[OFFSET - LENGTH - J], SRC[k]);\
            __MAC_8x8(SUM_HI, WAVELET.dec_hi[OFFSET - LENGTH - J], SRC[k]);\
        }                                                                       \
    }                                                                           \


#define ANTISYMMETRIC_EDGE_RIGHT(SUM_LO, SUM_HI, SRC, LENGTH, WAVELET, J, OFFSET)\
    while(OFFSET - J >= LENGTH){                                                \
        int32_t k;                                                              \
        for(k=0; k < LENGTH && OFFSET - J >= LENGTH; k++, J++) {                \
            __MSU_8x8(SUM_LO, WAVELET.dec_lo[OFFSET - LENGTH - J], SRC[LENGTH - 1 - k]);\
            __MSU_8x8(SUM_HI, WAVELET.dec_hi[OFFSET - LENGTH - J], SRC[LENGTH - 1 - k]);\
        }                                                                       \
        for(k=0; k < LENGTH && OFFSET - J >= LENGTH; k++, J++) {                \
                                                                                \
            __MAC_8x8(SUM_LO, WAVELET.dec_lo[OFFSET - LENGTH - J], SRC[k]);\
            __MAC_8x8(SUM_HI, WAVELET.dec_hi[OFFSET - LENGTH - J], SRC[k]);\
        }                                                                       \
    }                                                                           \


#define ANTIREFLECT_EDGE_RIGHT(SUM_LO, SUM_HI, SRC, LENGTH, WAVELET, J, OFFSET) \
{                                                                               \
    int16_t right_edge = SRC[LENGTH -1];                                        \
    int16_t tmp = 0;                                                            \
    while(OFFSET - J >= LENGTH){                                                \
        int32_t k;                                                              \
        for(k=1; k < LENGTH && OFFSET - J >= LENGTH; k++, J++) {                \
            tmp = right_edge - (SRC[LENGTH-1-k] - SRC[LENGTH-1]);               \
            __MAC_8x8(SUM_LO, WAVELET.dec_lo[OFFSET - LENGTH - J], tmp);   \
            __MAC_8x8(SUM_HI, WAVELET.dec_hi[OFFSET - LENGTH - J], tmp);   \
        }                                                                       \
        right_edge = tmp;                                                       \
        for(k=1; k < LENGTH && OFFSET - J >= LENGTH; k++, J++) {                \
            tmp = right_edge + (SRC[k] - SRC[0]);                               \
            __MAC_8x8(SUM_LO, WAVELET.dec_lo[OFFSET - LENGTH - J], tmp);   \
            __MAC_8x8(SUM_HI, WAVELET.dec_hi[OFFSET - LENGTH - J], tmp);   \
        }                                                                       \
        right_edge = tmp;                                                       \
    }                                                                           \
}                                                                               \




#define shufflemask1                                                                               \
    (v4s) { 3, 2, 1, 0 }


/**
  @ingroup dwt
 */

/**
  @defgroup realDWTKernels DWT kernels on real input values
  These kernels calculate the DWT transform on real input data.
*/

/**
  @addtogroup realDWTKernels
  @{
 */

/**
   @brief  8bit fixed-point DWT on real input data for XPULPV2 extension.
   @param[in]   pSrc     points to the input buffer (real data)
   @param[in]   length   length of input buffer
   @param[in]   wavelet  wavelet structure for calculating DWT
   @param[in]   mode     boundary extension mode

   @param[out]  pDstA    points to ouput buffer with Approximate coefficients
   @param[out]  pDstD    points to ouput buffer with Detailed coefficients
   @return      none
*/
void plp_dwt_q8_xpulpv2(const int8_t *__restrict__ pSrc,
                         uint32_t length,
                         const plp_dwt_wavelet_q8 wavelet,
                         plp_dwt_extension_mode mode,
                         int8_t *__restrict__ pDstA,
                         int8_t *__restrict__ pDstD) {
    int8_t *pCurrentA = pDstA;
    int8_t *pCurrentD = pDstD;

    static uint32_t step = 2;

    int32_t offset;
        
    /***
     * The filter convolution is done in 4 steps handling cases where
     *  1. Filter is hanging over the left side of the signal
     *  2. Filter is same size, or totally enclosed in signal
     *  3. Filter is larger than the enclosed signal and hangs over both edges
     *  4. Filter hangs over the right side of the signal
     * 
     *  Each of the cases, where signal hangs over the boundary of the signal, values are computed 
     *  on demand based on the edge extension mode.
     */

    
    /*
     *  Handle Left overhanging
     *
     * X() =  x x[A B C D E F]
     * H() = [d c b a]
     *          ^   ^
     *          |   First compute the filter part overlapping with the signal
     *          Then extend the signal (x x) by computing the values based on the extension mode
     */
    for(offset = step-1; offset < wavelet.length - 1 && offset < length; offset += step){
        int16_t sum_lo = 0;
        int16_t sum_hi = 0;

        uint32_t filt_j = 0;

        // Compute Filter overlapping with signal
        for(; filt_j <= offset; filt_j++){
            __MAC_8x8(sum_lo, wavelet.dec_lo[filt_j], pSrc[offset - filt_j]);
            __MAC_8x8(sum_hi, wavelet.dec_hi[filt_j], pSrc[offset - filt_j]);
        }

        // Compute Left edge extension
        switch(mode){
            case PLP_DWT_MODE_CONSTANT:
                CONSTANT_EDGE_LEFT(sum_lo, sum_hi, pSrc, length, wavelet, filt_j, offset);
                break;
            case PLP_DWT_MODE_SYMMETRIC:
                SYMMETRIC_EDGE_LEFT(sum_lo, sum_hi, pSrc, length, wavelet, filt_j, offset);
                break;
            case PLP_DWT_MODE_REFLECT:
                REFLECT_EDGE_LEFT(sum_lo, sum_hi, pSrc, length, wavelet, filt_j, offset);
                break;
            case PLP_DWT_MODE_ANTISYMMETRIC:
                ANTISYMMETRIC_EDGE_LEFT(sum_lo, sum_hi, pSrc, length, wavelet, filt_j, offset);
                break;
            case PLP_DWT_MODE_ANTIREFLECT:
                ANTIREFLECT_EDGE_LEFT(sum_lo, sum_hi, pSrc, length, wavelet, filt_j, offset);
                break;
            case PLP_DWT_MODE_PERIODIC:
            case PLP_DWT_MODE_ZERO:
            default:
                break;
        }
    
    
        *pCurrentA++ = sum_lo >> MAC_SHIFT;
        *pCurrentD++ = sum_hi >> MAC_SHIFT;
    }

    /*
     *  Compute center (length >= wavelet.length)
     *
     *  X() = [A B C D E F]
     *  h() =   [d c b a]
     *                 ^
     *                 Compute a full convolution of the filter with the signal
     */    
    for(;offset < length; offset += step){

        int32_t sum_lo = 0;
        int32_t sum_hi = 0;

        // We can process 4 elements at a time
        uint32_t blkCnt = wavelet.length >> 2U;

        const int8_t *pYlo = wavelet.dec_lo; // Start of wavelet lo
        const int8_t *pYhi = wavelet.dec_hi; // Start of wavelet hi
        const int8_t *pX = pSrc + offset;

        while (blkCnt > 0U){
            v4s v_ylo = *((v4s *)pYlo);     // {lo[0], lo[1], lo[2], lo[3]}
            v4s v_yhi = *((v4s *)pYhi);     // {hi[0], hi[1], hi[2], hi[3]}
            v4s v_x   = *((v4s *)(pX - 3)); // { x[0],  x[1],  x[2],  x[3]}

            v4s v_sx = __builtin_shuffle(v_x, v_x, shufflemask1); // {x[3], x[2], x[1], [0]}

            sum_lo = __SUMDOTP4(v_sx, v_ylo, sum_lo);
            sum_hi = __SUMDOTP4(v_sx, v_yhi, sum_hi);

            pYlo += 4;
            pYhi += 4;
            pX   -= 4;

            blkCnt--;
        }

        // Wavelet length is not a multiple of 4. However it will always be multiple of 2.
        // Thus just handel this case extra
        if(wavelet.length % 4 > 0){
            v4s v_ylo = *((v4s *)pYlo);     // {lo[0], lo[1],   XX ,   XX }
            v4s v_yhi = *((v4s *)pYhi);     // {hi[0], hi[1],   XX ,   XX }
            v4s v_x   = *((v4s *)(pX - 3)); // {  XX ,   XX ,  x[0],  x[1]}

            v_x = __AND4(v_x, ((v4s){ 0, 0, 0xff, 0xff }));         // {  0 ,   0 , x[0], x[1]}
            v4s v_sx = __builtin_shuffle(v_x, v_x, shufflemask1); // {x[1], x[0],   0 ,   0 }

            sum_lo = __SUMDOTP4(v_sx, v_ylo, sum_lo);
            sum_hi = __SUMDOTP4(v_sx, v_yhi, sum_hi);

        }  


        *pCurrentA++ = sum_lo >> 7U;
        *pCurrentD++ = sum_hi >> 7U;
    }

     /*
     *  Compute center (length < wavelet.length)
     *
     *  X() =   y y[A B C]x x x
     *  h() =  [h g f e d c b a]
     *            ^     ^     ^
     *            |     |     Compute Right extension (x x x) based on extension mode
     *            |     Compute a full convolution of the filter overlapping with the signal
     *            Compute Left extension (y y) based on extension mode
     */      

    for(;offset < wavelet.length - 1; offset += step){
        int16_t sum_lo = 0;
        int16_t sum_hi = 0;

        uint32_t filt_j = 0;

        // Filter Right extension
        switch(mode){
            case PLP_DWT_MODE_CONSTANT:
                CONSTANT_EDGE_RIGHT(sum_lo, sum_hi, pSrc, length, wavelet, filt_j, offset);
                break;
            case PLP_DWT_MODE_SYMMETRIC:
                SYMMETRIC_EDGE_RIGHT(sum_lo, sum_hi, pSrc, length, wavelet, filt_j, offset);
                break;
            case PLP_DWT_MODE_REFLECT:
                REFLECT_EDGE_RIGHT(sum_lo, sum_hi, pSrc, length, wavelet, filt_j, offset);
                break;
            case PLP_DWT_MODE_ANTISYMMETRIC:
                ANTISYMMETRIC_EDGE_RIGHT(sum_lo, sum_hi, pSrc, length, wavelet, filt_j, offset);
                break;
            case PLP_DWT_MODE_ANTIREFLECT:
                ANTIREFLECT_EDGE_RIGHT(sum_lo, sum_hi, pSrc, length, wavelet, filt_j, offset);
                break;
            case PLP_DWT_MODE_PERIODIC:
            case PLP_DWT_MODE_ZERO:
            default:
                filt_j = offset - length + 1;
                break;
        }

        // Filter Center overlapp
        for(; filt_j <= offset; filt_j++){
            __MAC_8x8(sum_lo, wavelet.dec_lo[filt_j], pSrc[offset - filt_j]);
            __MAC_8x8(sum_hi, wavelet.dec_hi[filt_j], pSrc[offset - filt_j]);
        }   

        // Filter Left extension
        switch(mode){
            case PLP_DWT_MODE_CONSTANT:
                CONSTANT_EDGE_LEFT(sum_lo, sum_hi, pSrc, length, wavelet, filt_j, offset);
                break;
            case PLP_DWT_MODE_SYMMETRIC:
                SYMMETRIC_EDGE_LEFT(sum_lo, sum_hi, pSrc, length, wavelet, filt_j, offset);
                break;
            case PLP_DWT_MODE_REFLECT:
                REFLECT_EDGE_LEFT(sum_lo, sum_hi, pSrc, length, wavelet, filt_j, offset);
                break;
            case PLP_DWT_MODE_ANTISYMMETRIC:
                ANTISYMMETRIC_EDGE_LEFT(sum_lo, sum_hi, pSrc, length, wavelet, filt_j, offset);
                break;
            case PLP_DWT_MODE_ANTIREFLECT:
                ANTIREFLECT_EDGE_LEFT(sum_lo, sum_hi, pSrc, length, wavelet, filt_j, offset);
                break;
            case PLP_DWT_MODE_PERIODIC:
            case PLP_DWT_MODE_ZERO:
            default:
                break;
        }

        *pCurrentA++ = sum_lo >> MAC_SHIFT;
        *pCurrentD++ = sum_hi >> MAC_SHIFT;
    }


    /*
     *  Handle Right overhanging
     *
     * X() = [A B C D E F]x x
     * H() =         [d c b a]
     *                  ^   ^
     *                  |   First extend the signal (x x) by computing the values based on the extension mode
     *                  Then compute the filter part overlapping with the signal
     */
    for(; offset < length + wavelet.length - 1; offset += step){
        int16_t sum_lo = 0;
        int16_t sum_hi = 0;

        uint32_t filt_j = 0;

        // Compute Left edge extension
        switch(mode){
            case PLP_DWT_MODE_CONSTANT:
                CONSTANT_EDGE_RIGHT(sum_lo, sum_hi, pSrc, length, wavelet, filt_j, offset);
                break;
            case PLP_DWT_MODE_SYMMETRIC:
                SYMMETRIC_EDGE_RIGHT(sum_lo, sum_hi, pSrc, length, wavelet, filt_j, offset);
                break;
            case PLP_DWT_MODE_REFLECT:
                REFLECT_EDGE_RIGHT(sum_lo, sum_hi, pSrc, length, wavelet, filt_j, offset);
                break;
            case PLP_DWT_MODE_ANTISYMMETRIC:
                ANTISYMMETRIC_EDGE_RIGHT(sum_lo, sum_hi, pSrc, length, wavelet, filt_j, offset);
                break;
            case PLP_DWT_MODE_ANTIREFLECT:
                ANTIREFLECT_EDGE_RIGHT(sum_lo, sum_hi, pSrc, length, wavelet, filt_j, offset);
                break;
            case PLP_DWT_MODE_PERIODIC:
            case PLP_DWT_MODE_ZERO:
            default:
                filt_j = offset - length + 1;
                break;
        }
    
        // Filter overlapping with signal
        for(; filt_j < wavelet.length; filt_j++){
            __MAC_8x8(sum_lo, wavelet.dec_lo[filt_j], pSrc[offset - filt_j]);
            __MAC_8x8(sum_hi, wavelet.dec_hi[filt_j], pSrc[offset - filt_j]);
        }

        *pCurrentA++ = sum_lo >> MAC_SHIFT;
        *pCurrentD++ = sum_hi >> MAC_SHIFT;
    }
}




/**
   @brief 8bit fixed-point DWT kernel optimized for Haar Wavelet on real input data for XPULPV2 extension.
   @param[in]   pSrc     points to the input buffer (real data)
   @param[in]   length   length of input buffer
   @param[in]   mode     boundary extension mode

   @param[out]  pDstA    points to ouput buffer with Approximate coefficients
   @param[out]  pDstD    points to ouput buffer with Detailed coefficients
   @return      none
*/
void plp_dwt_haar_q8_xpulpv2(const int8_t *__restrict__ pSrc,
                         uint32_t length,
                         plp_dwt_extension_mode mode,
                         int8_t *__restrict__ pDstA,
                         int8_t *__restrict__ pDstD) {
    int8_t *pCurrentA = pDstA;
    int8_t *pCurrentD = pDstD;

    static uint32_t step = 2;

    int32_t offset = step-1 ;

    static v4s v_ylo_l = (v4s){HAAR_COEF, HAAR_COEF, 0, 0};
    static v4s v_ylo_r = (v4s){0, 0, HAAR_COEF, HAAR_COEF};
    static v4s v_yhi_l = (v4s){HAAR_COEF, -HAAR_COEF, 0, 0};
    static v4s v_yhi_r = (v4s){0, 0, HAAR_COEF, -HAAR_COEF};
    v4s v_x;
    
    /***
     * The filter convolution is done in 4 steps handling cases where
     *  1. Filter is hanging over the left side of the signal
     *  2. Filter is same size, or totally enclosed in signal
     *  3. Filter is larger than the enclosed signal and hangs over both edges
     *  4. Filter hangs over the right side of the signal
     * 
     *  Each of the cases, where signal hangs over the boundary of the signal, values are computed 
     *  on demand based on the edge extension mode.
     */

    
 
    /*
     *  Compute center (length >= wavelet.length)
     *
     *  X() = [A B C D E F]
     *  h() =       [b a]
     *                 ^
     *                 Compute a full convolution of the filter with the signal
     */ 

    uint32_t blkCnt = length >> 2U;
    const int8_t *pS = pSrc;

    while(blkCnt > 0){

        v_x   = *((v4s *)(pS));     // { x[0],  x[1],  x[2],  x[3]}

        *pCurrentA++ = __DOTP4(v_x, v_ylo_l) >> MAC_SHIFT;
        *pCurrentD++ = __DOTP4(v_x, v_yhi_l) >> MAC_SHIFT;

        *pCurrentA++ = __DOTP4(v_x, v_ylo_r) >> MAC_SHIFT;
        *pCurrentD++ = __DOTP4(v_x, v_yhi_r) >> MAC_SHIFT;
        pS   += 4;
        blkCnt--;
    }

    // Assume block fits perfectly into the signal length and we are now to beyond the signal (Will skip right overhang)
    offset = length + 1; 


    switch(length % 4U){
    case 0:
        // We are actually done, the signal fit perfectly
        break;
    case 3:
        offset = length; // We still need to compute the offset but also the last full overlap pair
    case 2:
        // We compute the full overlap pair here

        v_x   = *((v4s *)(pS));     // { x[0],  x[1],  x[2],  x[3]}

        *pCurrentA++ = __DOTP4(v_x, v_ylo_l) >> MAC_SHIFT;
        *pCurrentD++ = __DOTP4(v_x, v_yhi_l) >> MAC_SHIFT;

        // *pCurrentA++ = __MULSN(HAAR_COEF, (pSrc[offset - 1] + pSrc[offset]), MAC_SHIFT);
        // *pCurrentD++ = __MULSN(HAAR_COEF, (pSrc[offset - 1] - pSrc[offset]), MAC_SHIFT);
        break;

    case 1:
        offset = length; // We still need to compute the offset but no full overlap
        break;

    }
   


    /*
     *  Handle Right overhanging
     *
     * X() = [A B C D E F]x x
     * H() =         [d c b a]
     *                  ^   ^
     *                  |   First extend the signal (x x) by computing the values based on the extension mode
     *                  Then compute the filter part overlapping with the signal
     */
    if(offset < length + 1){
        int16_t sum_lo = 0;
        int16_t sum_hi = 0;

        uint32_t filt_j = 0;

        // Compute Left edge extension
        switch(mode){
            case PLP_DWT_MODE_CONSTANT:
            case PLP_DWT_MODE_SYMMETRIC:
                sum_lo = 2 * HAAR_COEF * pSrc[length - 1];   // dec_lo[0] * src[N-1] + dec_lo[1] * src[N-1]
                sum_hi = 0;                                  // dec_hi[0] * src[N-1] + dec_hi[1] * src[N-1] == -dec_hi[1] * src[N-1] + dec_hi[1] * src[N-1]
                break;
            case PLP_DWT_MODE_REFLECT:
                sum_lo = HAAR_COEF * (pSrc[length - 1] + pSrc[length - 2]);
                sum_hi = HAAR_COEF * (pSrc[length - 1] - pSrc[length - 2]);
                break;
            case PLP_DWT_MODE_ANTISYMMETRIC:
                sum_lo = HAAR_COEF * (pSrc[length - 1] - pSrc[length - 1]);
                sum_hi = HAAR_COEF * (pSrc[length - 1] + pSrc[length - 1]);
                break;
            case PLP_DWT_MODE_ANTIREFLECT:
                sum_lo = HAAR_COEF * (3*pSrc[length - 1] - pSrc[length - 2]);
                sum_hi = HAAR_COEF * ( -pSrc[length - 1] + pSrc[length - 2]);
                break;
            case PLP_DWT_MODE_PERIODIC:
            case PLP_DWT_MODE_ZERO:
            default:
                sum_lo = HAAR_COEF * pSrc[length - 1];
                sum_hi = HAAR_COEF * pSrc[length - 1];
                break;
        }
    
        *pCurrentA = sum_lo >> 7U;
        *pCurrentD = sum_hi >> 7U;
    }
}