//example of some shaders compiled
flat basic.vs flat.fs
texture basic.vs texture.fs
skybox basic.vs skybox.fs
depth quad.vs depth.fs
multi basic.vs multi.fs
compute test.cs
singlepass basic.vs single_phong.fs
multipass basic.vs multi_phong.fs
plain basic.vs plain.fs

\test.cs
#version 430 core

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() 
{
	vec4 i = vec4(0.0);
}

\basic.vs

#version 330 core

in vec3 a_vertex;
in vec3 a_normal;
in vec2 a_coord;
in vec4 a_color;

uniform vec3 u_camera_position;

uniform mat4 u_model;
uniform mat4 u_viewprojection;

//this will store the color for the pixel shader
out vec3 v_position;
out vec3 v_world_position;
out vec3 v_normal;
out vec2 v_uv;
out vec4 v_color;

uniform float u_time;

void main()
{	
	//calcule the normal in camera space (the NormalMatrix is like ViewMatrix but without traslation)
	v_normal = (u_model * vec4( a_normal, 0.0) ).xyz;
	
	//calcule the vertex in object space
	v_position = a_vertex;
	v_world_position = (u_model * vec4( v_position, 1.0) ).xyz;
	
	//store the color in the varying var to use it from the pixel shader
	v_color = a_color;

	//store the texture coordinates
	v_uv = a_coord;

	//calcule the position of the vertex using the matrices
	gl_Position = u_viewprojection * vec4( v_world_position, 1.0 );
}

\quad.vs

#version 330 core

in vec3 a_vertex;
in vec2 a_coord;
out vec2 v_uv;

void main()
{	
	v_uv = a_coord;
	gl_Position = vec4( a_vertex, 1.0 );
}


\flat.fs

#version 330 core

uniform vec4 u_color;

out vec4 FragColor;

void main()
{
	FragColor = u_color;
}


\texture.fs

#version 330 core

in vec3 v_position;
in vec3 v_world_position;
in vec3 v_normal;
in vec2 v_uv;
in vec4 v_color;

uniform vec4 u_color;
uniform sampler2D u_texture;
uniform float u_time;
uniform float u_alpha_cutoff;

out vec4 FragColor;

void main()
{
	vec2 uv = v_uv;
	vec4 color = u_color;
	color *= texture( u_texture, v_uv );

	if(color.a < u_alpha_cutoff)
		discard;

	FragColor = color;
}


\skybox.fs

#version 330 core

in vec3 v_position;
in vec3 v_world_position;

uniform samplerCube u_texture;
uniform vec3 u_camera_position;
out vec4 FragColor;

void main()
{
	vec3 E = v_world_position - u_camera_position;
	vec4 color = texture( u_texture, E );
	FragColor = color;
}


\multi.fs

#version 330 core

in vec3 v_position;
in vec3 v_world_position;
in vec3 v_normal;
in vec2 v_uv;

uniform vec4 u_color;
uniform sampler2D u_texture;
uniform float u_time;
uniform float u_alpha_cutoff;

layout(location = 0) out vec4 FragColor;
layout(location = 1) out vec4 NormalColor;

void main()
{
	vec2 uv = v_uv;
	vec4 color = u_color;
	color *= texture( u_texture, uv );

	if(color.a < u_alpha_cutoff)
		discard;

	vec3 N = normalize(v_normal);

	FragColor = color;
	NormalColor = vec4(N,1.0);
}


\depth.fs

#version 330 core

uniform vec2 u_camera_nearfar;
uniform sampler2D u_texture; //depth map
in vec2 v_uv;
out vec4 FragColor;

void main()
{
	float n = u_camera_nearfar.x;
	float f = u_camera_nearfar.y;
	float z = texture2D(u_texture,v_uv).x;
	if( n == 0.0 && f == 1.0 )
		FragColor = vec4(z);
	else
		FragColor = vec4( n * (z + 1.0) / (f + n - z * (f - n)) );
}


