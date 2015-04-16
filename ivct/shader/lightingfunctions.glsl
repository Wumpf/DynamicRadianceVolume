
float ComputeSpotFalloff(vec3 toLight)
{
	return sqrt(saturate(dot(-toLight, LightDirection) - LightCosHalfAngle) / (1.0 - LightCosHalfAngle));
}