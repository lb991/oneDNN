/*******************************************************************************
* Copyright 2021-2022 Arm Ltd. and affiliates
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

#ifndef CPU_AARCH64_ACL_MATMUL_UTILS_HPP
#define CPU_AARCH64_ACL_MATMUL_UTILS_HPP

#include "cpu/matmul/cpu_matmul_pd.hpp"

#include "cpu/aarch64/acl_utils.hpp"

namespace dnnl {
namespace impl {
namespace cpu {
namespace aarch64 {

struct acl_matmul_obj_t {
    arm_compute::NEGEMM gemm;
    arm_compute::NETranspose transA;
    arm_compute::NETranspose transB;
    arm_compute::Tensor src_tensor;
    arm_compute::Tensor src_acc_tensor;
    arm_compute::Tensor wei_tensor;
    arm_compute::Tensor wei_acc_tensor;
    arm_compute::Tensor dst_tensor;
};

struct acl_matmul_conf_t {
    bool with_bias;
    bool is_transA;
    bool is_transB;
    arm_compute::TensorInfo src_info;
    arm_compute::TensorInfo src_acc_info;
    arm_compute::TensorInfo wei_info;
    arm_compute::TensorInfo wei_acc_info;
    arm_compute::TensorInfo dst_info;
    arm_compute::GEMMInfo gemm_info;
    float alpha;
};

namespace acl_matmul_utils {

status_t init_conf_matmul(acl_matmul_conf_t &amp, memory_desc_t &src_md,
        memory_desc_t &wei_md, memory_desc_t &dst_md, memory_desc_t &bias_md,
        const matmul_desc_t &md, const primitive_attr_t &attr);

arm_compute::ActivationLayerInfo get_acl_act(const primitive_attr_t &attr);
bool acl_act_ok(alg_kind_t eltwise_activation);
} // namespace acl_matmul_utils

} // namespace aarch64
} // namespace cpu
} // namespace impl
} // namespace dnnl

#endif // CPU_AARCH64_ACL_MATMUL_UTILS_HPP
