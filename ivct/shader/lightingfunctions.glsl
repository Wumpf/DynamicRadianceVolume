

float ComputeSpotFalloff(float cosToLight)
{
	return sqrt(saturate(cosToLight - LightCosHalfAngle) / (1.0 - LightCosHalfAngle));
}
float ComputeSpotFalloff(vec3 toLight)
{
	return ComputeSpotFalloff(dot(-toLight, LightDirection));
}


// Evaluates default BRDF.
// Attention: Parameter directions are not as (often) given in theoretical notation!
vec3 BRDF(vec3 toLight, vec3 toCamera, vec3 diffuseColor)
{
	// Diffuse Term
	vec3 brdf = diffuseColor; // Omitted 1/PI!

	return brdf;
}
