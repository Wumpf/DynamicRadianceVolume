#define PI 3.14159265358979
#define PI_2 6.28318530717958

float saturate(float x)
{
	return clamp(x, 0.0, 1.0);
}
vec2 saturate(vec2 x)
{
	return clamp(x, vec2(0.0), vec2(1.0));
}
vec3 saturate(vec3 x)
{
	return clamp(x, vec3(0.0), vec3(1.0));
}
vec4 saturate(vec4 x)
{
	return clamp(x, vec4(0.0), vec4(1.0));
}
float sqr(float x)
{
	return x*x;
}
vec2 sqr(vec2 x)
{
	return x*x;
}
vec3 sqr(vec3 x)
{
	return x*x;
}
vec4 sqr(vec4 x)
{
	return x*x;
}

float GetLuminance(vec3 rgb)
{
	const vec3 W = vec3(0.212671, 0.715160, 0.072169);
	return dot(rgb, W);
}

// Expects: vec2(atan(normal.y, normal.x), normal.z)
vec3 UnpackNormal(vec2 packedNormal)
{
	float sinPhi = sqrt(1.0 - packedNormal.y*packedNormal.y);
	return normalize(vec3(cos(packedNormal.x)*sinPhi, sin(packedNormal.x)*sinPhi, packedNormal.y));
}

// Expects: vec2(atan(normal.y, normal.x), normal.z), remapped to 16bit int
vec3 UnpackNormal16I(vec2 packedNormal)
{
	// Unpack from 16bit signed int.
	packedNormal.x *= PI / 32768.0;
	packedNormal.y *= 1.0 / 32768.0;

	return UnpackNormal(packedNormal);
}


// Returns: vec2(atan(normal.y, normal.x), normal.z), packed for 16 bit signed integer output
vec2 PackNormal(vec3 normal)
{
	// Invalid implementation which tries to remove division by 0 with an epsilon:
	//const float ATAN_EPS = 0.0001;
	//return vec2(atan(normal.y, step(abs(normal.x), ATAN_EPS) * ATAN_EPS + normal.x), normal.z);

	// A mathematical correct and more robust alternative would be using 2 atan functions:
	// http://stackoverflow.com/questions/26070410/robust-atany-x-on-glsl-for-converting-xy-coordinate-to-angle
	//return vec2(PI/2 - mix(atan(normal.x, normal.y), atan(normal.y, normal.x), abs(normal.x) > abs(normal.y)), normal.z);

	// Alternative fix (selfmade):
	// When x is zero then cos(packedNormal0)==0. So packedNormal0 may be +-PI/2
	// dependent on the sign of y.
	vec2 packedNormal;
	packedNormal.x = normal.x == 0.0 ? (sign(normal.y)*PI/2) : atan(normal.y, normal.x);
	packedNormal.y = normal.z;

	return packedNormal;
}

// Returns: vec2(atan(normal.y, normal.x), normal.z)
ivec2 PackNormal16I(vec3 normal)
{
	vec2 packedNormal = PackNormal(normal);

	// Pack to 16bit signed int.
	return ivec2(packedNormal.x * (32768.0 / PI), packedNormal.y * 32768.0);
}

bool IsOnScreen(vec4 clipspaceCoordinate)
{
	return all(lessThanEqual(clipspaceCoordinate.xyz, clipspaceCoordinate.www)) && 
			all(lessThanEqual(vec3(-clipspaceCoordinate.ww, 0.0), clipspaceCoordinate.xyz));
}


// Possible hemispherical projections.
//#define HEMIPROJECTION_LAMBERT
#define HEMIPROJECTION_LAMBERT_CONCENTRICQUAD

// Projects a direction in the z+ hemisphere to the 0-1 square
vec2 HemisphericalProjection(vec3 hemisphereDirection)
{
	// Equal area hemisphere map, ranging from -1 to 1 - http://en.wikipedia.org/wiki/User:Mgnbar/Hemispherical_projection
	vec2 projectedHalfVec = sqrt((1.0 - hemisphereDirection.z) / dot(hemisphereDirection.xy, hemisphereDirection.xy)) * hemisphereDirection.xy;
#ifdef HEMIPROJECTION_LAMBERT
	return saturate(projectedHalfVec * 0.5 + 0.5);
#else
	// Original implementation of concentric disc map - https://mediatech.aalto.fi/~jaakko/T111-5310/K2013/JGT-97.pdf
	float r = length(projectedHalfVec);
	float phi = atan(projectedHalfVec.y, projectedHalfVec.x);

	if(phi < -PI/4) phi += PI_2;

	float a,b;
	if(phi < PI/4)
	{
		a = r;
		b = phi * a / (PI/4);
	}
	else if(phi < 3 * PI/4)
	{
		b = r;
		a = -(phi - PI/2) * b / (PI/4);
	}
	else if(phi < 5* PI/4)
	{
		a = -r;
		b = (phi - PI) * a / (PI/4);
	}
	else
	{
		b = -r;
		a = -(phi - 3*PI/2) * b / (PI/4);
	}

	return vec2(a*0.5 + 0.5, b*0.5 + 0.5);
#endif
}
float GetBlinnPhongExponentForHemispherePixel(float hemisphereResolution)
{
	// BlinnPhong cap size: 2pi / (exponent + 1)

#ifdef HEMIPROJECTION_LAMBERT
	// Hemisphere Pixel size: 2pi / (res²/4 * pi)
	return hemisphereResolution * hemisphereResolution *0.25 * PI - 1.0;
#else
	// Hemisphere Pixel size: 2pi / res²
	return hemisphereResolution * hemisphereResolution - 1.0;
#endif


}
float GetHemisphereLodForBlinnPhongExponent(float blinnPhongExponent, float baseLevelResolution)
{
	// See GetBlinnPhongExponentForHemispherePixel but resolved to mip level
#ifdef HEMIPROJECTION_LAMBERT
	return log2(PI * baseLevelResolution * baseLevelResolution / (4.0 + 4.0 * blinnPhongExponent))  * 0.5;
#else
	return log2(baseLevelResolution * baseLevelResolution / (1.0 + blinnPhongExponent)) * 0.5;
#endif
}