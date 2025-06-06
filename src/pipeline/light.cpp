#include "light.h"

#include "../core/ui.h"
#include "../utils/utils.h"

#include "gfx/shader.h"
#include "renderer.h"

SCN::LightEntity::LightEntity()
{
	light_type = eLightType::POINT;
	color.set(1, 1, 1);
	cone_info.set(25, 40);
	intensity = 1;
	max_distance = 100;
	cast_shadows = false;
	shadow_bias = 0.001;
	near_distance = 0.1;
	area = 1000;
}

void SCN::LightEntity::configure(cJSON* json)
{
	color = readJSONVector3(json, "color", color );
	intensity = readJSONNumber(json, "intensity", intensity);
	max_distance = readJSONNumber(json, "max_dist", max_distance);
	cast_shadows = readJSONBool(json, "cast_shadows", cast_shadows);

	cone_info.x = readJSONNumber(json, "cone_start", cone_info.x );
	cone_info.y = readJSONNumber(json, "cone_end", cone_info.y );
	area = readJSONNumber(json, "area", area);
	near_distance = readJSONNumber(json, "near_dist", near_distance);

	std::string light_type_str = readJSONString(json, "light_type", "");
	if (light_type_str == "POINT")
		light_type = eLightType::POINT;
	if (light_type_str == "SPOT")
		light_type = eLightType::SPOT;
	if (light_type_str == "DIRECTIONAL")
		light_type = eLightType::DIRECTIONAL;
}

void SCN::LightEntity::serialize(cJSON* json)
{
	writeJSONVector3(json, "color", color);
	writeJSONNumber(json, "intensity", intensity);
	writeJSONNumber(json, "max_dist", max_distance);
	writeJSONBool(json, "cast_shadows", cast_shadows);
	writeJSONNumber(json, "near_dist", near_distance);

	if (light_type == eLightType::SPOT)
	{
		writeJSONNumber(json, "cone_start", cone_info.x);
		writeJSONNumber(json, "cone_end", cone_info.y);
	}
	if (light_type == eLightType::DIRECTIONAL)
		writeJSONNumber(json, "area", area);

	if (light_type == eLightType::POINT)
		writeJSONString(json, "light_type", "POINT");
	if (light_type == eLightType::SPOT)
		writeJSONString(json, "light_type", "SPOT");
	if (light_type == eLightType::DIRECTIONAL)
		writeJSONString(json, "light_type", "DIRECTIONAL");
}

void SCN::LightUniforms::bind(GFX::Shader* shader) const
{
	// upload light-related uniforms
	shader->setUniform("u_ambient_light", ambient_light);
	shader->setUniform("u_light_count", l_count);

	shader->setUniform1Array("u_light_intensities", intensities, MAX_LIGHTS);
	shader->setUniform1Array("u_light_types", types, MAX_LIGHTS);
	shader->setUniform3Array("u_light_positions", (float*)positions, MAX_LIGHTS); // vector
	shader->setUniform3Array("u_light_colors", (float*)colors, MAX_LIGHTS);

	// for directional lights
	shader->setUniform3Array("u_light_directions", (float*)directions, MAX_LIGHTS);

	// for spotlights
	shader->setUniform2Array("u_light_cones", (float*)cone_info, MAX_LIGHTS);

	shader->setMatrix44Array("u_shadowmap_viewprojections", (Matrix44*)viewprojections, MAX_LIGHTS);
	shader->setUniform1Array("u_light_cast_shadowss", cast_shadows, MAX_LIGHTS);
	shader->setUniform1Array("u_shadowmap_biases", shadow_biases, MAX_LIGHTS);
}

void SCN::LightUniforms::bind_single(GFX::Shader* shader, int i) const
{
	// upload light-related uniforms
	if (i == 0) {
		shader->setUniform("u_ambient_light", ambient_light);
	}
	else {
		shader->setUniform("u_ambient_light", Vector3f(0.f));
	}

	shader->setUniform("u_light_intensity", intensities[i]);
	shader->setUniform("u_light_type", types[i]);
	shader->setUniform("u_light_position", positions[i]); // vector
	shader->setUniform("u_light_color", colors[i]);

	// for directional lights
	shader->setUniform("u_light_direction", directions[i]);

	// for spotlights
	shader->setUniform("u_light_cone", cone_info[i]);

	shader->setUniform("u_light_cast_shadows", cast_shadows[i]);
	shader->setUniform("u_shadowmap_viewprojection", viewprojections[i]);
	shader->setUniform("u_shadowmap_bias", entities[i]->shadow_bias);
}

void SCN::LightUniforms::add_light(LightEntity* light)
{
	Matrix44 gm = light->root.getGlobalMatrix();

	uint8_t& i = l_count;

	// for all types of light
	intensities[i] = light->intensity;
	types[i] = light->light_type;
	colors[i] = light->color;
	positions[i] = gm.getTranslation();

	// for directional + spot lights
	directions[i] = gm.frontVector();

	// for spotlights
	cone_info[i] = light->cone_info * DEG2RAD;

	// for shadowmap purposes
	entities[i] = light;
	cast_shadows[i] = static_cast<int>(light->cast_shadows);
	shadow_biases[i] = light->shadow_bias;

	i++; // increase counter
	
	if (types[i-1] == eLightType::POINT || !cast_shadows[i-1])
		return;

	shadow_lights_idxs.push_back(i - 1);
}

void SCN::LightUniforms::clear()
{
	l_count = 0;

	shadow_lights_idxs.clear();
}
