//example of some shaders compiled
flat basic.vs flat.fs
texture basic.vs texture.fs
skybox basic.vs skybox.fs
depth quad.vs depth.fs
multi basic.vs multi.fs
compute test.cs

shadows_plain basic.vs shadows_plain.fs

// forward shaders
singlepass_phong_forward basic.vs singlepass_phong_forward.fs
multipass_phong_forward basic.vs multipass_phong_forward.fs

singlepass_pbr_forward basic.vs singlepass_pbr_forward.fs

// deferred shaders
fill_gbuffer basic.vs fill_gbuffer.fs
ssao_compute quad.vs ssao_compute.fs
volumetric_rendering_compute quad.vs volumetric_rendering_compute.fs
upsample_half_to_full_rgb quad.vs upsample_half_to_full_rgb.fs
upsample_half_to_full_rgba quad.vs upsample_half_to_full_rgba.fs
screen_space_reflections_firstpass quad.vs screen_space_reflections_firstpass.fs

singlepass_phong_deferred quad.vs singlepass_phong_deferred.fs
multipass_phong_deferred_firstpass quad.vs multipass_phong_deferred_firstpass.fs
multipass_phong_deferred basic.vs multipass_phong_deferred.fs

singlepass_pbr_deferred quad.vs singlepass_pbr_deferred.fs

deferred_to_viewport quad.vs deferred_to_viewport.fs
deferred_tonemapper_to_viewport quad.vs deferred_tonemapper_to_viewport.fs

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

// =========================================== INCLUDES ===========================================

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

vec3 perturbNormal(vec3 N, vec3 WP, vec2 uv, vec3 normal_pixel)
{
	mat3 TBN = cotangent_frame(N, WP, uv);
	return normalize(TBN * normal_pixel);
}

\constants

// light types
const int NO_LIGHT = 		0;
const int LT_POINT = 		1;
const int LT_SPOT =			2;
const int LT_DIRECTIONAL = 	3;
const int MAX_LIGHTS = 5;

// texture types
const int ALBEDO				= 0;
const int EMISSIVE				= 1;
const int OPACITY				= 2;
const int METALLIC_ROUGHNESS	= 3;
const int OCCLUSION				= 4;
const int NORMALMAP				= 5;
const int MAX_MAPS = 6;

\lights

uniform vec3 u_ambient_light;

// for singlepass
uniform int u_light_count;
uniform float u_light_intensities[MAX_LIGHTS];
uniform float u_light_types[MAX_LIGHTS];
uniform vec3 u_light_positions[MAX_LIGHTS];
uniform vec3 u_light_colors[MAX_LIGHTS];
uniform vec3 u_light_directions[MAX_LIGHTS];
uniform vec2 u_light_cones[MAX_LIGHTS]; // alpha_min and alpha_max in radians

// for multipass
uniform float u_light_intensity;
uniform float u_light_type;
uniform vec3 u_light_position;
uniform vec3 u_light_color;
uniform vec3 u_light_direction;
uniform vec2 u_light_cone; // alpha_min and alpha_max in radians

\shadows

// start shadowmaps inputs ====================================================
uniform sampler2D u_shadow_atlas;

uniform int u_light_cast_shadows;
uniform mat4 u_shadowmap_viewprojection;
uniform float u_shadowmap_bias;

uniform int u_light_cast_shadowss[MAX_LIGHTS];
uniform mat4 u_shadowmap_viewprojections[MAX_LIGHTS];
uniform float u_shadowmap_biases[MAX_LIGHTS];

uniform ivec2 u_shadow_atlas_dims; // (cols, rows) of shadow atlas
uniform ivec2 u_shadow_atlas_offset; 
uniform int u_atlas_indices[MAX_LIGHTS];
// end shadowmaps inputs ======================================================

float compute_shadow_factor(vec3 world_position)
{
	// project to light homogeneous space
	vec4 proj_pos = u_shadowmap_viewprojection * vec4(world_position, 1.0);
	proj_pos.z -= u_shadowmap_bias;

	// from homogeneus space to clip space
	vec4 proj_pos_clip = proj_pos / proj_pos.w;

	// from clip space to uv space
	vec3 proj_pos_uv = (proj_pos_clip.xyz + vec3(1.0)) / 2.0;

	// get point depth in uv space
	float real_depth = proj_pos_uv.z;

	// take into account the atlas offset
	vec2 uv_in_atlas = (proj_pos_uv.xy + vec2(u_shadow_atlas_offset)) / vec2(u_shadow_atlas_dims);

	// read depth from depth buffer in uv space
	float shadow_depth = texture(u_shadow_atlas, uv_in_atlas).x;

	if( shadow_depth < real_depth )
		return 0.0;
	return 1.0;
}

float compute_shadow_factor_singlepass(int i, vec3 world_position)
{
	// project to light homogeneous space
	vec4 proj_pos = u_shadowmap_viewprojections[i] * vec4(world_position, 1.0);
	proj_pos.z -= u_shadowmap_biases[i];

	// from homogeneus space to clip space
	vec4 proj_pos_clip = proj_pos / proj_pos.w;

	// from clip space to uv space
	vec3 proj_pos_uv = proj_pos_clip.xyz * 0.5 + 0.5;

	// get point depth in uv space
	float real_depth = proj_pos_uv.z;

	// take into account the atlas offset
	vec2 shadow_atlas_offset = vec2(mod(u_atlas_indices[i], u_shadow_atlas_dims.x), u_atlas_indices[i] / u_shadow_atlas_dims.x);
	vec2 uv_in_atlas = (proj_pos_uv.xy + shadow_atlas_offset) / vec2(u_shadow_atlas_dims);

	// read depth from depth buffer in uv space
	float shadow_depth = texture(u_shadow_atlas, uv_in_atlas).x;

	if( shadow_depth < real_depth )
		return 0.0;
	return 1.0;
}

\pbr_functions

const float PI = 3.14159265359;

vec3 fresnel_schlick(vec3 V, vec3 H, vec3 f0) {
	return f0 + (vec3(1.0) - f0) * pow(1 - clamp(dot(H, V), 0.0, 1.0), 5.0);
}

float normal_dist_GGX(vec3 N, vec3 H, float alpha) {
	float alpha2 = pow(alpha, 2);
	return alpha2 / (PI * pow(pow(clamp(dot(N, H), 0.0, 1.0), 2) * (alpha2 - 1.0) + 1.0, 2));
}

float g1_schlick_GGX(vec3 v, vec3 N, float k) {
	float N_dot_v = clamp(dot(N, v), 0.0001, 1.0);
	return N_dot_v / (N_dot_v * (1.0 - k) + k);
}

float geometry_smith_GGX(vec3 L, vec3 V, vec3 N, float alpha) {
	float k = alpha / 2.0;
	return g1_schlick_GGX(L, N, k) * g1_schlick_GGX(V, N, k);
}

