

float ComputeSpotFalloff(float cosToLight)
{
	// Linear falloff as used in the Mitsuba renderer
	return saturate(acos(LightCosHalfAngle) - acos(cosToLight)) / (acos(LightCosHalfAngle));

	// Much nicer and faster falloff
	//return saturate(cosToLight - LightCosHalfAngle) / (1.0 - LightCosHalfAngle);
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
	vec3 brdf = diffuseColor / PI;

	return brdf;
}
