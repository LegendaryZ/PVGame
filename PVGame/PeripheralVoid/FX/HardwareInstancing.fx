//=============================================================================
// Basic.fx by Frank Luna (C) 2011 All Rights Reserved.
//
// Basic effect that currently supports transformations, lighting, and texturing.
//=============================================================================

#include "LightHelper.fx"
 
#define MAX_LIGHTS 20

cbuffer cbPerFrame
{
	DirectionalLight gDirLights[3];
	PointLight testLights[MAX_LIGHTS];
	PointLight gPointLight;
	float3 gEyePosW;

	float  gFogStart;
	float  gFogRange;
	float4 gFogColor;
};

cbuffer cbPerObject
{
	float4x4 gViewProj;
	float4x4 gTexTransform;
}; 

// Nonnumeric values cannot be added to a cbuffer.
Texture2D gDiffuseMap;

SamplerState samAnisotropic
{
	Filter = ANISOTROPIC;
	MaxAnisotropy = 4;

	AddressU = WRAP;
	AddressV = WRAP;
};

struct VertexIn
{
	float3 PosL    : POSITION;
	float3 NormalL : NORMAL;
	float2 Tex     : TEXCOORD;
	row_major float4x4 World  : WORLD;
	Material Material : MATERIAL;
	uint InstanceId : SV_InstanceID;
	float2 AtlasCoord : ATLASCOORD;
};

struct VertexOut
{
	float4 PosH    : SV_POSITION;
    float3 PosW    : POSITION;
    float3 NormalW : NORMAL;
	float2 Tex     : TEXCOORD;
	Material Material : MATERIAL;
};

VertexOut VS(VertexIn vin, uniform bool isUsingAtlas)
{
	VertexOut vout;
	
	// Transform to world space space.
	vout.PosW    = mul(float4(vin.PosL, 1.0f), vin.World).xyz;
	vout.NormalW = mul(vin.NormalL, (float3x3)vin.World);
		
	// Transform to homogeneous clip space.
	vout.PosH = mul(float4(vout.PosW, 1.0f), gViewProj);
	float2 texCoord = (isUsingAtlas) ? float2((vin.Tex.x / 2.0f) + (1.0f / 2.0f * vin.AtlasCoord.x),
							(vin.Tex.y / 2.0f) + (1.0f / 2.0f * vin.AtlasCoord.y))
							: vin.Tex;

	// Output vertex attributes for interpolation across triangle.
	vout.Tex   = mul(float4(texCoord, 0.0f, 1.0f), gTexTransform).xy;
	vout.Material = vin.Material;
	return vout;
}

float4 PS(VertexOut pin, uniform bool gUseTexure) : SV_Target
{
	// Interpolating normal can unnormalize it, so normalize it.
    pin.NormalW = normalize(pin.NormalW);

	// The toEye vector is used in lighting.
	float3 toEye = gEyePosW - pin.PosW;

	// Cache the distance to the eye from this surface point.
	float distToEye = length(toEye); 

	// Normalize.
	toEye /= distToEye;
	
    // Default to multiplicative identity.
    float4 texColor = float4(1, 1, 1, 1);
    if(gUseTexure)
	{
		// Sample texture.
		texColor = gDiffuseMap.Sample( samAnisotropic, pin.Tex );
	}
	 
	//
	// Lighting.
	//
	float4 litColor = texColor;

	// Start with a sum of zero. 
	float4 ambient = float4(0.0f, 0.0f, 0.0f, 0.0f);
	float4 diffuse = float4(0.0f, 0.0f, 0.0f, 0.0f);
	float4 spec    = float4(0.0f, 0.0f, 0.0f, 0.0f);

	
	// Sum the light contribution from each light source.  
	[unroll]
	for(int i = 0; i < MAX_LIGHTS; ++i)
	{
		if (testLights[i].On.x == 1)
		{
			float4 A, D, S;
			ComputePointLight(pin.Material, testLights[i], pin.PosW, pin.NormalW, toEye, A, D, S);

			ambient += A;
			diffuse += D;
			spec    += S;
		}
	}

	float4 A, D, S;
	ComputeDirectionalLight(pin.Material, gDirLights[0], pin.NormalW, toEye, 
				A, D, S);

	ambient += A;
	diffuse += D;
	spec    += S;

	// Modulate with late add.
	litColor = texColor*(ambient + diffuse) + spec;

	// Common to take alpha from diffuse material and texture.
	litColor.a = pin.Material.Diffuse.a * texColor.a;

    return litColor;
}

#pragma region DX11 Techniques
technique11 LightsWithAtlas
{
    pass P0
    {
        SetVertexShader( CompileShader( vs_5_0, VS(true) ) );
		SetGeometryShader( NULL );
        SetPixelShader( CompileShader( ps_5_0, PS(true) ) );
    }
}

technique11 LightsWithoutAtlas
{
    pass P0
    {
        SetVertexShader( CompileShader( vs_5_0, VS(false) ) );
		SetGeometryShader( NULL );
        SetPixelShader( CompileShader( ps_5_0, PS(true) ) );
    }
}
#pragma endregion

#pragma region DX10 Techniques
technique11 LightsWithAtlasDX10
{
    pass P0
    {
        SetVertexShader( CompileShader( vs_4_0, VS(true) ) );
		SetGeometryShader( NULL );
        SetPixelShader( CompileShader( ps_4_0, PS(true) ) );
    }
}

technique11 LightsWithoutAtlasDX10
{
    pass P0
    {
        SetVertexShader( CompileShader( vs_4_0, VS(false) ) );
		SetGeometryShader( NULL );
        SetPixelShader( CompileShader( ps_4_0, PS(true) ) );
    }
}
#pragma endregion