vec3 cook_torrance_reflectance(vec3 V, vec3 L, vec3 N, vec3 albedo, float roughness, float metalness) {
	float alpha = pow(clamp(roughness, 0.0001, 1.0), 2); // just in case clamp
	vec3 H = normalize(normalize(V) + normalize(L));
	
	vec3 F0 = mix(vec3(0.04), albedo, metalness);
	vec3 F = fresnel_schlick(V, H, F0);
	float D = normal_dist_GGX(N, H, alpha);
	float G = geometry_smith_GGX(L, V, N, alpha);

	//vec3 kd = mix(vec3(1.0) - F, vec3(0.0), metalness); // from https://github.com/Nadrin/PBR/blob/master/data/shaders/glsl/pbr_fs.glsl line 165
	vec3 kd = albedo;

	return (kd / PI) + (F * D * G) / (4.0 * clamp(dot(N, L), 0.0001, 1.0) * clamp(dot(N, V), 0.0001, 1.0)); // small delta to avoid division by 0
}

\hdr_tonemapping

uniform int u_lgc_active;

vec3 degamma(vec3 c)
{
	return pow(c,vec3(2.2));
}

vec3 gamma(vec3 c)
{
	return pow(c,vec3(1.0/2.2));
}

\shadows_plain.fs

#version 330 core

#include constants

in vec2 v_uv;

uniform vec4 u_color;
uniform float u_alpha_cutoff;
uniform sampler2D u_texture;
uniform int u_maps[MAX_MAPS];

out vec4 FragColor;

void main ()
{
	vec4 color = u_color; // should always be 1 if not changed somehow

	if (u_maps[ALBEDO] != 0){
		color *= texture( u_texture, v_uv ); // ka = kd = ks = color (in our implementation)
	}

	if(color.a < u_alpha_cutoff)
		discard;

	FragColor = vec4(0.0, 0.0, 0.0, 1.0);
}

\singlepass_phong_forward.fs

#version 330 core

#include utils
#include constants
#include lights
#include shadows
#include hdr_tonemapping

in vec3 v_position;
in vec3 v_world_position;
in vec3 v_normal;
in vec2 v_uv;
in vec4 v_color;

uniform float u_time;
uniform vec3 u_camera_position;

// start material-related inputs ==============================================
uniform vec4 u_color;
uniform float u_alpha_cutoff;

uniform float u_shininess;
// end material-related inputs ================================================


// start maps =================================================================
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

	if (u_maps[ALBEDO] != 0) {
		color *= texture( u_texture, v_uv ); // ka = kd = ks = color (in our implementation)
	}

	if(color.a < u_alpha_cutoff)
		discard;

	// add ambient term
	vec3 final_light = u_ambient_light;

	// degamma (to linear) correction if active
	vec3 light_colors[MAX_LIGHTS] = u_light_colors;
	if (u_lgc_active != 0) {
		color.rgb = degamma(color.rgb);
		final_light = degamma(final_light);
		for (int i=0; i<u_light_count; i++) {
			light_colors[i] = degamma(light_colors[i]);
		}
	}

	vec3 diffuse_term, specular_term, light_intensity, L, R;
	float N_dot_L, R_dot_V, dist, numerator;
	
	vec3 N = normalize(v_normal);
	vec3 V = normalize(u_camera_position - v_world_position);

	if (u_maps[NORMALMAP] != 0) {
		vec3 texture_normal = texture(u_normal_map, uv).xyz;
		texture_normal = (texture_normal * 2.0) - 1.0;
		texture_normal = normalize(texture_normal);
		N = perturbNormal(N, v_world_position, uv, texture_normal);
	}

	for (int i=0; i<u_light_count; i++)
	{
		// diffuse
		if (u_light_types[i] == LT_DIRECTIONAL) {
			L = u_light_directions[i];
			L = normalize(L);

			float shadow_factor = 1.0;
			if (u_light_cast_shadowss[i] == 1)
				shadow_factor = compute_shadow_factor_singlepass(i, v_world_position);

			light_intensity = light_colors[i] * u_light_intensities[i] * shadow_factor; // No attenuation for directional light
		} else if (u_light_types[i] == LT_POINT) {
			L = u_light_positions[i] - v_world_position;
			dist = length(L); // used in light intensity
			L = normalize(L);
			light_intensity = light_colors[i] * u_light_intensities[i] / pow(dist, 2); // light intensity reduced by distance
		} else if (u_light_types[i] == LT_SPOT) {
			L = u_light_positions[i] - v_world_position;
			dist = length(L); // used in light intensity
			L = normalize(L);

			float shadow_factor = 1.0;
			if (u_light_cast_shadowss[i] == 1)
				shadow_factor = compute_shadow_factor_singlepass(i, v_world_position);

			numerator = clamp(dot(L, normalize(u_light_directions[i])), 0.0, 1.0) - cos(u_light_cones[i].y);
			light_intensity = vec3(0.0);
			if (numerator >= 0) {
				light_intensity = light_colors[i] * u_light_intensities[i] / pow(dist, 2);
				light_intensity *= numerator;
				light_intensity /= (cos(u_light_cones[i].x) - cos(u_light_cones[i].y));
				light_intensity *= shadow_factor;
			}
		} else {
			continue;
		}

		N_dot_L = clamp(dot(N, L), 0.0, 1.0);
		diffuse_term = N_dot_L * light_intensity;

		// specular
		R = reflect(-L, N); // minus because of reflect function first argument is incident
		R_dot_V = clamp(dot(R, V), 0.0, 1.0);

		specular_term = pow(R_dot_V, u_shininess) * light_intensity;

		// add diffuse and specular terms
		final_light += diffuse_term;
		final_light += specular_term;
	}

	vec3 final_color = final_light * color.xyz;

	// to gamma if linear / gamma correction is active
	if (u_lgc_active != 0) {
		final_color = gamma(final_color);
	}

	FragColor = vec4(final_color, color.a);
}

\multipass_phong_forward.fs

#version 330 core

#include utils
#include constants
#include lights
#include shadows
#include hdr_tonemapping

in vec3 v_position;
in vec3 v_world_position;
in vec3 v_normal;
in vec2 v_uv;
in vec4 v_color;

uniform float u_time;
uniform vec3 u_camera_position;

// start material-related inputs ==============================================
uniform vec4 u_color;
uniform float u_alpha_cutoff;

uniform float u_shininess;
// end material-related inputs ================================================


