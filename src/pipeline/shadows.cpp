#include "shadows.h"

#include <algorithm>

#include "gfx/fbo.h"
#include "gfx/shader.h"
#include "gfx/mesh.h"

#include "renderer.h"
#include "camera.h"
#include "material.h"

SCN::Shadows::Shadows()
{
	shadow_atlas = new GFX::FBO();

	// initial values, will vary depending on the lights
	shadow_atlas_dims.x = 2;
	shadow_atlas_dims.y = shadow_map_count / shadow_atlas_dims.x + 1;

	shadow_atlas->setDepthOnly(shadow_map_res * shadow_atlas_dims.x, shadow_map_res * shadow_atlas_dims.y);
}

void SCN::Shadows::updateShadowAtlasSize(int shadows_count)
{
	if (shadow_atlas->depth_texture->width != shadow_atlas_dims.x * shadow_map_res ||
		shadow_atlas->depth_texture->height != shadow_atlas_dims.y * shadow_map_res || 
		shadow_map_count != shadows_count)
	{
		delete shadow_atlas;

		shadow_map_count = shadows_count;

		shadow_atlas_dims.y = ceil(float(shadow_map_count) / float(shadow_atlas_dims.x));
		shadow_atlas_dims.y = max(shadow_atlas_dims.y, 1);

		shadow_atlas = new GFX::FBO();
		shadow_atlas->setDepthOnly(shadow_map_res * shadow_atlas_dims.x, shadow_map_res * shadow_atlas_dims.y);
	}
}

void SCN::Shadows::generateShadowMaps(std::vector<s_DrawCommand>& opaque, std::vector<s_DrawCommand>& transparent,
	LightUniforms& light_info, bool ffc)
{	
	updateShadowAtlasSize(light_info.shadow_lights_idxs.size());

	shadow_atlas->bind();

	glEnable(GL_DEPTH_TEST);
	glColorMask(false, false, false, false);

	glClear(GL_DEPTH_BUFFER_BIT);

	if (ffc == false) {
		glDisable(GL_CULL_FACE);
	}
	else {
		glEnable(GL_CULL_FACE);
		//glFrontFace(GL_CW);
	}

	for (int i = 0; i < shadow_map_count; i++) {

		int row = i / shadow_atlas_dims.x; // Integer division
		int col = i % shadow_atlas_dims.x; // Modulo

		glViewport(col * shadow_map_res, row * shadow_map_res, shadow_map_res, shadow_map_res);
		/*glScissor(col * GFX::SHADOW_RES, row * GFX::SHADOW_RES, GFX::SHADOW_RES, GFX::SHADOW_RES);
		glEnable(GL_SCISSOR_TEST);*/

		LightEntity* light = light_info.entities[light_info.shadow_lights_idxs[i]];
		Camera light_camera;

		mat4 light_model = light->root.getGlobalMatrix();

		light_camera.lookAt(light_model.getTranslation(), light_model * vec3(0.f, 0.f, -1.f), vec3(0.f, 1.f, 0.f));

		if (light->light_type == SCN::eLightType::DIRECTIONAL) {
			float half_size = light->area / 2.f;

			light_camera.setOrthographic(
				-half_size, half_size,
				-half_size, half_size,
				light->near_distance, light->max_distance
			);
		}
		else if (light->light_type == SCN::eLightType::SPOT) {
			light_camera.setPerspective(light->cone_info.y * 2.f, 1.f, light->near_distance, light->max_distance);
		}
		else {
			continue;
		}

		light_info.viewprojections[light_info.shadow_lights_idxs[i]] = light_camera.viewprojection_matrix;

		for (s_DrawCommand command : opaque) {
			renderPlain(&light_camera, command.model, command.mesh, command.material);
		}

		for (s_DrawCommand command : transparent) {
			renderPlain(&light_camera, command.model, command.mesh, command.material);
		}

		//glDisable(GL_SCISSOR_TEST);
	}
	// Disable the default front face culling
	glDisable(GL_CULL_FACE);

	glDisable(GL_DEPTH_TEST);
	glColorMask(true, true, true, true);

	shadow_atlas->unbind();
}

