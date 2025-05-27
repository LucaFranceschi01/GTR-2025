#include "ssr.h"

#include "camera.h"
#include "scene.h"

#include "gfx/shader.h"
#include "gfx/mesh.h"


SCN::ScreenSpaceReflections::ScreenSpaceReflections() {
	is_active = true;
	steps = 100;
	max_ray_len = 10.f; // meters
	hidden_offset = -0.00015;
	step_size = max_ray_len / float(steps);
}

void SCN::ScreenSpaceReflections::create_fbo(int width, int height)
{
	instance().fbo.create(
		width,
		height,
		1,
		GL_RGBA,
		GL_FLOAT,
		false);
}

void SCN::ScreenSpaceReflections::showUI()
{
	ScreenSpaceReflections& ssr = instance();

	ImGui::Checkbox("Screen Space Reflections", &ssr.is_active);
	if (ssr.is_active) {
		if (ImGui::TreeNode("SSR Settings")) {

			ImGui::SliderInt("Raymarching Steps", &ssr.steps, 1, SSR_MAX_STEP_COUNT);
			ImGui::SliderFloat("Max. Ray Length (m)", &ssr.max_ray_len, 0.5f, SSR_MAX_RAY_LEN);
			ImGui::DragFloat("Hidden surface offset", &ssr.hidden_offset, 0.00001, -0.01, -0.00001f, "%.5f");
			ImGui::DragFloat("Step size", &ssr.step_size, 0.001f, 0.00001f, float(SSR_MAX_RAY_LEN) / ssr.steps, "%.5f");

			ImGui::TreePop();
		}
	}
}

void SCN::ScreenSpaceReflections::bind(GFX::Shader* shader)
{
	shader->setTexture("u_ssr_texture", ScreenSpaceReflections::instance().fbo.color_textures[0], 14);
	shader->setUniform("u_ssr_active", (int)ScreenSpaceReflections::instance().is_active);
}

void SCN::ScreenSpaceReflections::compute_firstpass(Scene* scene, const GFX::FBO& prev_gbuffer_fbo, const GFX::FBO& prev_frame, bool lgc_active)
{
	ScreenSpaceReflections& ssr = instance();
	GFX::FBO& fbo = ssr.fbo;

	Camera* camera = Camera::current;
	GFX::Shader* shader = GFX::Shader::Get("screen_space_reflections_firstpass");

	assert(glGetError() == GL_NO_ERROR);

	//no shader? then nothing to render
	if (!shader)
		return;
	shader->enable();

	// In the CPU
	fbo.bind();
	shader->enable();

	GFX::Mesh* quad = GFX::Mesh::getQuad();

	glDisable(GL_DEPTH_TEST);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// Send the inverse of the FBO res, for the UVs
	shader->setUniform("u_res_inv", vec2(
		1.0f / fbo.color_textures[0]->width,
		1.0f / fbo.color_textures[0]->height)
	);

	shader->setTexture("u_gbuffer_normal", prev_gbuffer_fbo.color_textures[1], 10);
	shader->setTexture("u_gbuffer_depth", prev_gbuffer_fbo.depth_texture, 11);
	shader->setTexture("u_prev_frame", prev_frame.color_textures[0], 12);

	shader->setUniform("u_raymarching_steps", ssr.steps);
	shader->setUniform("u_max_ray_len", ssr.max_ray_len);
	shader->setUniform("u_hidden_offset", ssr.hidden_offset);
	shader->setUniform("u_step_size", ssr.step_size);

	// send camera matrices
	Matrix44 inv_proj_mat = camera->projection_matrix;
	if (!inv_proj_mat.inverse())
		return;

	shader->setUniform("u_inv_vp_mat", camera->inverse_viewprojection_matrix);
	shader->setUniform("u_proj_mat", camera->projection_matrix);
	shader->setUniform("u_inv_proj_mat", inv_proj_mat);
	shader->setUniform("u_view_mat", camera->view_matrix);
	shader->setUniform("u_camera_position", camera->eye);

	shader->setUniform("u_lgc_active", lgc_active);

	shader->setUniform("u_bg_color", scene->background_color);

	quad->render(GL_TRIANGLES);

	shader->disable();
	fbo.unbind();

	glEnable(GL_DEPTH_TEST);

	//fbo.color_textures[0]->toViewport();
}

void SCN::ScreenSpaceReflections::compute_secondpass(Scene* scene, const GFX::FBO& gbuffer_fbo, const GFX::FBO& lighting_fbo, bool lgc_active)
{
}