// start maps =================================================================
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

	if (u_maps[ALBEDO] != 0){
		color *= texture( u_texture, v_uv ); // ka = kd = ks = color (in our implementation)
	}

	if(color.a < u_alpha_cutoff)
		discard;

	// add ambient term
	vec3 final_light = u_ambient_light;

	// degamma (to linear) correction if active
	vec3 light_color = u_light_color;
	if (u_lgc_active != 0) {
		color.rgb = degamma(color.rgb);
		final_light = degamma(final_light);
		light_color = degamma(light_color);
	}

	vec3 diffuse_term, specular_term, light_intensity, L, R;
	float N_dot_L, R_dot_V, dist, numerator;
	
	vec3 N = normalize(v_normal);
	vec3 V = normalize(u_camera_position - v_world_position);

	if (u_maps[NORMALMAP] != 0) {
		vec3 texture_normal = texture(u_normal_map, uv).xyz;
		texture_normal = (texture_normal * 2.0) - 1.0;
		texture_normal = normalize(texture_normal);
		N = perturbNormal(N, v_world_position, uv, texture_normal);
	}

	// diffuse
	if (u_light_type == LT_DIRECTIONAL)
	{
		L = u_light_direction;
		float shadow_factor = 1.0;
		if (u_light_cast_shadows == 1) // La luz tiene una sombra
			shadow_factor = compute_shadow_factor(v_world_position);
		L = normalize(L);
		
		light_intensity = light_color * u_light_intensity * shadow_factor; // No attenuation for directional light
	}
	else if (u_light_type == LT_POINT)
	{
		L = u_light_position - v_world_position;
		dist = length(L); // used in light intensity
		L = normalize(L);

		light_intensity = light_color * u_light_intensity / pow(dist, 2); // light intensity reduced by distance
	}
	else if (u_light_type == LT_SPOT)
	{
		L = u_light_position - v_world_position;
		dist = length(L); // used in light intensity
		L = normalize(L);
		
		float shadow_factor = 1.0;
		if (u_light_cast_shadows == 1) // La luz tiene una sombra
			shadow_factor = compute_shadow_factor(v_world_position);

		numerator = clamp(dot(L, normalize(u_light_direction)), 0.0, 1.0) - cos(u_light_cone.y);
		light_intensity = vec3(0.0);
		
		if (numerator >= 0) {
			light_intensity = light_color * u_light_intensity / pow(dist, 2);
			light_intensity *= numerator;
			light_intensity /= (cos(u_light_cone.x) - cos(u_light_cone.y));
			light_intensity *= shadow_factor;
		}
	} else {
		discard;
	}

	N_dot_L = clamp(dot(N, L), 0.0, 1.0);
	diffuse_term = N_dot_L * light_intensity;

	// specular
	R = reflect(-L, N); // minus because of reflect function first argument is incident
	R_dot_V = clamp(dot(R, V), 0.0, 1.0);

	specular_term = pow(R_dot_V, u_shininess) * light_intensity;

	// add diffuse and specular terms
	final_light += diffuse_term;
	final_light += specular_term;

	vec3 final_color = final_light * color.xyz;

	// to gamma if linear / gamma correction is active
	if (u_lgc_active != 0) {
		final_color = gamma(final_color);
	}

	FragColor = vec4(final_color, color.a);
}

\fill_gbuffer.fs

#version 330 core

#include utils
#include constants
#include hdr_tonemapping

in vec3 v_position;
in vec3 v_world_position;
in vec3 v_normal;
in vec2 v_uv;
in vec4 v_color;

uniform float u_time;
uniform vec3 u_camera_position;

// start material-related inputs ==============================================
uniform vec4 u_color;
uniform float u_alpha_cutoff;
// end material-related inputs ================================================


// start maps =================================================================
uniform int u_maps[MAX_MAPS];
uniform sampler2D u_texture;
uniform sampler2D u_normal_map;
uniform sampler2D u_texture_metallic_roughness;
// end maps ===================================================================

layout(location = 0) out vec4 gbuffer_albedo;
layout(location = 1) out vec4 gbuffer_normal_mat;

void main()
{
	vec2 uv = v_uv;
	vec4 color = u_color; // should always be 1 if not changed somehow

	if (u_maps[ALBEDO] != 0){
		color *= texture( u_texture, v_uv ); // ka = kd = ks = color (in our implementation)
	}

	// here only degamma color
	if (u_lgc_active != 0) {
		color.rgb = degamma(color.rgb);
	}

	if(color.a < u_alpha_cutoff)
		discard;
	
	vec3 N = normalize(v_normal);
	vec3 V = normalize(u_camera_position - v_world_position);

	if (u_maps[NORMALMAP] != 0) {
		vec3 texture_normal = texture(u_normal_map, uv).xyz;
		texture_normal = (texture_normal * 2.0) - 1.0;
		texture_normal = normalize(texture_normal);
		//N = perturbNormal(N, v_world_position, uv, texture_normal);
	}

	vec3 bao_rou_met = vec3(1.0);
	if (u_maps[METALLIC_ROUGHNESS] != 0){
		bao_rou_met *= texture( u_texture_metallic_roughness, v_uv ).rgb;
	}

	gbuffer_albedo = vec4(color.xyz, bao_rou_met.g);
	gbuffer_normal_mat = vec4(N*0.5+0.5, bao_rou_met.b);
}

\singlepass_phong_deferred.fs

#version 330 core

#include constants
#include lights
#include shadows
#include hdr_tonemapping

in vec2 v_uv;

uniform sampler2D u_gbuffer_color;
uniform sampler2D u_gbuffer_normal;
uniform sampler2D u_gbuffer_depth;
uniform sampler2D u_ssao_texture;

uniform vec2 u_res_inv;
uniform mat4 u_inv_vp_mat;
uniform vec3 u_camera_position;
uniform float u_shininess;
uniform vec3 u_bg_color;
uniform int u_ssao_active;

layout(location = 0) out vec4 illumination;

