struct vOut
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
};

float4 main(const vOut input) : SV_TARGET
{
	float4 outCol = input.color;
	return outCol;
}