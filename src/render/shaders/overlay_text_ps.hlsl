// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

Texture2D    g_atlas   : register(t0);
SamplerState g_sampler : register(s0);

struct PS_INPUT {
    float4 pos   : SV_POSITION;
    float2 uv    : TEXCOORD0;
    float4 color : COLOR;
};

float4 main(PS_INPUT input) : SV_TARGET {
    float4 s = g_atlas.Sample(g_sampler, input.uv);
    float coverage = min(s.r, s.a);
    return float4(input.color.rgb, input.color.a * coverage);
}