void main()
{
	vec2 uv = gl_FragCoord.xy * u_res_inv;

	float depth = texture(u_gbuffer_depth, uv).r;
	float depth_clip = depth * 2.0 - 1.0;
	
	vec2 uv_clip = uv * 2.0 - 1.0;
	vec4 clip_coords = vec4( uv_clip.x, uv_clip.y, depth_clip, 1.0);
	vec4 not_norm_world_pos = u_inv_vp_mat * clip_coords;
	vec3 world_pos = not_norm_world_pos.xyz / not_norm_world_pos.w;

	vec3 color = texture(u_gbuffer_color, uv).rgb;
	
	vec3 final_light = u_ambient_light;

	// degamma (to linear) correction if active
	vec3 light_colors[MAX_LIGHTS] = u_light_colors;
	if (u_lgc_active != 0) {
		final_light = degamma(final_light);
		for (int i=0; i<u_light_count; i++) {
			light_colors[i] = degamma(light_colors[i]);
		}
	}

	if (u_ssao_active != 0) {
		final_light *= texture(u_ssao_texture, uv).rgb;
	}

	vec3 diffuse_term, specular_term, light_intensity, L, R;
	float N_dot_L, R_dot_V, dist, numerator;
	
	vec3 N0 = texture(u_gbuffer_normal, uv).rgb;
	vec3 N = N0 * 2.0 - 1.0;
	vec3 V = normalize(u_camera_position - world_pos);

	// if the normal is equal to the background color --> skip shading (for skybox)
	// calculate the squared error, since it seems that comparisons are not performed properly
	vec3 tmp = N0 - u_bg_color;
	tmp = tmp * tmp;
	if (tmp.x + tmp.y + tmp.z < 0.0001){
		discard;
	}

	for (int i=0; i<u_light_count; i++)
	{
		// diffuse
		if (u_light_types[i] == LT_DIRECTIONAL) {
			L = u_light_directions[i];
			L = normalize(L);

			float shadow_factor = 1.0;
			if (u_light_cast_shadowss[i] == 1)
				shadow_factor = compute_shadow_factor_singlepass(i, world_pos);

			light_intensity = light_colors[i] * u_light_intensities[i] * shadow_factor; // No attenuation for directional light
		} else if (u_light_types[i] == LT_POINT) {
			L = u_light_positions[i] - world_pos;
			dist = length(L); // used in light intensity
			L = normalize(L);
			light_intensity = light_colors[i] * u_light_intensities[i] / pow(dist, 2); // light intensity reduced by distance
		} else if (u_light_types[i] == LT_SPOT) {
			L = u_light_positions[i] - world_pos;
			dist = length(L); // used in light intensity
			L = normalize(L);

			float shadow_factor = 1.0;
			if (u_light_cast_shadowss[i] == 1)
				shadow_factor = compute_shadow_factor_singlepass(i, world_pos);

			numerator = clamp(dot(L, normalize(u_light_directions[i])), 0.0, 1.0) - cos(u_light_cones[i].y);
			light_intensity = vec3(0.0);
			if (numerator >= 0) {
				light_intensity = light_colors[i] * u_light_intensities[i] / pow(dist, 2);
				light_intensity *= numerator;
				light_intensity /= (cos(u_light_cones[i].x) - cos(u_light_cones[i].y));
				light_intensity *= shadow_factor;
			}
		} else {
			continue;
		}

		N_dot_L = clamp(dot(N, L), 0.0, 1.0);
		diffuse_term = N_dot_L * light_intensity;

		// specular
		R = reflect(-L, N); // minus because of reflect function first argument is incident
		R_dot_V = clamp(dot(R, V), 0.0, 1.0);

		specular_term = pow(R_dot_V, u_shininess) * light_intensity;

		// add diffuse and specular terms
		final_light += diffuse_term;
		final_light += specular_term;
	}

	illumination = vec4(final_light * color, 1.0);
}

\multipass_phong_deferred_firstpass.fs

#version 330 core

#include constants
#include lights
#include shadows
#include hdr_tonemapping

in vec2 v_uv;

uniform sampler2D u_gbuffer_color;
uniform sampler2D u_gbuffer_normal;
uniform sampler2D u_gbuffer_depth;
uniform sampler2D u_ssao_texture;

uniform vec2 u_res_inv;
uniform mat4 u_inv_vp_mat;
uniform vec3 u_camera_position;
uniform float u_shininess;
uniform vec3 u_bg_color;
uniform int u_ssao_active;

layout(location = 0) out vec4 illumination;

void main()
{
	vec2 uv = gl_FragCoord.xy * u_res_inv;

	float depth = texture(u_gbuffer_depth, uv).r;
	float depth_clip = depth * 2.0 - 1.0;
	
	vec2 uv_clip = uv * 2.0 - 1.0;
	vec4 clip_coords = vec4( uv_clip.x, uv_clip.y, depth_clip, 1.0);
	vec4 not_norm_world_pos = u_inv_vp_mat * clip_coords;
	vec3 world_pos = not_norm_world_pos.xyz / not_norm_world_pos.w;
	
	vec3 color = texture(u_gbuffer_color, uv).rgb;

	vec3 final_light = u_ambient_light;

	// degamma (to linear) correction if active
	vec3 light_colors[MAX_LIGHTS] = u_light_colors;
	if (u_lgc_active != 0) {
		final_light = degamma(final_light);
		for (int i=0; i<u_light_count; i++) {
			light_colors[i] = degamma(light_colors[i]);
		}
	}

	if (u_ssao_active != 0) {
		final_light *= texture(u_ssao_texture, uv).rgb;
	}

	vec3 diffuse_term, specular_term, light_intensity, L, R;
	float N_dot_L, R_dot_V, dist, numerator;
	
	vec3 N0 = texture(u_gbuffer_normal, uv).rgb;
	vec3 N = N0 * 2.0 - 1.0;
	vec3 V = normalize(u_camera_position - world_pos);

	// if the normal is equal to the background color --> skip shading (for skybox)
	// calculate the squared error, since it seems that comparisons are not performed properly
	vec3 tmp = N0 - u_bg_color;
	tmp = tmp * tmp;
	if (tmp.x + tmp.y + tmp.z < 0.0001){
		discard;
	}

	for (int i=0; i<u_light_count; i++)
	{
		// diffuse
		if (u_light_types[i] == LT_DIRECTIONAL) {
			L = u_light_directions[i];
			L = normalize(L);

			float shadow_factor = 1.0;
			if (u_light_cast_shadowss[i] == 1)
				shadow_factor = compute_shadow_factor_singlepass(i, world_pos);

			light_intensity = light_colors[i] * u_light_intensities[i] * shadow_factor; // No attenuation for directional light
		} else {
			continue;
		}

		N_dot_L = clamp(dot(N, L), 0.0, 1.0);
		diffuse_term = N_dot_L * light_intensity;

		// specular
		R = reflect(-L, N); // minus because of reflect function first argument is incident
		R_dot_V = clamp(dot(R, V), 0.0, 1.0);

		specular_term = pow(R_dot_V, u_shininess) * light_intensity;

		// add diffuse and specular terms
		final_light += diffuse_term;
		final_light += specular_term;
	}

	illumination = vec4(final_light * color, 1.0);
}

\multipass_phong_deferred.fs

#version 330 core

#include constants
#include lights
#include shadows
#include hdr_tonemapping

in vec2 v_uv;

uniform sampler2D u_gbuffer_color;
uniform sampler2D u_gbuffer_normal;
uniform sampler2D u_gbuffer_depth;

uniform vec2 u_res_inv;
uniform mat4 u_inv_vp_mat;
uniform vec3 u_camera_position;
uniform float u_shininess;
uniform vec3 u_bg_color;
uniform int u_light_id;

layout(location = 0) out vec4 illumination;

