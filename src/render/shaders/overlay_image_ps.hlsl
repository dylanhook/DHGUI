// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

Texture2D g_image : register(t0);
SamplerState g_sampler : register(s0);

struct PS_INPUT {
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD0;
    float4 color : COLOR;
};

float4 main(PS_INPUT input) : SV_TARGET {
    return g_image.Sample(g_sampler, input.uv) * input.color;
}
