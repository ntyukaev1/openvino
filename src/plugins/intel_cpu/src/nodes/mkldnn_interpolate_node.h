// Copyright (C) 2018-2021 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#pragma once

#include <ie_common.h>
#include <mkldnn_node.h>
#include <string>
#include <memory>
#include <vector>

#define MAX_INPUT_INTERPOLATE 8

using namespace InferenceEngine;

namespace MKLDNNPlugin {
enum InterpolateLayoutType {
    planar,
    block,
    by_channel
};

enum InterpolateMode {
    nearest,
    linear,
    linear_onnx,
    cubic
};

enum InterpolateCoordTransMode {
    half_pixel,
    pytorch_half_pixel,
    asymmetric,
    tf_half_pixel_for_nn,
    align_corners
};

enum class InterpolateNearestMode {
    round_prefer_floor,
    round_prefer_ceil,
    floor,
    ceil,
    simple
};

enum class InterpolateShapeCalcMode {
    sizes,
    scales
};

struct jit_interpolate_config_params {
    InterpolateLayoutType layout;
    InterpolateMode mode;
    mkldnn::memory::data_type src_dt;
    mkldnn::memory::data_type dst_dt;
    int src_data_size;
    int dst_data_size;
    int indices_size;
    int spatial_dim_size;
    int ID, IH, IW, OD, OH, OW;
};

struct jit_interpolate_call_args {
    const void *src_ptr[MAX_INPUT_INTERPOLATE];
    const void *weight_ptr[MAX_INPUT_INTERPOLATE];
    const int *index;
    void *dst;
    size_t work_amount;
    size_t oc_off;
    //ptr to array of post op inputs pointers (flat list)
    const void* post_op_data;
};

struct jit_uni_interpolate_kernel {
    void (*ker_)(const jit_interpolate_call_args *);

    void operator()(const jit_interpolate_call_args *args) {
        assert(ker_);
        ker_(args);
    }

    explicit jit_uni_interpolate_kernel(jit_interpolate_config_params jcp, const mkldnn_primitive_attr &attr) : ker_(nullptr), jcp_(jcp), attr_(attr) {}
    virtual ~jit_uni_interpolate_kernel() {}

    virtual void create_ker() = 0;

    jit_interpolate_config_params jcp_;
    const mkldnn_primitive_attr &attr_;
};


class MKLDNNInterpolateNode : public MKLDNNNode {
public:
    MKLDNNInterpolateNode(const std::shared_ptr<ngraph::Node>& op, const mkldnn::engine& eng, MKLDNNWeightsSharing::Ptr &cache);

    void getSupportedDescriptors() override;
    void initSupportedPrimitiveDescriptors() override;
    void createPrimitive() override;
    bool created() const override;
    void execute(mkldnn::stream strm) override;
    void executeDynamicImpl(mkldnn::stream strm) override;
    bool canBeInPlace() const override {
        return false;
    }
    bool canFuse(const MKLDNNNodePtr& node) const override;

    static bool isSupportedOperation(const std::shared_ptr<const ngraph::Node>& op, std::string& errorMessage) noexcept;

    bool needShapeInfer() const override;
    std::vector<VectorDims> shapeInfer() const override;
    bool needPrepareParams() const override;
    void prepareParams() override;

    struct InterpolateAttrs {
        InterpolateMode mode;
        InterpolateCoordTransMode coordTransMode;
        InterpolateNearestMode nearestMode;
        bool antialias;
        float cubeCoeff;
        std::vector<int> padBegin;
        std::vector<int> padEnd;
        InferenceEngine::Precision inPrc;
        InferenceEngine::Precision outPrc;
        InterpolateLayoutType layout;
    };

private:
    InterpolateAttrs interpAttrs;

    class InterpolateExecutor {
        public:
            InterpolateExecutor(const InterpolateAttrs& interpAttrs,
                                const VectorDims &srcDims,
                                const VectorDims &dstDims,
                                const std::vector<float> &dataScales);

            virtual void exec(const uint8_t *in_ptr_, uint8_t *out_ptr_, const void *post_ops_data_) = 0;
            virtual ~InterpolateExecutor() = default;
            VectorDims getSrcDimPad5d() const { return srcDimPad5d; }

        private:
            void buildTblNN(const SizeVector& srcDimPad5d, const SizeVector& dstDim5d, const std::vector<float>& dataScales,
                            InterpolateLayoutType layout, InterpolateNearestMode nearestMode);
            void buildTblLinearOnnx(const SizeVector& srcDimPad5d, const SizeVector& dstDim5d, const std::vector<float>& dataScales,
                                    InterpolateLayoutType layout);
            void buildTblLinear(const SizeVector& srcDimPad5d, const SizeVector& dstDim5d, const std::vector<float>& dataScales, int kernel_width,
                                bool antialias);
            void buildTblCubic(const SizeVector& srcDimPad5d, const SizeVector& dstDim5d, const std::vector<float>& dataScales, float cubicCoeff,
                               InterpolateLayoutType layout);

            float coordTransToInput(int outCoord, float scale, int inShape, int outShape) const;
            int nearestRound(float origin, bool isDownsample, InterpolateNearestMode nearestMode) const;
            void linearOnnxCF(int outCoord, float scale, int inShape, int outShape, int& index0, int& index1, float& weight0, float& weight1);
            std::vector<float> getCubicCoeffs(float mantissa, float a);