\instanced.vs

#version 330 core

in vec3 a_vertex;
in vec3 a_normal;
in vec2 a_coord;

in mat4 u_model;

uniform vec3 u_camera_position;

uniform mat4 u_viewprojection;

//this will store the color for the pixel shader
out vec3 v_position;
out vec3 v_world_position;
out vec3 v_normal;
out vec2 v_uv;

void main()
{	
	//calcule the normal in camera space (the NormalMatrix is like ViewMatrix but without traslation)
	v_normal = (u_model * vec4( a_normal, 0.0) ).xyz;
	
	//calcule the vertex in object space
	v_position = a_vertex;
	v_world_position = (u_model * vec4( a_vertex, 1.0) ).xyz;
	
	//store the texture coordinates
	v_uv = a_coord;

	//calcule the position of the vertex using the matrices
	gl_Position = u_viewprojection * vec4( v_world_position, 1.0 );
}

\utils

//from this github repo
mat3 cotangent_frame(vec3 N, vec3 p, vec2 uv)
{
	// get edge vectors of the pixel triangle
	vec3 dp1 = dFdx( p );
	vec3 dp2 = dFdy( p );
	vec2 duv1 = dFdx( uv );
	vec2 duv2 = dFdy( uv );

	// solve the linear system
	vec3 dp2perp = cross( dp2, N );
	vec3 dp1perp = cross( N, dp1 );
	vec3 T = dp2perp * duv1.x + dp1perp * duv2.x;
	vec3 B = dp2perp * duv1.y + dp1perp * duv2.y;

	// construct a scale-invariant frame
	float invmax = inversesqrt( max( dot(T,T), dot(B,B)));
	return mat3( T * invmax, B * invmax, N );
}

// assume N, the interpolated vertex normal and
// WP the world position
//vec3 normal_pixel = texture2D( normalmap, uv ) .xyz;
vec3 perturbNormal(vec3 N, vec3 WP, vec2 uv, vec3 normal_pixel)
{
	normal_pixel = normal_pixel * 255./127. - 128./127.;
	mat3 TBN = cotangent_frame(N, WP, uv);
	return normalize(TBN * normal_pixel);
}

\single_phong.fs

#version 330 core

#include utils

in vec3 v_position;
in vec3 v_world_position;
in vec3 v_normal;
in vec2 v_uv;
in vec4 v_color;

uniform float u_time;
uniform vec3 u_camera_position;

// start light inputs =========================================================
uniform vec3 u_ambient_light;
uniform int u_light_count;

const uint MAX_LIGHTS = 5;

const int NO_LIGHT = 		0;
const int LT_POINT = 		1;
const int LT_SPOT =			2;
const int LT_DIRECTIONAL = 	3;

uniform float u_light_intensity[MAX_LIGHTS];
uniform float u_light_type[MAX_LIGHTS];
uniform vec3 u_light_position[MAX_LIGHTS];
uniform vec3 u_light_color[MAX_LIGHTS];
uniform vec3 u_light_direction[MAX_LIGHTS];
uniform vec2 u_light_cone[MAX_LIGHTS]; // alpha_min and alpha_max in radians
// end light inputs ===========================================================


// start material-related inputs ==============================================
uniform vec4 u_color;
uniform float u_alpha_cutoff;

uniform float u_roughness;
// uniform float u_metallic;
// end material-related inputs ================================================


// start maps =================================================================
const uint MAX_MAPS = 6;

// texture types
const int ALBEDO				= 0;
const int EMISSIVE				= 1;
const int OPACITY				= 2;
const int METALLIC_ROUGHNESS	= 3;
const int OCCLUSION				= 4;
const int NORMALMAP				= 5;

uniform int u_maps[MAX_MAPS];
uniform sampler2D u_texture;
uniform sampler2D u_normal_map;
uniform sampler2D u_texture_metallic_roughness;
// end maps ===================================================================

out vec4 FragColor;

