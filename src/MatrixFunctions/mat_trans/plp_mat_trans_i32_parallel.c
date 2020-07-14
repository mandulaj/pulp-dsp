/** =====================================================================
 * Project:      PULP DSP Library
 * Title:        plp_mat_trans_i32_parallel.c
 * Description:  parallel 16-bit integer matrix transpose glue code
 *
 * $Date:        14. July 2020
 * $Revision:    V0
 *
 * Target Processor: PULP cores
 * ===================================================================== */
/**
 * Copyright (C) 2020 ETH Zurich and Ubiversity of Bologna. All rights reserved.
 *
 * Author: Tibor Schneider, ETH Zurich
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


/**
  @ingroup groupMatrix
 */


/**
  @addtogroup MatTrans
  @{
 */

/**
  @brief Glue code for parallel matrix transpose of 16-bit integer matrices.
  <ARGS_DOC_PARALLEL
  @return     none
 */

void plp_mat_trans_i32_parallel(const int32_t* __restrict__ pSrc,
                                uint32_t M,
                                uint32_t N,
                                uint32_t nPE,
                                int32_t* __restrict__ pDst){

    if (rt_cluster_id() == ARCHI_FC_CID){
        printf("parallel processing supported only for cluster side\n");
        return;
    }
    else{
        plp_mat_trans_instance_i32 args = {
            .pSrc = pSrc,
            .M = M,
            .N = N,
            .nPE = nPE,
            .pDst = pDst
        };

        rt_team_fork(nPE, plp_mat_trans_i32p_xpulpv2, (void*) &args);
    }

}

/**
  @} end of MatTrans group
 */