        protected:
            InterpolateMode mode;
            InterpolateCoordTransMode coordTransMode;
            InterpolateLayoutType configured_for_layout;
            VectorDims srcDimPad5d, dstDim5d;
            InferenceEngine::Precision inputPrec, outputPrec;
            size_t srcDataSize, dstDataSize;
            int spatialDimSize;
            size_t dataRank;
            std::vector<int> indexTable;
    };
    std::shared_ptr<InterpolateExecutor> execPtr = nullptr;

    class InterpolateJitExecutor : public InterpolateExecutor {
        public:
            InterpolateJitExecutor(const InterpolateAttrs& interpAttrs,
                                   const VectorDims &srcDims,
                                   const VectorDims &dstDims,
                                   const std::vector<float> &dataScales,
                                   const mkldnn::primitive_attr &attr);

            void exec(const uint8_t *in_ptr_, uint8_t *out_ptr_, const void *post_ops_data_) override;

        private:
            // nearest neighbor
            void NNPlanar(const uint8_t *in_ptr_, uint8_t *out_ptr_, const void *post_ops_data_,
                int B, int C, int ID, int IH, int IW, int OD, int OH, int OW);
            void NNCGathered(const uint8_t *in_ptr_, uint8_t *out_ptr_, const void *post_ops_data_,
                int B, int C, int ID, int IH, int IW, int OD, int OH, int OW);

            // onnx linear
            void linearOnnxPlanar(const uint8_t *in_ptr_, uint8_t *out_ptr_, const void *post_ops_data_,
                int B, int C, int ID, int IH, int IW, int OD, int OH, int OW);
            void linearOnnxCGathered(const uint8_t *in_ptr_, uint8_t *out_ptr_, const void *post_ops_data_,
                int B, int C, int ID, int IH, int IW, int OD, int OH, int OW);

            // cubic
            void cubicPlanar(const uint8_t *in_ptr_, uint8_t *out_ptr_, const void *post_ops_data_,
                int B, int C, int IH, int IW, int OH, int OW);
            void cubicCGathered(const uint8_t *in_ptr_, uint8_t *out_ptr_, const void *post_ops_data_,
                int B, int C, int IH, int IW, int OH, int OW);

        private:
            std::shared_ptr<jit_uni_interpolate_kernel> interpolateKernel = nullptr;
    };

    class InterpolateRefExecutor : public InterpolateExecutor {
        public:
            InterpolateRefExecutor(const InterpolateAttrs& interpAttrs,
                                   const VectorDims &srcDims,
                                   const VectorDims &dstDims,
                                   const std::vector<float> &_dataScales) : dataScales(_dataScales), antialias(interpAttrs.antialias),
                InterpolateExecutor(interpAttrs, srcDims, dstDims, _dataScales) {}

            void exec(const uint8_t *in_ptr_, uint8_t *out_ptr_, const void *post_ops_data_) override;

        private:
            void NNRef(const uint8_t *in_ptr_, uint8_t *out_ptr_, int B, int C, int ID, int IH, int IW, int OD, int OH, int OW);
            void linearOnnxRef(const uint8_t *in_ptr_, uint8_t *out_ptr_, int B, int C, int ID, int IH, int IW, int OD, int OH, int OW);

            void cubicRef(const uint8_t *in_ptr_, uint8_t *out_ptr_, int B, int C, int IH, int IW, int OH, int OW);
            void linearInterpolation(const uint8_t *in_ptr_, uint8_t *out_ptr_, int B, int C, int ID, int IH, int IW,
                                      float fx, float fy, float fz, int OD, int OH, int OW, int kernel_width, bool antialias);

            static float getValue(const uint8_t *base, size_t offset, InferenceEngine::Precision prec);
            static void setValue(uint8_t *base, size_t offset, float value, InferenceEngine::Precision prec);

        private:
            bool antialias;
            std::vector<float> dataScales;
    };

    void setPostOps(mkldnn::primitive_attr &attr, const VectorDims &dims, bool initWeights = false);

    static SizeVector getPaddedInputShape(const VectorDims &srcDims, const std::vector<int> &padBegin, const std::vector<int> &padEnd);
    std::vector<float> getScales(const VectorDims &srcDimPad, const VectorDims &dstDim);
    static size_t getSpatialDimsNum(const Dim rank);

    static constexpr size_t DATA_ID = 0;
    static constexpr size_t TARGET_SHAPE_ID = 1;
    static constexpr size_t SCALES_ID = 2;
    static constexpr size_t AXES_ID = 3;
    static constexpr int CUBIC_GRID_LEN = 4;

    bool hasPad = false;
    InterpolateShapeCalcMode shapeCalcMode;

    bool isAxesSpecified = false;
    std::vector<int> axes;

    // 6 ptrs for each quantization, 2 ptrs for each depth_wise
    std::vector<const void*> postOpsDataPtrs;

    std::vector<float> lastScales;
    std::vector<int32_t> lastSizes;

    VectorDims lastOutputDims;

    std::string errorPrefix;
};

}  // namespace MKLDNNPlugin
