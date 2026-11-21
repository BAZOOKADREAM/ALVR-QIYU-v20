#include "srgb_correction_pass.h"
#include "utils.h"
#include <cmath>
#include <cstdint>
#include <memory>
#include <sys/types.h>

using namespace std;
using namespace gl_render_utils;

namespace {
struct CorrectionParams {
    float encodingGamma;
    float padding[3];
};

const string ENCODING_GAMMA_FRAGMENT_SHADER = R"glsl(#version 300 es
        #extension GL_OES_EGL_image_external_essl3 : enable
        precision mediump float;

        uniform samplerExternalOES tex0;
        in vec2 uv;
        out vec4 color;
        layout(std140) uniform CorrectionParams {
            vec4 correction;
        };

        void main()
        {
            color = texture(tex0, uv);
            color.rgb = pow(max(color.rgb, vec3(0.0)), vec3(correction.x));
        }
    )glsl";
} // namespace

SrgbCorrectionPass::SrgbCorrectionPass(Texture *inputSurface) : mInputSurface(inputSurface) {}

void SrgbCorrectionPass::Initialize(uint32_t width, uint32_t height, float encodingGamma) {
    mOutputTexture.reset(new Texture(false, 0, false, width * 2, height));
    mOutputTextureState = make_unique<RenderState>(mOutputTexture.get());

    mEncodingGamma = encodingGamma;
    mStagingPipeline = unique_ptr<RenderPipeline>(new RenderPipeline(
        {mInputSurface}, QUAD_2D_VERTEX_SHADER, ENCODING_GAMMA_FRAGMENT_SHADER,
        sizeof(CorrectionParams)));
}

void SrgbCorrectionPass::Render() const {
    mOutputTextureState->ClearDepth();
    const CorrectionParams params = {mEncodingGamma, {0.0f, 0.0f, 0.0f}};
    mStagingPipeline->Render(*mOutputTextureState, &params);
}