void main()
{
	vec2 uv = gl_FragCoord.xy * u_res_inv;

	float depth = texture(u_gbuffer_depth, uv).r;
	float depth_clip = depth * 2.0 - 1.0;
	
	vec2 uv_clip = uv * 2.0 - 1.0;
	vec4 clip_coords = vec4( uv_clip.x, uv_clip.y, depth_clip, 1.0);
	vec4 not_norm_world_pos = u_inv_vp_mat * clip_coords;
	vec3 world_pos = not_norm_world_pos.xyz / not_norm_world_pos.w;
	
	vec3 color = texture(u_gbuffer_color, uv).rgb;

	vec3 final_light = vec3(0.0);

	// degamma (to linear) correction if active
	// NOTE: it is weird that in multipass we use the arrays and not the single values, but we did it that way and now we are not changing it
	vec3 light_colors[MAX_LIGHTS] = u_light_colors;
	if (u_lgc_active != 0) {
		final_light = degamma(final_light);
		for (int i=0; i<u_light_count; i++) {
			light_colors[i] = degamma(light_colors[i]);
		}
	}

	vec3 diffuse_term, specular_term, light_intensity, L, R;
	float N_dot_L, R_dot_V, dist, numerator;
	
	vec3 N0 = texture(u_gbuffer_normal, uv).rgb;
	vec3 N = N0 * 2.0 - 1.0;
	vec3 V = normalize(u_camera_position - world_pos);

	// if the normal is equal to the background color --> skip shading (for skybox)
	// calculate the squared error, since it seems that comparisons are not performed properly
	vec3 tmp = N0 - u_bg_color;
	tmp = tmp * tmp;
	if (tmp.x + tmp.y + tmp.z < 0.0001){
		discard;
	}

	// first pass, we do ambient and directional lights
	int i = u_light_id;

	if (u_light_types[i] == LT_POINT) {
		L = u_light_positions[i] - world_pos;
		dist = length(L); // used in light intensity
		L = normalize(L);
		light_intensity = light_colors[i] * u_light_intensities[i] / pow(dist, 2); // light intensity reduced by distance
	} else if (u_light_types[i] == LT_SPOT) {
		L = u_light_positions[i] - world_pos;
		dist = length(L); // used in light intensity
		L = normalize(L);

		float shadow_factor = 1.0;
		if (u_light_cast_shadowss[i] == 1)
			shadow_factor = compute_shadow_factor_singlepass(i, world_pos);

		numerator = clamp(dot(L, normalize(u_light_directions[i])), 0.0, 1.0) - cos(u_light_cones[i].y);
		light_intensity = vec3(0.0);
		if (numerator >= 0) {
			light_intensity = light_colors[i] * u_light_intensities[i] / pow(dist, 2);
			light_intensity *= numerator;
			light_intensity /= (cos(u_light_cones[i].x) - cos(u_light_cones[i].y));
			light_intensity *= shadow_factor;
		}
	} else {
		discard;
	}

	N_dot_L = clamp(dot(N, L), 0.0, 1.0);
	diffuse_term = N_dot_L * light_intensity;

	// specular
	R = reflect(-L, N); // minus because of reflect function first argument is incident
	R_dot_V = clamp(dot(R, V), 0.0, 1.0);

	specular_term = pow(R_dot_V, u_shininess) * light_intensity;

	// add diffuse and specular terms
	final_light += diffuse_term;
	final_light += specular_term;

	illumination = vec4(final_light * color, 1.0);
}

\deferred_to_viewport.fs

#version 330 core

#include hdr_tonemapping

in vec2 v_uv;

uniform vec3 u_bg_color;

uniform sampler2D u_texture;

uniform sampler2D u_vr_texture;
uniform int u_vr_active;

layout(location = 0) out vec4 FragColor;

void main()
{
	vec2 uv = v_uv;
	vec3 color = texture( u_texture, v_uv ).rgb;	// if the normal is equal to the background color --> skip shading (for skybox)

	if (u_vr_active != 0) {
		vec4 volumetric_vals = texture( u_vr_texture, v_uv );

		vec3 in_scattering = volumetric_vals.xyz;
		vec3 transmittance = vec3(volumetric_vals.w);

		color = color * transmittance + in_scattering;
	}

	// calculate the squared error, since it seems that comparisons are not performed properly
	vec3 tmp = color.xyz - u_bg_color;
	tmp = tmp * tmp;
	if (tmp.x + tmp.y + tmp.z < 0.0001){
		discard;
	}

	// in deferred degamma here
	if (u_lgc_active != 0) {
		color = gamma(color);
	}

	FragColor = vec4(color, 1.0);
}

\singlepass_pbr_forward.fs

#version 330 core

#include utils
#include constants
#include lights
#include shadows
#include pbr_functions
#include hdr_tonemapping

in vec3 v_position;
in vec3 v_world_position;
in vec3 v_normal;
in vec2 v_uv;
in vec4 v_color;

uniform float u_time;
uniform vec3 u_camera_position;

// start material-related inputs ==============================================
uniform vec4 u_color;
uniform float u_alpha_cutoff;
// end material-related inputs ================================================


// start maps =================================================================
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

	if (u_maps[ALBEDO] != 0) {
		color *= texture( u_texture, v_uv ); // ka = kd = ks = color (in our implementation)
	}

	if(color.a < u_alpha_cutoff)
		discard;

	// add ambient term
	vec3 final_light = u_ambient_light;

	// degamma (to linear) correction if active
	vec3 light_colors[MAX_LIGHTS] = u_light_colors;
	if (u_lgc_active != 0) {
		color.rgb = degamma(color.rgb);
		final_light = degamma(final_light);
		for (int i=0; i<u_light_count; i++) {
			light_colors[i] = degamma(light_colors[i]);
		}
	}

	vec3 light_intensity, L;
	float dist, numerator;
	
	vec3 N = normalize(v_normal);
	vec3 V = normalize(u_camera_position - v_world_position);

	if (u_maps[NORMALMAP] != 0) {
		vec3 texture_normal = texture(u_normal_map, uv).xyz;
		texture_normal = (texture_normal * 2.0) - 1.0;
		texture_normal = normalize(texture_normal);
		N = perturbNormal(N, v_world_position, uv, texture_normal);
	}

	vec3 bao_rou_met = vec3(1.0);
	if (u_maps[METALLIC_ROUGHNESS] != 0){
		bao_rou_met *= texture( u_texture_metallic_roughness, v_uv ).rgb;
	}

	for (int i=0; i<u_light_count; i++)
	{
		// diffuse
		if (u_light_types[i] == LT_DIRECTIONAL) {
			L = u_light_directions[i];
			L = normalize(L);

			float shadow_factor = 1.0;
			if (u_light_cast_shadowss[i] == 1)
				shadow_factor = compute_shadow_factor_singlepass(i, v_world_position);

			light_intensity = light_colors[i] * u_light_intensities[i] * shadow_factor; // No attenuation for directional light
		} else if (u_light_types[i] == LT_POINT) {
			L = u_light_positions[i] - v_world_position;
			dist = length(L); // used in light intensity
			L = normalize(L);
			light_intensity = light_colors[i] * u_light_intensities[i] / pow(dist, 2); // light intensity reduced by distance
		} else if (u_light_types[i] == LT_SPOT) {
			L = u_light_positions[i] - v_world_position;
			dist = length(L); // used in light intensity
			L = normalize(L);

			float shadow_factor = 1.0;
			if (u_light_cast_shadowss[i] == 1)
				shadow_factor = compute_shadow_factor_singlepass(i, v_world_position);

			numerator = clamp(dot(L, normalize(u_light_directions[i])), 0.0, 1.0) - cos(u_light_cones[i].y);
			light_intensity = vec3(0.0);
			if (numerator >= 0) {
				light_intensity = light_colors[i] * u_light_intensities[i] / pow(dist, 2);
				light_intensity *= numerator;
				light_intensity /= (cos(u_light_cones[i].x) - cos(u_light_cones[i].y));
				light_intensity *= shadow_factor;
			}
		} else {
			continue;
		}

		final_light += light_intensity * cook_torrance_reflectance(V, L, N, color.rgb, bao_rou_met.g, bao_rou_met.b);
	}

	vec3 final_color = final_light * color.xyz;
	if (u_lgc_active != 0) {
		final_color = gamma(final_color);
	}
	FragColor = vec4(final_color, color.a);
}

