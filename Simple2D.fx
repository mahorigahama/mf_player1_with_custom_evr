SamplerState MeshTextureSampler
{
	Filter = MIN_MAG_MIP_LINEAR;
	AddressU = Wrap;
	AddressV = Wrap;
};

Texture2D tex : register(t0);

struct VS_OUTPUT {
	float4 Pos : SV_POSITION;
	float4 Color : COLOR;
	float2 UV : TEXCOORD;
};

VS_OUTPUT VS(float4 Pos : POSITION, float4 Color : COLOR, float2 UV : TEXCOORD)
{
	VS_OUTPUT Out = (VS_OUTPUT)0;
	Out.Pos = Pos;
	Out.Color = Color;
	Out.UV = UV;
	return Out;
}

float4 PS(VS_OUTPUT In) : SV_Target
{
	return tex.Sample(MeshTextureSampler, In.UV) + In.Color;
}

technique10 SimpleRender
{
	pass P0
	{
		SetVertexShader( CompileShader( vs_4_0, VS() ) );
		SetGeometryShader( NULL );
		SetPixelShader( CompileShader( ps_4_0, PS() ) );
	}
}
