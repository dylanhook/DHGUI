// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

struct VS_INPUT {
    float4 pos   : POSITION;
    float2 uv    : TEXCOORD0;
    float4 color : COLOR;
};

struct VS_OUTPUT {
    float4 pos   : SV_POSITION;
    float2 uv    : TEXCOORD0;
    float4 color : COLOR;
};

VS_OUTPUT main(VS_INPUT input) {
    VS_OUTPUT output;
    output.pos   = input.pos;
    output.uv    = input.uv;
    output.color = input.color;
    return output;
}