\singlepass_pbr_deferred.fs

#version 330 core

#include constants
#include lights
#include shadows
#include pbr_functions
#include hdr_tonemapping

in vec2 v_uv;

uniform sampler2D u_gbuffer_color;
uniform sampler2D u_gbuffer_normal;
uniform sampler2D u_gbuffer_depth;
uniform sampler2D u_ssao_texture;

uniform vec2 u_res_inv;
uniform mat4 u_inv_vp_mat;
uniform vec3 u_camera_position;
uniform vec3 u_bg_color;
uniform int u_ssao_active;

layout(location = 0) out vec4 illumination;

void main()
{
	vec2 uv = gl_FragCoord.xy * u_res_inv;

	float depth = texture(u_gbuffer_depth, uv).r;
	float depth_clip = depth * 2.0 - 1.0;
	
	vec2 uv_clip = uv * 2.0 - 1.0;
	vec4 clip_coords = vec4( uv_clip.x, uv_clip.y, depth_clip, 1.0);
	vec4 not_norm_world_pos = u_inv_vp_mat * clip_coords;
	vec3 world_pos = not_norm_world_pos.xyz / not_norm_world_pos.w;

	vec4 gbuffer_fbo1 = texture(u_gbuffer_color, uv);

	vec3 color = gbuffer_fbo1.rgb;
	float roughness = gbuffer_fbo1.a;
	
	vec3 final_light = u_ambient_light;

	// degamma (to linear) correction if active
	vec3 light_colors[MAX_LIGHTS] = u_light_colors;
	if (u_lgc_active != 0) {
		final_light = degamma(final_light);
		for (int i=0; i<u_light_count; i++) {
			light_colors[i] = degamma(light_colors[i]);
		}
	}

	if (u_ssao_active != 0) {
		final_light *= texture(u_ssao_texture, uv).rgb;
	}

	vec3 light_intensity, L;
	float dist, numerator;
	
	vec4 gbuffer_fbo2 = texture(u_gbuffer_normal, uv);
	float metalness = gbuffer_fbo2.a;

	vec3 N0 = gbuffer_fbo2.rgb;
	vec3 N = N0 * 2.0 - 1.0;
	vec3 V = normalize(u_camera_position - world_pos);

	// if the normal is equal to the background color --> skip shading (for skybox)
	// calculate the squared error, since it seems that comparisons are not performed properly
	vec3 tmp = N0 - u_bg_color;
	tmp = tmp * tmp;
	if (tmp.x + tmp.y + tmp.z < 0.0001){
		discard;
	}

	for (int i=0; i<u_light_count; i++)
	{
		// diffuse
		if (u_light_types[i] == LT_DIRECTIONAL) {
			L = u_light_directions[i];
			L = normalize(L);

			float shadow_factor = 1.0;
			if (u_light_cast_shadowss[i] == 1)
				shadow_factor = compute_shadow_factor_singlepass(i, world_pos);

			light_intensity = light_colors[i] * u_light_intensities[i] * shadow_factor; // No attenuation for directional light
		} else if (u_light_types[i] == LT_POINT) {
			L = u_light_positions[i] - world_pos;
			dist = length(L); // used in light intensity
			L = normalize(L);
			light_intensity = light_colors[i] * u_light_intensities[i] / pow(dist, 2); // light intensity reduced by distance
		} else if (u_light_types[i] == LT_SPOT) {
			L = u_light_positions[i] - world_pos;
			dist = length(L); // used in light intensity
			L = normalize(L);

			float shadow_factor = 1.0;
			if (u_light_cast_shadowss[i] == 1)
				shadow_factor = compute_shadow_factor_singlepass(i, world_pos);

			numerator = clamp(dot(L, normalize(u_light_directions[i])), 0.0, 1.0) - cos(u_light_cones[i].y);
			light_intensity = vec3(0.0);
			if (numerator >= 0) {
				light_intensity = light_colors[i] * u_light_intensities[i] / pow(dist, 2);
				light_intensity *= numerator;
				light_intensity /= (cos(u_light_cones[i].x) - cos(u_light_cones[i].y));
				light_intensity *= shadow_factor;
			}
		} else {
			continue;
		}

		final_light += light_intensity * cook_torrance_reflectance(V, L, N, color, roughness, metalness);
	}

	illumination = vec4(final_light * color, 1.0);
}

\ssao_compute.fs

#version 330 core

const int MAX_SSAO_SAMPLES = 64;

in vec2 v_uv;

uniform sampler2D u_gbuffer_normal;
uniform sampler2D u_gbuffer_depth;

uniform int u_sample_count;
uniform float u_sample_radius;
uniform vec3 u_sample_pos[MAX_SSAO_SAMPLES];
uniform int u_ssao_plus_active;

uniform vec2 u_res_inv;
uniform mat4 u_proj_mat;
uniform mat4 u_inv_proj_mat;
uniform mat4 u_view_mat;

layout(location = 0) out vec3 ssao_fbo;

