/*******************************************************************************
* Copyright 2017-2022 Intel Corporation
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

#include "bnorm/bnorm.hpp"

namespace bnorm {

void compute_ref_fwd(const prb_t *prb, const args_t &args) {
    const dnn_mem_t &src = args.find(DNNL_ARG_SRC);
    const dnn_mem_t &mean = args.find(DNNL_ARG_MEAN);
    const dnn_mem_t &var = args.find(DNNL_ARG_VARIANCE);
    const dnn_mem_t &ss
            = args.find(prb->use_sc() ? DNNL_ARG_SCALE : DNNL_ARG_SCALE_SHIFT);
    const dnn_mem_t &sh = args.find(DNNL_ARG_SHIFT);
    const dnn_mem_t &ws = args.find(DNNL_ARG_WORKSPACE);
    const dnn_mem_t &dst = args.find(DNNL_ARG_DST);
    const dnn_mem_t &src_hat = args.find(DNNL_ARG_DST_1);

    uint8_t *ws_ptr = (uint8_t *)ws;
    float *dst_ptr = (float *)dst;
    float *src_hat_ptr = (float *)src_hat;

    const int64_t MB = prb->mb;
    const int64_t C = prb->ic;
    const int64_t D = prb->id;
    const int64_t H = prb->ih;
    const int64_t W = prb->iw;
    const bool use_ss = prb->use_ss();
    const bool use_sc = prb->use_sc();
    const bool use_sh = prb->use_sh();
    const bool fuse_relu = prb->flags & FUSE_NORM_RELU;
    const bool need_ws = prb->need_ws();
    const auto &attr = prb->attr;

    benchdnn_parallel_nd(C, [&](int64_t c) {
        float smean = mean.get_elem(c);
        float svar = var.get_elem(c);
        float sqrt_var = sqrtf(svar + prb->eps);
        float rcp_denom = 1.f / sqrt_var;
        float gamma = (use_ss || use_sc) ? ss.get_elem(c) : 1.f;
        float beta = use_ss ? ss.get_elem(C + c) : use_sh ? sh.get_elem(c) : 0;

        for_(int64_t mb = 0; mb < MB; ++mb)
        for_(int64_t d = 0; d < D; ++d)
        for_(int64_t h = 0; h < H; ++h)
        for (int64_t w = 0; w < W; ++w) {
            auto off = data_off(prb, mb, c, d, h, w);
            float x_hat = (src.get_elem(off) - smean) * rcp_denom;
            float res = gamma * x_hat + beta;
            if (fuse_relu && res < 0) res = 0;
            if (need_ws) ws_ptr[off] = !!res;
            maybe_post_ops(attr, res);
            dst_ptr[off] = res;
            if (prb->dir & FLAG_BWD) src_hat_ptr[off] = x_hat;
        }
    });
}

void compute_ref_bwd(const prb_t *prb, const args_t &args) {
    const dnn_mem_t &src_hat = args.find(DNNL_ARG_DST_1);
    const dnn_mem_t &var = args.find(DNNL_ARG_VARIANCE);
    const dnn_mem_t &d_dst = args.find(DNNL_ARG_DIFF_DST);
    const dnn_mem_t &ss
            = args.find(prb->use_sc() ? DNNL_ARG_SCALE : DNNL_ARG_SCALE_SHIFT);
    const dnn_mem_t &ws = args.find(DNNL_ARG_WORKSPACE);
    const dnn_mem_t &d_src = args.find(DNNL_ARG_DIFF_SRC);
    const dnn_mem_t &d_ss = args.find(
            prb->use_sc() ? DNNL_ARG_DIFF_SCALE : DNNL_ARG_DIFF_SCALE_SHIFT);
    const dnn_mem_t &d_sh = args.find(DNNL_ARG_DIFF_SHIFT);

    float *d_src_ptr = (float *)d_src;
    float *d_ss_ptr = (float *)d_ss;
    float *d_sh_ptr = (float *)d_sh;

    const int64_t MB = prb->mb;
    const int64_t C = prb->ic;
    const int64_t D = prb->id;
    const int64_t H = prb->ih;
    const int64_t W = prb->iw;
    const bool glob_stats = prb->flags & GLOB_STATS;
    const bool use_ss = prb->use_ss();
    const bool use_sc = prb->use_sc();
    const bool use_sh = prb->use_sh();
    const bool fuse_relu = prb->flags & FUSE_NORM_RELU;

    const float MB_SP = MB * D * H * W;

    benchdnn_parallel_nd(C, [&](int64_t c) {
        float rcp_denom = 1.f / sqrtf(var.get_elem(c) + prb->eps);
        float gamma = (use_ss || use_sc) ? ss.get_elem(c) : 1.f;

        float d_gamma = 0;
        float d_beta = 0;

        for_(int64_t mb = 0; mb < MB; ++mb)
        for_(int64_t d = 0; d < D; ++d)
        for_(int64_t h = 0; h < H; ++h)
        for (int64_t w = 0; w < W; ++w) {
            auto off = data_off(prb, mb, c, d, h, w);
            float dd = d_dst.get_elem(off);
            if (fuse_relu && ws.get_elem(off) == 0) dd = 0;
            d_gamma += dd * src_hat.get_elem(off);
            d_beta += dd;
        }

        if (use_ss && (prb->dir & FLAG_WEI)) {
            d_ss_ptr[c] = d_gamma;
            d_ss_ptr[C + c] = d_beta;
        }

        if (use_sc && (prb->dir & FLAG_WEI)) d_ss_ptr[c] = d_gamma;
        if (use_sh && (prb->dir & FLAG_WEI)) d_sh_ptr[c] = d_beta;

        for_(int64_t mb = 0; mb < MB; ++mb)
        for_(int64_t d = 0; d < D; ++d)
        for_(int64_t h = 0; h < H; ++h)
        for (int64_t w = 0; w < W; ++w) {
            auto off = data_off(prb, mb, c, d, h, w);
            float dd = d_dst.get_elem(off);
            if (fuse_relu && ws.get_elem(off) == 0) dd = 0;
            float ds = dd;

            if (!glob_stats)
                ds -= (d_beta + src_hat.get_elem(off) * d_gamma) / MB_SP;

            d_src_ptr[off] = rcp_denom * ds * gamma;
        }
    });
}

void compute_ref(
        const prb_t *prb, const args_t &args, dnnl_primitive_t prim_ref) {
    compute_ref_fwd(prb, args);
    if (prb->dir & FLAG_BWD) compute_ref_bwd(prb, args);
}

} // namespace bnorm
