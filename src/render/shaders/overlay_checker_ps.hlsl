// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

struct PS_INPUT {
    float4 pos                  : SV_POSITION;
    nointerpolation float4 grid : TEXCOORD0;
    float4 color                : COLOR;
};

float2 cell_edge(float2 k, float4 grid) {
    precise float2 authored = grid.xy + k * grid.z;
    precise float2 device = authored * grid.w;
    return device;
}

float4 main(PS_INPUT input) : SV_TARGET {
    const float2 center = input.pos.xy;
    float2 k = floor((center / input.grid.w - input.grid.xy) / input.grid.z);
    k += (cell_edge(k + 1.0f, input.grid) <= center) ? 1.0f : 0.0f;
    k -= (cell_edge(k, input.grid) > center) ? 1.0f : 0.0f;
    const float odd = fmod(abs(k.x + k.y), 2.0f);
    return float4(input.color.rgb, input.color.a * (1.0f - odd));
}