void main() {
	// On the shader:
	// Centering the UV coords in the middle
	// of the pixel
	vec2 uv = v_uv + 0.5 * u_res_inv;

	// The depth is in range (0, 1)
	float depth = texture(u_gbuffer_depth, uv).r;

	// Skip if we are in the sky
	if (depth >= 1.0) {
		ssao_fbo = vec3(1.0);
		return;
	}

	vec3 N0 = vec3(1.0);
	vec4 N = vec4(1.0);
	if (u_ssao_plus_active != 0) {
		N0 *= texture(u_gbuffer_normal, uv).xyz * 2.0 - 1.0;
		N = u_view_mat * vec4(N0, 0.0);
		N0 = N.xyz;
	}
	N0 = normalize(N0); // just in case
	//N = N * 2.0 - 1.0;

	vec3 v = vec3(0.0, 1.0, 0.0); // make random !!! check out https://www.aussiedwarf.com/2017/05/09/Random10Bit
	vec3 T = normalize(v - N0 * dot(v, N0));
	vec3 B = cross(N0, T);
	mat3 rotmat = mat3(T, B, N0);

	vec4 clip_coords = vec4(uv, depth, 1.0);
	clip_coords.xyz = clip_coords.xyz * 2.0 - 1.0;

	// Inverse the clip coordinates (-1, 1) to
	// view coordinates
	vec4 view_sample_origin = u_inv_proj_mat * clip_coords;
	view_sample_origin /= view_sample_origin.w;

	float ao_term = 0.0;
	for(int i = 0; i < u_sample_count; i++) {
		vec3 view_sample = u_sample_pos[i];
		if (u_ssao_plus_active != 0) {
			view_sample = rotmat * view_sample;
		}
		view_sample *= u_sample_radius;
		view_sample += view_sample_origin.xyz;

		// Project the view space sample
		// Remember to normalize!
		// Result is clip space (-1,1)
		vec4 proj_sample = u_proj_mat * vec4(view_sample, 1.0);
		proj_sample /= proj_sample.w;

		vec2 sample_uv = proj_sample.xy * 0.5 + 0.5;

		// The depth buffer is in the range (0, 1)
		float sample_depth = texture(u_gbuffer_depth, sample_uv).r;

		// Compare the sample_depth and the
		// proj_sample.z. If it’s not occluded
		// increment the ao_term.
		if (sample_depth > (proj_sample.z * 0.5 + 0.5)) {
			ao_term += 1.0;
		}
	}
	ao_term =ao_term / float(u_sample_count);
	ssao_fbo = vec3(ao_term);
}

\deferred_tonemapper_to_viewport.fs

#version 330 core

#include hdr_tonemapping

in vec2 v_uv;

uniform vec3 u_bg_color;
uniform sampler2D u_texture;

uniform sampler2D u_vr_texture;
uniform int u_vr_active;

uniform float u_scale; //color scale before tonemapper
uniform float u_average_lum; 
uniform float u_lumwhite2;
uniform float u_igamma; //inverse gamma

layout(location = 0) out vec4 FragColor;

void main() {
	vec4 color = texture( u_texture, v_uv );
	vec3 rgb = color.xyz;

	if (u_vr_active != 0) {
		vec4 volumetric_vals = texture( u_vr_texture, v_uv );

		vec3 in_scattering = volumetric_vals.xyz;
		vec3 transmittance = vec3(volumetric_vals.w);

		rgb = rgb * transmittance + in_scattering;
	}

	// calculate the squared error, since it seems that comparisons are not performed properly
	vec3 tmp = rgb - u_bg_color;
	tmp = tmp * tmp;
	if (tmp.x + tmp.y + tmp.z < 0.0001){
		discard;
	}

	float lum = dot(rgb, vec3(0.2126, 0.7152, 0.0722));
	float L = (u_scale / u_average_lum) * lum;
	float Ld = (L * (1.0 + L / u_lumwhite2)) / (1.0 + L);

	rgb = (rgb / lum) * Ld;
	rgb = max(rgb,vec3(0.001));
	rgb = pow( rgb, vec3( u_igamma ) );

	// in deferred degamma here
	if (u_lgc_active != 0) {
		rgb = gamma(rgb);
	}

	FragColor = vec4(rgb, color.a);
}

\volumetric_rendering_compute.fs

#version 330 core

#include constants
#include lights
#include shadows
#include hdr_tonemapping

const float MAX_VR_HEIGHT = 5.0;
in vec2 v_uv;

uniform sampler2D u_gbuffer_depth;

uniform vec2 u_res_inv;
uniform mat4 u_inv_vp_mat;
uniform vec3 u_camera_position;

uniform int u_raymarching_steps;
uniform float u_max_ray_len;
uniform float u_air_density;
uniform float u_vr_vertical_density_factor;

layout(location = 0) out vec4 vol_fbo;

void main() {
	// Typical deferred preamble to get the world position of a fragment
	vec2 uv = gl_FragCoord.xy * u_res_inv;

	float depth = texture(u_gbuffer_depth, uv).r;
	float depth_clip = depth * 2.0 - 1.0;
	
	vec2 uv_clip = uv * 2.0 - 1.0;
	vec4 clip_coords = vec4( uv_clip.x, uv_clip.y, depth_clip, 1.0);
	vec4 not_norm_world_pos = u_inv_vp_mat * clip_coords;
	vec3 world_pos = not_norm_world_pos.xyz / not_norm_world_pos.w;

	// degamma (to linear) correction if active
	vec3 light_colors[MAX_LIGHTS] = u_light_colors;
	if (u_lgc_active != 0) {
		for (int i=0; i<u_light_count; i++) {
			light_colors[i] = degamma(light_colors[i]);
		}
	}

	// Raymarching variables
	vec3 ray_dir = world_pos - u_camera_position;
	float ray_dist = length(ray_dir);
	ray_dist = min(ray_dist, u_max_ray_len);
	ray_dir = normalize(ray_dir);

	float ray_step = ray_dist / float(u_raymarching_steps);
	vec3 sample_pos = u_camera_position;
	float air_density;
	
	// Temporal variable declarations
	vec3 L, light_intensity;
	float dist, shadow_factor, numerator;
	
	// Accum variables
	float transmittance = 1.0;
	vec3 in_scatter = vec3(0.0);

	for (int i = 0; i < u_raymarching_steps; i++) {
		sample_pos += ray_step * ray_dir;
		//air_density = u_air_density + (MAX_VR_HEIGHT - sample_pos.y) / MAX_VR_HEIGHT * u_vr_vertical_density_factor;
		air_density = u_air_density + exp(-u_vr_vertical_density_factor * sample_pos.y / MAX_VR_HEIGHT) * u_air_density;
		air_density = clamp(air_density, 0.0001, u_air_density * 3.0);

		// Typical singlepass lighting loop
		for (int j = 0; j < u_light_count; j++) {
			shadow_factor = 1.0;
			if (u_light_types[j] == LT_DIRECTIONAL) {
				L = u_light_directions[j];
				L = normalize(L);

				if (u_light_cast_shadowss[j] == 1)
					shadow_factor = compute_shadow_factor_singlepass(j, sample_pos);

				light_intensity = light_colors[j] * u_light_intensities[j] * shadow_factor; // No attenuation for directional light
			} else if (u_light_types[j] == LT_POINT) {
				L = u_light_positions[j] - sample_pos;
				dist = length(L); // used in light intensity
				L = normalize(L);
				light_intensity = light_colors[j] * u_light_intensities[j] / pow(dist, 2); // light intensity reduced by distance
			} else if (u_light_types[j] == LT_SPOT) {
				L = u_light_positions[j] - sample_pos;
				dist = length(L); // used in light intensity
				L = normalize(L);

				if (u_light_cast_shadowss[j] == 1)
					shadow_factor = compute_shadow_factor_singlepass(j, sample_pos);

				numerator = clamp(dot(L, normalize(u_light_directions[j])), 0.0, 1.0) - cos(u_light_cones[j].y);
				light_intensity = vec3(0.0);
				if (numerator >= 0) {
					light_intensity = light_colors[j] * u_light_intensities[j] / pow(dist, 2);
					light_intensity *= numerator;
					light_intensity /= (cos(u_light_cones[j].x) - cos(u_light_cones[j].y));
					light_intensity *= shadow_factor;
				}
			} else {
				continue;
			}

			in_scatter += light_intensity * air_density * ray_step;
		}

		// Reduce visibility
		transmittance -= air_density * ray_step;

		// Early out, if we have reached
		// full opacity
		if( 0.001 >= transmittance )
			break;
	}

	vol_fbo = vec4(in_scatter, transmittance);
}

