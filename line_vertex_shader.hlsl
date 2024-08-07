struct vInput
{
	float4 position : POSITION;
	float4 nextPosition : NEXTPOSITION;
	float thickness : THICC;
    float4 color : COLOR;
};

struct vOut
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
};

cbuffer viewMatrixBuffer
{
	matrix mView;
};

vOut main(const vInput input)
{
    vOut output;

	float4 clipPos = mul(input.position, transpose(mView));
	float4 clipNextPos = mul(input.nextPosition, transpose(mView));

	float3 tangent = clipNextPos - clipPos;
    float3 clip3 = clipPos;
    float3 normal = normalize(cross(clip3, tangent));

	float4 offset = float4(normal.x, normal.y, normal.z, 0.0f);

	output.position = clipPos + offset * input.thickness;
	output.color = input.color;

    return output;
}