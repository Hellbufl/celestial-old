struct vInput
{
	float4 position : POSITION;
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

	output.position = mul(input.position, transpose(mView));
	output.color = input.color;

    return output;
}