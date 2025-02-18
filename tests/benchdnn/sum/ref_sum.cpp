/*******************************************************************************
* Copyright 2019-2022 Intel Corporation
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*     http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*******************************************************************************/

#include "utils/parallel.hpp"

#include "sum/sum.hpp"

namespace sum {

void compute_ref(
        const prb_t *prb, const args_t &args, dnnl_primitive_t prim_ref) {
    const dnn_mem_t &dst = args.find(DNNL_ARG_DST);

    float *dst_ptr = (float *)dst;

    const auto nelems = dst.nelems();

    benchdnn_parallel_nd(nelems, [&](int64_t k) {
        dst_ptr[k] = 0;
        for (int i_input = 0; i_input < prb->n_inputs(); ++i_input) {
            const dnn_mem_t &src_i = args.find(DNNL_ARG_MULTIPLE_SRC + i_input);
            dst_ptr[k] += (src_i.get_elem(k) * prb->scales[i_input]);
        }
    });
}

} // namespace sum