void main()
{
	vec2 uv = v_uv;
	vec4 color = u_color; // should always be 1 if not changed somehow

	if (u_maps[ALBEDO]) {
		color *= texture( u_texture, v_uv ); // ka = kd = ks = color (in our implementation)
	}
	
	vec4 metallic_roughness = vec4(0.0);
	if (u_maps[METALLIC_ROUGHNESS]) {
		metallic_roughness = texture( u_texture_metallic_roughness, v_uv ) * 255.0;
	}
	// float metallic = u_metallic * metallic_roughness.x; // red is metallic
	float roughness = u_roughness * metallic_roughness.y; // green is roughness

	float shininess = 255.0 - roughness;

	if(color.a < u_alpha_cutoff)
		discard;

	// add ambient term
	vec3 final_light = u_ambient_light;

	vec3 diffuse_term, specular_term, light_intensity, L, R;
	float N_dot_L, R_dot_V, dist, numerator;
	
	vec3 N = normalize(v_normal);
	vec3 V = normalize(u_camera_position - v_world_position); // -V is vertex_world_position

	if (u_maps[NORMALMAP]) {
		vec3 texture_normal = texture(u_normal_map, uv).xyz;
		texture_normal = (texture_normal * 2.0) - 1.0;
		texture_normal = normalize(texture_normal);
		N = perturbNormal(N, -V, uv, texture_normal);
	}

	for (int i=0; i<u_light_count; i++)
	{
		// diffuse
		if (u_light_type[i] == LT_DIRECTIONAL) {
			L = u_light_direction[i];
			L = normalize(L);
			light_intensity = u_light_color[i] * u_light_intensity[i]; // No attenuation for directional light
		} else if (u_light_type[i] == LT_POINT) {
			L = u_light_position[i] - v_world_position;
			dist = length(L); // used in light intensity
			L = normalize(L);
			light_intensity = u_light_color[i] * u_light_intensity[i] / pow(dist, 2); // light intensity reduced by distance
		} else if (u_light_type[i] == LT_SPOT) {
			L = u_light_position[i] - v_world_position;
			dist = length(L); // used in light intensity
			L = normalize(L);
			numerator = clamp(dot(L, normalize(u_light_direction[i])), 0.0, 1.0) - cos(u_light_cone[i].y);
			light_intensity = vec3(0.0);
			if (numerator >= 0) {
				light_intensity = u_light_color[i] * u_light_intensity[i] / pow(dist, 2);
				light_intensity *= numerator;
				light_intensity /= (cos(u_light_cone[i].x) - cos(u_light_cone[i].y));
			}
		}

		N_dot_L = clamp(dot(N, L), 0.0, 1.0);
		diffuse_term = N_dot_L * light_intensity;

		// specular
		R = reflect(-L, N); // minus because of reflect function first argument is incident
		R_dot_V = clamp(dot(R, V), 0.0, 1.0);

		specular_term = pow(R_dot_V, shininess) * light_intensity;

		// add diffuse and specular terms
		final_light += diffuse_term;
		final_light += specular_term;
	}

	FragColor = vec4(final_light * color.xyz, color.a);
}

\multi_phong.fs

#version 330 core

#include utils

in vec3 v_position;
in vec3 v_world_position;
in vec3 v_normal;
in vec2 v_uv;
in vec4 v_color;

uniform float u_time;
uniform vec3 u_camera_position;

// start light inputs =========================================================
uniform vec3 u_ambient_light;

const int NO_LIGHT = 		0;
const int LT_POINT = 		1;
const int LT_SPOT =			2;
const int LT_DIRECTIONAL = 	3;

uniform float u_light_intensity;
uniform float u_light_type;
uniform vec3 u_light_position;
uniform vec3 u_light_color;
uniform vec3 u_light_direction;
uniform vec2 u_light_cone; // alpha_min and alpha_max in radians
// end light inputs ===========================================================


// start material-related inputs ==============================================
uniform vec4 u_color;
uniform float u_alpha_cutoff;

