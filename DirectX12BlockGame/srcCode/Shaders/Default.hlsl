#include "LightingUtil.hlsl"

Texture2D    gTexture2D : register(t0);
SamplerState gASampler  : register(s0);

cbuffer cbPass : register(b0)
{
    float4x4 ViewProj;
    float4 Eye;
    float4 LightDir;
    float4 LightPower;
    float4 Ambient;
};

cbuffer cbMatarial : register(b1)
{
    float roughness;
    float3 FrR0;
};

cbuffer cbObj : register(b2)
{
    float WPosX;
    float WPosY;
    float WPosZ;
};

struct VertexIn
{
	float3 PosL    : POSITION;
	float3 NormalL : NORMAL;
	float2 TexC    : TEXCOORD;
};

struct VertexOut
{
	float4 PosH    : SV_POSITION;
	float3 PosW    : POSITION;
	float3 NormalW : NORMAL;
	float2 TexC    : TEXCOORD;
};

VertexOut VS(VertexIn vin)
{
    VertexOut vout = (VertexOut)0.0f;

    float4x4 World = { 1.0f,0.0f,0.0f ,0.0f,
                      0.0f ,1.0f ,0.0f ,0.0f,
                      0.0f ,0.0f ,1.0f ,0.0f,
                      WPosX,WPosY,WPosZ,1.0f };


    float4 posW = mul(float4(vin.PosL, 1.0f), World);
    vout.PosW = posW.xyz;

    vout.NormalW = vin.NormalL;


    vout.PosH = mul(posW, ViewProj);

    vout.TexC = vin.TexC;

    return vout;
}

float4 PS(VertexOut pin) : SV_Target
{
    float4 diffuseAlbedo = gTexture2D.Sample(gASampler, pin.TexC);

    pin.NormalW = normalize(pin.NormalW);

    float3 toEyeW = normalize(float3(Eye.x,Eye.y,Eye.z) - pin.PosW);

    float4 ambient = Ambient * diffuseAlbedo;

    const float shininess = 1.0f - roughness;
    Material mat = { diffuseAlbedo, FrR0, shininess };
    Light L;
    L.Direction = LightDir;
    L.Strength = LightPower;
    float3 shadowFactor = 1.0f;
    float4 directLight = float4(ComputeDirectionalLight(L, mat, pin.NormalW, toEyeW),0.0f);

    float4 litColor = ambient + directLight;

    litColor.a = diffuseAlbedo.a;

    return litColor;
}