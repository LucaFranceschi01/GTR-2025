#include "volumetric.h"

#include "camera.h"
#include "scene.h"

#include "gfx/shader.h"
#include "gfx/mesh.h"


SCN::VolumetricRendering::VolumetricRendering() {
	is_active = true;
	steps = 100;
	max_ray_len = 10.f; // meters
	air_density = 0.01f;
	vertical_density_factor = 7.f;
	is_half_active = true;
}

void SCN::VolumetricRendering::create_fbo(int width, int height)
{
	instance().fbo_full.create(
		width,
		height,
		1,
		GL_RGB,
		GL_FLOAT,
		false);

	instance().fbo.create(
		width / 2,
		height / 2,
		1,
		GL_RGB,
		GL_FLOAT,
		false);
}

void SCN::VolumetricRendering::showUI()
{
	VolumetricRendering& vol = instance();

	ImGui::Checkbox("Volumetric Rendering", &vol.is_active);
	if (vol.is_active) {
		if (ImGui::TreeNode("Volumetric Rendering Settings")) {

			ImGui::SliderInt("Raymarching Steps", &vol.steps, 1, VOLUMETRIC_MAX_STEP_COUNT);
			ImGui::SliderFloat("Max. Ray Length (m)", &vol.max_ray_len, 0.5f, VOLUMETRIC_MAX_RAY_LEN);
			ImGui::SliderFloat("Air Density", &vol.air_density, 0.001f, 0.1f);
			ImGui::SliderFloat("Vertical Density Factor", &vol.vertical_density_factor, 1.f, 10.f);

			ImGui::Checkbox("Enhanced VR", &vol.is_half_active);

			ImGui::TreePop();
		}
	}
}

void SCN::VolumetricRendering::bind(GFX::Shader* shader)
{
	shader->setTexture("u_vr_texture", VolumetricRendering::instance().fbo_full.color_textures[0], 13);
	shader->setUniform("u_vr_active", (int)VolumetricRendering::instance().is_active);
}

void SCN::VolumetricRendering::compute(SCN::Scene* scene, const GFX::FBO& gbuffer_fbo, SCN::LightUniforms& light_info, Shadows& shadow_info, bool lgc_active)
{
	VolumetricRendering& vol = instance();
	{
		GFX::FBO* fbo = &vol.fbo_full;
		if (vol.is_half_active) {
			fbo = &vol.fbo;
		}

		Camera* camera = Camera::current;
		GFX::Shader* shader = GFX::Shader::Get("volumetric_rendering_compute");

		assert(glGetError() == GL_NO_ERROR);

		//no shader? then nothing to render
		if (!shader)
			return;
		shader->enable();

		// In the CPU
		fbo->bind();
		shader->enable();

		GFX::Mesh* quad = GFX::Mesh::getQuad();

		glDisable(GL_DEPTH_TEST);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		// Send the inverse of the FBO res, for the UVs
		shader->setUniform("u_res_inv", vec2(
			1.0f / fbo->color_textures[0]->width,
			1.0f / fbo->color_textures[0]->height)
		);

		shader->setTexture("u_gbuffer_depth", gbuffer_fbo.depth_texture, 11);

		shader->setUniform("u_raymarching_steps", vol.steps);
		shader->setUniform("u_max_ray_len", vol.max_ray_len);
		shader->setUniform("u_air_density", vol.air_density);
		shader->setUniform("u_vr_vertical_density_factor", vol.vertical_density_factor);

		shader->setUniform("u_inv_vp_mat", camera->inverse_viewprojection_matrix);
		shader->setUniform("u_camera_position", camera->eye);

		shader->setUniform("u_lgc_active", lgc_active);

		light_info.bind(shader);

		shader->setTexture("u_shadow_atlas", shadow_info.shadow_atlas->depth_texture, 8);
		shader->setUniform("u_shadow_atlas_dims", shadow_info.shadow_atlas_dims);

		shadow_info.bindShadowAtlasPositions(shader, light_info.shadow_lights_idxs);

		quad->render(GL_TRIANGLES);

		shader->disable();
		fbo->unbind();
	}

	if (vol.is_half_active) {
		vol.fbo_full.bind();

		glClear(GL_COLOR_BUFFER_BIT);

		Camera* camera = Camera::current;
		GFX::Mesh* quad = GFX::Mesh::getQuad();
		GFX::Shader* shader = GFX::Shader::Get("upsample_half_to_full_rgba");

		assert(glGetError() == GL_NO_ERROR);

		//no shader? then nothing to render
		if (!shader)
			return;
		shader->enable();

		// Send the inverse of the FBO res, for the UVs
		shader->setUniform("u_res_inv", vec2(
			1.0f / vol.fbo_full.color_textures[0]->width,
			1.0f / vol.fbo_full.color_textures[0]->height)
		);

		shader->setTexture("u_texture_half", vol.fbo.color_textures[0], 10);

		quad->render(GL_TRIANGLES);

		shader->disable();

		vol.fbo_full.unbind();
	}

	glEnable(GL_DEPTH_TEST);
}
