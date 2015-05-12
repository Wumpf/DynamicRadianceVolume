

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
vec3 BRDF(vec3 toLight, vec3 toCamera, vec3 normal, vec3 baseColor, vec2 roughnessMetalic)
{
	// Normalized Blinn Phong as in Realtime Rendering (3rd edition) page 257 (eq 7.48)

	// Converting the parameters Roughness and Metalness to BRDF specific values is not entirely straigh forward.
	// Some useful resources on the topic:
	// - Feeding a physically based shading model https://seblagarde.wordpress.com/2011/08/17/feeding-a-physical-based-lighting-mode/
	// - Marmoset PBR Texture Conversion Tutorial: http://www.marmoset.co/toolbag/learn/pbr-conversion
	// - Unity Material charts http://blogs.unity3d.com/2015/02/18/working-with-physically-based-shading-a-practical-approach/
	// 		(this gives a good hint of what they do internally with metallic) 
	float blinnExponent = 1.0 / (roughnessMetalic.x * roughnessMetalic.x + 0.0005); // Completely made up
	vec3 diffuseColor = mix(baseColor, vec3(0.02), roughnessMetalic.y);	// Almost dark for Metalic values
	vec3 specularColor = mix(vec3(0.0356), baseColor, roughnessMetalic.y); // Greyish for specular. Matches value of Marmoset Tutorial which fits into the range given at the unity material chart.

	float normalization = (blinnExponent + 8.0) / 8.0;
	vec3 halfVector = normalize(toCamera + toLight);
	float blinnPhong = normalization * pow(saturate(dot(normal, halfVector)), blinnExponent);

	return (diffuseColor + blinnPhong * specularColor) / PI;
}