void SCN::Shadows::renderPlain(Camera* light_camera, const Matrix44 model, GFX::Mesh* mesh, SCN::Material* material)
{
	//in case there is nothing to do
	if (!mesh || !mesh->getNumVertices() || !material)
		return;
	assert(glGetError() == GL_NO_ERROR);

	//define locals to simplify coding
	GFX::Shader* shader = GFX::Shader::Get("plain");

	//glDisable(GL_BLEND);
	glEnable(GL_DEPTH_TEST);

	//no shader? then nothing to render
	if (!shader)
		return;
	shader->enable();

	//material->bind(shader);
	int flags[SCN::eTextureChannel::ALL] = { 0 };
	GFX::Texture* texture = material->textures[SCN::eTextureChannel::ALBEDO].texture;

	shader->setUniform("u_color", material->color);

	if (texture) {
		shader->setUniform("u_texture", texture, 0);
		flags[SCN::eTextureChannel::ALBEDO] = 1;
	}
	else {
		texture = GFX::Texture::getWhiteTexture(); //a 1x1 white texture
	}

	shader->setUniform("u_alpha_cutoff", material->alpha_mode == SCN::eAlphaMode::MASK ? material->alpha_cutoff : 0.001f);

	shader->setUniform1Array("u_maps", (int*)flags, SCN::eTextureChannel::ALL);

	//upload uniforms
	shader->setUniform("u_model", model);

	// Upload camera uniforms
	shader->setUniform("u_viewprojection", light_camera->viewprojection_matrix);
	shader->setUniform("u_camera_position", light_camera->eye);

	mesh->render(GL_TRIANGLES);

	//disable shader
	shader->disable();
}

void SCN::Shadows::bindShadowAtlasPosition(GFX::Shader* shader, std::vector<int>& shadow_indices, int light_index)
{
	auto result = std::find(shadow_indices.begin(), shadow_indices.end(), light_index);

	if (result != shadow_indices.end()) {
		int shadow_atlas_idx = result - shadow_indices.begin();

		shader->setUniform("u_shadow_atlas_offset",
			Vector2<int>(shadow_atlas_idx % shadow_atlas_dims.x,
				shadow_atlas_idx / shadow_atlas_dims.x)
		);
	}
}

void SCN::Shadows::bindShadowAtlasPositions(GFX::Shader* shader, std::vector<int>& shadow_indices)
{
	int atlas_indices[MAX_LIGHTS] = { 0 };
	for (int i = 0; i < MAX_LIGHTS; i++) {
		auto result = std::find(shadow_indices.begin(), shadow_indices.end(), i);

		if (result != shadow_indices.end()) {
			atlas_indices[i] = result - shadow_indices.begin();
		}
	}
	shader->setUniform1Array("u_atlas_indices", atlas_indices, MAX_LIGHTS);
}

void SCN::Shadows::showUI(Shadows& shadow_info)
{
	if (ImGui::TreeNode("Shadowmaps")) {
		ImGui::Text("ShadowMap columns");
		ImGui::SliderInt("##shadowmapcolumns", &shadow_info.shadow_atlas_dims.x, 1, 5);

		int exponent = static_cast<int>(std::log2(shadow_info.shadow_map_res));
		ImGui::Text("ShadowMap resolution exponent");
		if (ImGui::SliderInt("##shadowmapresolutionexponent", &exponent, 7, 11)) {
			shadow_info.shadow_map_res = 1 << exponent; // expression extracted from ChatGPT
		}
		ImGui::Text("ShadowMap Resolution: %dx%d", shadow_info.shadow_map_res, shadow_info.shadow_map_res);

		ImGui::TreePop();
	}
}