\upsample_half_to_full_rgb.fs

#version 330 core

const int M = 6;
const int N = 2 * M + 1;

const float coeffs[N] = float[N](
    0.002216,  // G(-6)
    0.008764,  // G(-5)
    0.026995,  // G(-4)
    0.064759,  // G(-3)
    0.120985,  // G(-2)
    0.174697,  // G(-1)
    0.199471,  // G(0)
    0.174697,  // G(1)
    0.120985,  // G(2)
    0.064759,  // G(3)
    0.026995,  // G(4)
    0.008764,  // G(5)
    0.002216   // G(6)
);

uniform sampler2D u_texture_half;

uniform vec2 u_res_inv;

layout(location = 0) out vec3 texture_full;

void main()
{
	vec2 uv = gl_FragCoord.xy * u_res_inv;

	vec3 color = vec3(0.0);

	for (int i = 0; i < N; ++i)
	{
		for (int j = 0; j < N; ++j)
		{
			vec2 tc = uv + u_res_inv * vec2(float(i - M), float(j - M));

			color += coeffs[i] * coeffs[j] * texture(u_texture_half, tc).rgb;
		}
	}

	texture_full = color;
}

\upsample_half_to_full_rgba.fs

#version 330 core

const int M = 6;
const int N = 2 * M + 1;

const float coeffs[N] = float[N](
    0.002216,  // G(-6)
    0.008764,  // G(-5)
    0.026995,  // G(-4)
    0.064759,  // G(-3)
    0.120985,  // G(-2)
    0.174697,  // G(-1)
    0.199471,  // G(0)
    0.174697,  // G(1)
    0.120985,  // G(2)
    0.064759,  // G(3)
    0.026995,  // G(4)
    0.008764,  // G(5)
    0.002216   // G(6)
);

uniform sampler2D u_texture_half;

uniform vec2 u_res_inv;

layout(location = 0) out vec4 texture_full;

void main()
{
	vec2 uv = gl_FragCoord.xy * u_res_inv;

	vec4 color = vec4(0.0);

	for (int i = 0; i < N; ++i)
	{
		for (int j = 0; j < N; ++j)
		{
			vec2 tc = uv + u_res_inv * vec2(float(i - M), float(j - M));

			color += coeffs[i] * coeffs[j] * texture(u_texture_half, tc);
		}
	}

	texture_full = color;
}


\screen_space_reflections_firstpass.fs

#version 330 core

#include constants
#include lights
#include shadows
#include hdr_tonemapping

in vec2 v_uv;

uniform sampler2D u_gbuffer_depth;
uniform sampler2D u_gbuffer_normal;

uniform vec2 u_res_inv;
uniform mat4 u_inv_vp_mat;
uniform mat4 u_view_mat;
uniform mat4 u_proj_mat;
uniform mat4 u_inv_proj_mat;
uniform vec3 u_camera_position;
uniform vec3 u_bg_color;

uniform int u_raymarching_steps;
uniform float u_max_ray_len;
uniform float u_hidden_offset;

uniform sampler2D u_prev_frame;

layout(location = 0) out vec4 ssr_fbo;

void main() {
	// Typical deferred preamble to get the world position of a fragment
	vec2 uv = gl_FragCoord.xy * u_res_inv;

	float depth = texture(u_gbuffer_depth, uv).r;
	float depth_clip = depth * 2.0 - 1.0;
	
	vec2 uv_clip = uv * 2.0 - 1.0;
	vec4 clip_coords = vec4( uv_clip.x, uv_clip.y, depth_clip, 1.0);
	vec4 not_norm_world_pos = u_inv_vp_mat * clip_coords;
	vec3 world_pos = not_norm_world_pos.xyz / not_norm_world_pos.w;

	vec3 N0 = texture(u_gbuffer_normal, uv).rgb;
	vec3 N = N0 * 2.0 - 1.0;

	vec3 tmp = N0 - u_bg_color;
	tmp = tmp * tmp;
	if (tmp.x + tmp.y + tmp.z < 0.0001){
		discard;
	}

	vec3 V = normalize(u_camera_position - world_pos);

	N = normalize(N); // just in case
	// Raymarching variables
	vec3 ray_dir = reflect(-V, N); // minus because of reflect function first argument is incident
	// assume returns normalized

	float ray_step = u_max_ray_len / float(u_raymarching_steps);
	vec3 sample_pos = world_pos;

	for (int i = 0; i < u_raymarching_steps; i++) {
		// compute global position of current raymarching step
		sample_pos += ray_step * ray_dir;

		// project that global position into camera space --> [-1, 1]
		vec4 proj_sample = u_proj_mat * u_view_mat * vec4(sample_pos, 1.0);
		proj_sample /= proj_sample.w;

		vec2 sample_uv = proj_sample.xy * 0.5 + 0.5; // to uv space

		// to avoid artifacts when the reflection is just on the edge of the screen. tradeoff: we lose a reflection pixel. we avoid weird looong lines
		if (sample_uv.x < u_res_inv.x || sample_uv.x > 1.0 - u_res_inv.x || sample_uv.y < u_res_inv.y || sample_uv.y > 1.0 - u_res_inv.y) discard;

		// The depth buffer is in the range (0, 1)
		float sample_depth = texture(u_gbuffer_depth, sample_uv).r;
		vec3 reflection_color = texture(u_prev_frame, sample_uv).rgb;

		float difference = sample_depth - (proj_sample.z * 0.5 + 0.5);

		// to avoid reflecting occluded points. if the z-buffer depth and the sample depth are very different, the collision is not gucci
		if (difference < u_hidden_offset) discard;

		if (difference < 0.0) {
			ssr_fbo = vec4(reflection_color, 1.0);
			return;
		}
	}
	discard;
	//ssr_fbo = vec4(ray_dir, 1.0);
	//ssr_fbo = vec4(sample_pos, 1.0);
}