uniform float u_roughness;
// uniform float u_metallic;
// end material-related inputs ================================================


// start maps =================================================================
const uint MAX_MAPS = 6;

// texture types
const int ALBEDO				= 0;
const int EMISSIVE				= 1;
const int OPACITY				= 2;
const int METALLIC_ROUGHNESS	= 3;
const int OCCLUSION				= 4;
const int NORMALMAP				= 5;

uniform int u_maps[MAX_MAPS];
uniform sampler2D u_texture;
uniform sampler2D u_normal_map;
uniform sampler2D u_texture_metallic_roughness;
// end maps ===================================================================

out vec4 FragColor;

void main()
{
	vec2 uv = v_uv;
	vec4 color = u_color; // should always be 1 if not changed somehow

	if (u_maps[ALBEDO]) {
		color *= texture( u_texture, v_uv ); // ka = kd = ks = color (in our implementation)
	}
	
	vec4 metallic_roughness = vec4(0.0);
	if (u_maps[METALLIC_ROUGHNESS]) {
		metallic_roughness = texture( u_texture_metallic_roughness, v_uv ) * 255.0;
	}
	// float metallic = u_metallic * metallic_roughness.x; // red is metallic
	float roughness = u_roughness * metallic_roughness.y; // green is roughness

	float shininess = 255.0 - roughness;

	if(color.a < u_alpha_cutoff)
		discard;

	// add ambient term
	vec3 final_light = u_ambient_light;

	vec3 diffuse_term, specular_term, light_intensity, L, R;
	float N_dot_L, R_dot_V, dist, numerator;
	
	vec3 N = normalize(v_normal);
	vec3 V = normalize(u_camera_position - v_world_position); // -V is vertex_world_position

	if (u_maps[NORMALMAP]) {
		vec3 texture_normal = texture(u_normal_map, uv).xyz;
		texture_normal = (texture_normal * 2.0) - 1.0;
		texture_normal = normalize(texture_normal);
		N = perturbNormal(N, -V, uv, texture_normal);
	}

	// diffuse
	if (u_light_type == LT_DIRECTIONAL) {
		L = u_light_direction;
		L = normalize(L);
		light_intensity = u_light_color * u_light_intensity; // No attenuation for directional light
	} else if (u_light_type == LT_POINT) {
		L = u_light_position - v_world_position;
		dist = length(L); // used in light intensity
		L = normalize(L);
		light_intensity = u_light_color * u_light_intensity / pow(dist, 2); // light intensity reduced by distance
	} else if (u_light_type == LT_SPOT) {
		L = u_light_position - v_world_position;
		dist = length(L); // used in light intensity
		L = normalize(L);
		numerator = clamp(dot(L, normalize(u_light_direction)), 0.0, 1.0) - cos(u_light_cone.y);
		light_intensity = vec3(0.0);
		if (numerator >= 0) {
			light_intensity = u_light_color * u_light_intensity / pow(dist, 2);
			light_intensity *= numerator;
			light_intensity /= (cos(u_light_cone.x) - cos(u_light_cone.y));
		}
	}

	N_dot_L = clamp(dot(N, L), 0.0, 1.0);
	diffuse_term = N_dot_L * light_intensity;

	// specular
	R = reflect(-L, N); // minus because of reflect function first argument is incident
	R_dot_V = clamp(dot(R, V), 0.0, 1.0);

	specular_term = pow(R_dot_V, shininess) * light_intensity;

	// add diffuse and specular terms
	final_light += diffuse_term;
	final_light += specular_term;

	FragColor = vec4(final_light * color.xyz, color.a);
}

\plain.fs

#version 330 core

uniform vec4 u_color;
uniform float u_alpha_cutoff;

out vec4 FragColor;

void main ()
{
	vec4 color = u_color;

	if(color.a < u_alpha_cutoff)
		discard;

	FragColor = vec4(0.0, 0.0, 0.0, 1.0);
}