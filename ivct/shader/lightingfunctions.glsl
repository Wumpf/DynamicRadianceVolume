

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
// Attention: Mind vector directions of incoming parameters!
vec3 BRDF(vec3 toLight, vec3 toCamera, vec3 normal, vec3 diffuseColor, vec3 specularColor, float roughness)
{
	// Normalized Blinn Phong as in Realtime Rendering (3rd edition) page 257 (eq 7.48)

	float normalization = (roughness + 8.0) / 8.0;
	vec3 halfVector = normalize(toCamera + toLight);
	float blinnPhong = normalization * pow(saturate(dot(normal, halfVector)), roughness);

	return (diffuseColor + blinnPhong * specularColor) / PI;
}