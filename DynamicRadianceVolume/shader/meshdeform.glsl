vec3 WorldPosDeform(vec3 worldPosition)
{
	//worldPosition.y += sin(PassedTime * 0.5 + worldPosition.x * 0.1) * 5.0;
	return worldPosition;
}
vec3 NormalDeform(vec3 normal, vec3 orginalWorldPos)
{
	return normal;

	// See http://http.developer.nvidia.com/GPUGems/gpugems_ch42.html
	// WorldPosDeform: f(pos) = vec3(pos.x, pos.y + sin(PassedTime * 0.5 + pos.x * 0.1) * 5.0, pow.z)
	mat3 jacobian = mat3(1, 0, 0,
						 0.5 * cos(PassedTime * 0.5 + orginalWorldPos.x * 0.1), 1, 0,
						 0, 0, 1);

	return normalize((inverse(jacobian)) * normal);
}