#pragma once

#include "scene.h"

constexpr auto MAX_LIGHTS = 5;

namespace GFX {
	class Shader;
}

namespace SCN {

	enum eLightType : uint32 {
		NO_LIGHT = 0,
		POINT = 1,
		SPOT = 2,
		DIRECTIONAL = 3
	};

	class LightEntity : public BaseEntity
	{
	public:

		eLightType light_type;
		float intensity;
		vec3 color;
		float near_distance;
		float max_distance;
		bool cast_shadows;
		float shadow_bias;
		vec2 cone_info;
		float area; //for direct;

		ENTITY_METHODS(LightEntity, LIGHT, 14,4);

		LightEntity();

		void configure(cJSON* json);
		void serialize(cJSON* json);
	};

	class LightUniforms // Maybe not the best name for this class
	{
	public:
		Vector3f ambient_light = { 0.2f };

		// Amount of visible lights
		uint8_t l_count = 0;

		// Typical light uniforms
		float intensities[MAX_LIGHTS];
		float types[MAX_LIGHTS];
		vec3 colors[MAX_LIGHTS];
		vec3 positions[MAX_LIGHTS];
		vec3 directions[MAX_LIGHTS];
		vec2 cone_info[MAX_LIGHTS];
		
		// Used for shadows
		LightEntity* entities[MAX_LIGHTS];
		Matrix44 viewprojections[MAX_LIGHTS];
		int cast_shadows[MAX_LIGHTS];
		float shadow_biases[MAX_LIGHTS];
		std::vector<int> shadow_lights_idxs;

		void bind(GFX::Shader* shader) const;
		void bind_single(GFX::Shader* shader, int i) const;
		void add_light(LightEntity* light);

		void clear();
	};
};
