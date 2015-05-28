

float ComputeSpotFalloff(float cosToLight)
{
	// Linear falloff as used in the Mitsuba renderer
	//return saturate(acos(LightCosHalfAngle) - acos(cosToLight)) / (acos(LightCosHalfAngle));

	// Much nicer and faster falloff
	return saturate(cosToLight - LightCosHalfAngle) / (1.0 - LightCosHalfAngle);
}

float ComputeSpotFalloff(vec3 toLight)
{
	return ComputeSpotFalloff(dot(-toLight, LightDirection));
}


// Converting the parameters Roughness and Metalness to BRDF specific values is not entirely straigh forward.
// Some useful resources on the topic:
// - Feeding a physically based shading model https://seblagarde.wordpress.com/2011/08/17/feeding-a-physical-based-lighting-mode/
// - Marmoset PBR Texture Conversion Tutorial: http://www.marmoset.co/toolbag/learn/pbr-conversion
// - http://www.marmoset.co/toolbag/learn/pbr-practice
// - Unity Material charts http://blogs.unity3d.com/2015/02/18/working-with-physically-based-shading-a-practical-approach/
// 		(this gives a good hint of what they do internally with metallic) 
// - General pbr math and some lookup values http://blog.selfshadow.com/publications/s2014-shading-course/hoffman/s2014_pbs_physics_math_slides.pdf

float RoughnessToBlinnExponent(float roughness)
{
	//roughness = 1.0 - roughness;
	float rsq = roughness * roughness;
	return 2 / (rsq * rsq + 0.0005); // similar to http://graphicrants.blogspot.de/2013/08/specular-brdf-reference.html
}
float BlinnNormalization(float blinnExponent)
{
	return (blinnExponent + 8.0) / (8.0 * PI);
}
void ComputeMaterialColors(vec3 baseColor, float metallic, out vec3 diffuseColor, out vec3 specularColor)
{
	diffuseColor = mix(baseColor, vec3(0.02), metallic);	// Almost dark for Metalic values
	specularColor = mix(vec3(0.04), baseColor, metallic); 	// Greyish for specular. 0.04 is about the the default for everything except metall http://blog.selfshadow.com/publications/s2014-shading-course/hoffman/s2014_pbs_physics_math_slides.pdf
}

// Evaluates default BRDF.
// Attention: Mind vector directions of incoming parameters!
vec3 BRDF(vec3 toLight, vec3 toCamera, vec3 normal, vec3 baseColor, vec2 roughnessMetalic)
{
	// Normalized Blinn Phong as in Realtime Rendering (3rd edition) page 257 (eq 7.48)
	float blinnExponent = RoughnessToBlinnExponent(roughnessMetalic.x);
	vec3 diffuseColor, specularColor;
	ComputeMaterialColors(baseColor, roughnessMetalic.y, diffuseColor, specularColor);

	float normalization = BlinnNormalization(blinnExponent);
	vec3 halfVector = normalize(toCamera + toLight);
	float blinnPhong = normalization * pow(saturate(dot(normal, halfVector)), blinnExponent);

	return diffuseColor / PI + blinnPhong * specularColor;
}