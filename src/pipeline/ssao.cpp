#include "ssao.h"

#include "camera.h"
#include "scene.h"

#include "gfx/shader.h"
#include "gfx/mesh.h"


SCN::SSAO::SSAO() {
	is_active = true;
	is_ssao_plus_active = true;
	samples = 64;
	sample_radius = 0.08;

	pointGenerator();
}

void SCN::SSAO::create_fbo(int width, int height)
{
	instance().fbo.create(
		width,
		height,
		1,
		GL_RGB,
		GL_UNSIGNED_BYTE,
		false);
}

void SCN::SSAO::showUI()
{
	SSAO& ssao = instance();

	ImGui::Checkbox("Screen Space Ambient Occlusion", &ssao.is_active);
	if (ssao.is_active) {
		ImGui::SliderFloat("Sample radius", &ssao.sample_radius, 0.01, 0.2);

		int sample_count_prev = ssao.samples;
		bool is_ssao_plus_active_prev = ssao.is_ssao_plus_active;

		ImGui::SliderInt("Sample count", &ssao.samples, 1, MAX_SSAO_SAMPLES);
		ImGui::Checkbox("SSAO+", &ssao.is_ssao_plus_active);

		if (ssao.is_ssao_plus_active != is_ssao_plus_active_prev || ssao.samples != sample_count_prev) {
			ssao.pointGenerator();
		}
	}
}

void SCN::SSAO::bind(GFX::Shader* shader)
{
	shader->setTexture("u_ssao_texture", SSAO::instance().fbo.color_textures[0], 12);
	shader->setUniform("u_ssao_active", (int)SSAO::instance().is_active);
}

void SCN::SSAO::pointGenerator()
{
	if (is_ssao_plus_active)
		ao_sample_points = generateSpherePoints(samples, 1.f, true);
	else
		ao_sample_points = generateSpherePoints(samples);
}

const std::vector<Vector3f> SCN::SSAO::generateSpherePoints(int num, float radius, bool hemi)
{
	assert(num <= MAX_SSAO_SAMPLES);
	
	std::vector<Vector3f> points;
	points.resize(num);
	for (int i = 0; i < num; i += 1) {
		Vector3f& p = points[i];
		float u = random();
		float v = random();
		float theta = u * 2.0 * PI;
		float phi = acos(2.0 * v - 1.0);
		float r = cbrt(random() * 0.9 + 0.1) * radius;
		float sinTheta = sin(theta);
		float cosTheta = cos(theta);
		float sinPhi = sin(phi);
		float cosPhi = cos(phi);
		p.x = r * sinPhi * cosTheta;
		p.y = r * sinPhi * sinTheta;
		p.z = r * cosPhi;
		if (hemi && p.z < 0)
			p.z *= -1.0;
	}
	return points;
}

void SCN::SSAO::compute(SCN::Scene* scene, const GFX::FBO& gbuffer_fbo)
{
	SSAO& ssao = instance();

	gbuffer_fbo.depth_texture->copyTo(ssao.fbo.depth_texture);

	Camera* camera = Camera::current;
	GFX::Mesh* quad = GFX::Mesh::getQuad();
	GFX::Shader* shader = GFX::Shader::Get("ssao_compute");

	assert(glGetError() == GL_NO_ERROR);

	//no shader? then nothing to render
	if (!shader)
		return;
	shader->enable();

	// send AO params
	shader->setUniform("u_sample_count", ssao.samples);
	shader->setUniform("u_sample_radius", ssao.sample_radius);
	shader->setUniform3Array("u_sample_pos", (float*)&ssao.ao_sample_points[0], ssao.samples);

	// send camera matrices
	Matrix44 inv_proj_mat = camera->projection_matrix;
	if (!inv_proj_mat.inverse())
		return;

	shader->setUniform("u_proj_mat", camera->projection_matrix);
	shader->setUniform("u_inv_proj_mat", inv_proj_mat);
	shader->setUniform("u_view_mat", camera->view_matrix);

	// In the CPU
	ssao.fbo.bind();
	shader->enable();

	// Send the inverse of the FBO res, for the UVs
	shader->setUniform("u_res_inv", vec2(
		1.0f / ssao.fbo.color_textures[0]->width,
		1.0f / ssao.fbo.color_textures[0]->height)
	);

	shader->setUniform("u_ssao_plus_active", (int)ssao.is_ssao_plus_active);
	if (ssao.is_ssao_plus_active) {
		shader->setTexture("u_gbuffer_normal", gbuffer_fbo.color_textures[1], 10);
	}

	shader->setTexture("u_gbuffer_depth", gbuffer_fbo.depth_texture, 11);

	quad->render(GL_TRIANGLES);

	shader->disable();
	ssao.fbo.unbind();
}
