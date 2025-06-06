#include "renderer.h"

#include <algorithm> // sort

#include "camera.h"
#include "../gfx/gfx.h"
#include "../gfx/shader.h"
#include "../gfx/mesh.h"
#include "../gfx/texture.h"
#include "../gfx/fbo.h"
#include "../pipeline/prefab.h"
#include "../pipeline/material.h"
#include "../pipeline/animation.h"
#include "../utils/utils.h"
#include "../extra/hdre.h"
#include "../core/ui.h"

#include "scene.h"
#include "ssao.h"
#include "volumetric.h"
#include "ssr.h"

using namespace SCN;

//some globals
GFX::Mesh sphere;

Renderer::Renderer(const char* shader_atlas_filename)
{
	render_wireframe = false;
	render_boundaries = false;
	scene = nullptr;
	skybox_cubemap = nullptr;

	if (!GFX::Shader::LoadAtlas(shader_atlas_filename))
		exit(1);
	GFX::checkGLErrors();

	sphere.createSphere(1.0f);
	sphere.uploadToVRAM();

	Vector2ui win_size = CORE::getWindowSize();

	gbuffer_fbo.create(win_size.x,
		win_size.y,
		2, // Create two textures to render to
		GL_RGBA, // Each texture has an R G B and A channels
		GL_UNSIGNED_BYTE, // Uses 8 bits per channel
		true); // Stores the depth, to a texture

	lighting_fbo.create(win_size.x,
		win_size.y,
		1,
		GL_RGBA,
		GL_FLOAT,
		true);

	final_frame.create(win_size.x,
		win_size.y,
		1,
		GL_RGBA,
		GL_FLOAT,
		true);

	sphere.createSphere(1.0);

	SSAO::create_fbo(win_size.x, win_size.y);

	VolumetricRendering::create_fbo(win_size.x, win_size.y);

	ScreenSpaceReflections::create_fbo(win_size.x, win_size.y);
}

void Renderer::setupScene()
{
	light_info.ambient_light = scene->ambient_light;
	
	if (scene->skybox_filename.size())
		skybox_cubemap = GFX::Texture::Get(std::string(scene->base_folder + "/" + scene->skybox_filename).c_str());
	else
		skybox_cubemap = nullptr;
}

void Renderer::parseNodes(SCN::Node* node, Camera* cam)
{
	if (!node) return;

	// parse all nodes including children
	for (SCN::Node* child : node->children) {
		parseNodes(child, cam);
	}

	if (!node->mesh) return;

	// start frustum culling
	if (frustum_culling) {
		bool in_frustum = cam->testBoxInFrustum(node->aabb.center, node->aabb.halfsize); // note: aabb is the bounding box

		if (!in_frustum) return;
	}
	// end frustum culling

	// since we will draw it for sure we create the renderable
	s_DrawCommand draw_command{
			node->getGlobalMatrix(),
			node->mesh,
			node->material
	};

	// start transparencies
	if (node->isTransparent()) {
		draw_commands_transp.push_back(draw_command);
	}
	else {
		draw_commands_opaque.push_back(draw_command);
	}
	// end transparencies
}

void SCN::Renderer::fillGBuffer()
{
	gbuffer_fbo.bind();
	// Clear the FBO from the prev frame
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	for (s_DrawCommand& command : draw_commands_opaque) {
		renderMeshWithMaterial(command.model, command.mesh, command.material);
	}

	for (s_DrawCommand& command : draw_commands_transp) {
		renderMeshWithMaterial(command.model, command.mesh, command.material);
	}

	gbuffer_fbo.unbind();
}

void SCN::Renderer::fillLightingFBOMultipass(SCN::Scene* scene, Camera* camera)
{
	// ================================================= FIRST PASS
	GFX::Mesh* quad = GFX::Mesh::getQuad();

	GFX::Shader* shader = GFX::Shader::Get("multipass_phong_deferred_firstpass");
		
	if (!shader)
		return;
	shader->enable();

	light_info.bind(shader);

	shadow_info.bindShadowAtlasPositions(shader, light_info.shadow_lights_idxs);

	shader->setTexture("u_gbuffer_color", gbuffer_fbo.color_textures[0], 9);
	shader->setTexture("u_gbuffer_normal", gbuffer_fbo.color_textures[1], 10);
	shader->setTexture("u_gbuffer_depth", gbuffer_fbo.depth_texture, 11);

	shader->setUniform("u_res_inv",
		vec2(1.0f / CORE::BaseApplication::instance->window_width,
			1.0f / CORE::BaseApplication::instance->window_height
		)
	);

	shader->setUniform("u_camera_position", camera->eye);
	shader->setUniform("u_inv_vp_mat", camera->inverse_viewprojection_matrix);
	shader->setUniform("u_shininess", shininess);

	shader->setTexture("u_shadow_atlas", shadow_info.shadow_atlas->depth_texture, 8);
	shader->setUniform("u_shadow_atlas_dims", shadow_info.shadow_atlas_dims);

	shader->setUniform("u_bg_color", scene->background_color);

	shader->setUniform("u_lgc_active", (int)linear_gamma_correction);
	
	SSAO::bind(shader);

	ScreenSpaceReflections::bind(shader);

	quad->render(GL_TRIANGLES);

	shader->disable();
		
	// ================================================= LIGHT PASSES
		
	// Set the OpenGL config
	glDepthFunc(GL_GREATER);
	glDepthMask(GL_FALSE);
	glBlendFunc(GL_ONE, GL_ONE);
	glFrontFace(GL_CW);
	glEnable(GL_BLEND);
		
	shader = GFX::Shader::Get("multipass_phong_deferred");

	//no shader? then nothing to render
	if (!shader)
		return;
	shader->enable();

	light_info.bind(shader);

	shadow_info.bindShadowAtlasPositions(shader, light_info.shadow_lights_idxs);

	// Bind the GBuffers
	shader->setTexture("u_gbuffer_color", gbuffer_fbo.color_textures[0], 9);
	shader->setTexture("u_gbuffer_normal", gbuffer_fbo.color_textures[1], 10);
	shader->setTexture("u_gbuffer_depth", gbuffer_fbo.depth_texture, 11);

	shader->setUniform("u_res_inv",
		vec2(1.0f / CORE::BaseApplication::instance->window_width,
			1.0f / CORE::BaseApplication::instance->window_height
		)
	);

	shader->setUniform("u_viewprojection", camera->viewprojection_matrix);
	shader->setUniform("u_camera_position", camera->eye);
	shader->setUniform("u_inv_vp_mat", camera->inverse_viewprojection_matrix);
	shader->setUniform("u_shininess", shininess);

	shader->setTexture("u_shadow_atlas", shadow_info.shadow_atlas->depth_texture, 8);
	shader->setUniform("u_shadow_atlas_dims", shadow_info.shadow_atlas_dims);

	shader->setUniform("u_bg_color", scene->background_color);

	shader->setUniform("u_lgc_active", (int)linear_gamma_correction);

	ScreenSpaceReflections::bind(shader);

	vec3 pos;
	float md;
	mat4 model;
		
	for (int i = 0; i < light_info.l_count; i++) {
		LightEntity* light = light_info.entities[i];

		if (light->light_type == eLightType::DIRECTIONAL) continue;

		// Retrieve light data
		pos = light_info.positions[i];
		md = light->max_distance;

		// Create the model from the light data.
		model.setTranslation(pos.x, pos.y, pos.z);
		model.scale(md, md, md);

		// Upload the necessary uniforms.
		shader->setUniform("u_light_id", i);
		shader->setUniform("u_model", model);

		sphere.render(GL_TRIANGLES);
	}

	shader->disable();	

	// Return the OpenGL config to what it was
	glDepthFunc(GL_LESS);
	glDepthMask(GL_TRUE);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDisable(GL_BLEND);
	glFrontFace(GL_CCW);
}

void SCN::Renderer::fillLightingFBOSinglepass(SCN::Scene* scene, Camera* camera)
{	
	GFX::Mesh* quad = GFX::Mesh::getQuad();
	GFX::Shader* shader;

	if (reflectance_model == PHONG) {
		shader = GFX::Shader::Get("singlepass_phong_deferred");
	}
	else {
		shader = GFX::Shader::Get("singlepass_pbr_deferred");
	}

	assert(glGetError() == GL_NO_ERROR);

	//no shader? then nothing to render
	if (!shader)
		return;
	shader->enable();

	light_info.bind(shader);

	shadow_info.bindShadowAtlasPositions(shader, light_info.shadow_lights_idxs);

	// Bind the GBuffers
	shader->setTexture("u_gbuffer_color", gbuffer_fbo.color_textures[0], 9);
	shader->setTexture("u_gbuffer_normal", gbuffer_fbo.color_textures[1], 10);
	shader->setTexture("u_gbuffer_depth", gbuffer_fbo.depth_texture, 11);

	shader->setUniform("u_res_inv",
		vec2(1.0f / CORE::BaseApplication::instance->window_width,
				1.0f / CORE::BaseApplication::instance->window_height
		)
	);

	shader->setUniform("u_camera_position", camera->eye);
	shader->setUniform("u_inv_vp_mat", camera->inverse_viewprojection_matrix);
	if (reflectance_model == PHONG) shader->setUniform("u_shininess", shininess);

	shader->setTexture("u_shadow_atlas", shadow_info.shadow_atlas->depth_texture, 8);
	shader->setUniform("u_shadow_atlas_dims", shadow_info.shadow_atlas_dims);

	shader->setUniform("u_bg_color", scene->background_color);

	shader->setUniform("u_lgc_active", (int)linear_gamma_correction);

	SSAO::bind(shader);

	ScreenSpaceReflections::bind(shader);
		
	quad->render(GL_TRIANGLES);
	
	shader->disable();
}

void SCN::Renderer::displayScene(SCN::Scene* scene)
{
	final_frame.bind();
	
	GFX::Mesh* quad = GFX::Mesh::getQuad();

	GFX::Shader* shader;
	
	if (tonemapper.active) {
		shader = GFX::Shader::Get("deferred_tonemapper_to_viewport");
	}
	else {
		shader = GFX::Shader::Get("deferred_to_viewport");
	}

	assert(glGetError() == GL_NO_ERROR);


	glClear(GL_COLOR_BUFFER_BIT);

	//set the clear color (the background color)
	glClearColor(scene->background_color.x, scene->background_color.y, scene->background_color.z, 1.0);

	//no shader? then nothing to render
	if (!shader)
		return;
	shader->enable();

	shader->setTexture("u_texture", lighting_fbo.color_textures[0], 12);
	shader->setUniform("u_bg_color", scene->background_color);

	shader->setUniform("u_lgc_active", (int)linear_gamma_correction);

	shader->setUniform("u_scale", tonemapper.scale);
	shader->setUniform("u_average_lum", tonemapper.average_lum);
	shader->setUniform("u_lumwhite2", tonemapper.lumwhite2);
	shader->setUniform("u_igamma", tonemapper.igamma);

	VolumetricRendering::bind(shader);

	quad->render(GL_TRIANGLES);

	shader->disable();

	final_frame.unbind();

	final_frame.color_textures[0]->toViewport();
}

void Renderer::parseSceneEntities(SCN::Scene* scene, Camera* cam)
{
	// important to clear the list in each pass
	draw_commands_opaque.clear();
	draw_commands_transp.clear();
	
	light_info.clear();

	for (int i = 0; i < scene->entities.size(); i++) {
		BaseEntity* entity = scene->entities[i];

		if (!entity->visible) {
			continue;
		}

		switch (entity->getType())
		{
		case eEntityType::PREFAB:
		{
			// once we know it is a PREFAB entity perform static cast
			PrefabEntity* prefab_entity = static_cast<PrefabEntity*>(entity);

			// parse all nodes (including children)
			parseNodes(&prefab_entity->root, cam);
			break;
		}
		case eEntityType::LIGHT:
		{
			// Store Lights
			LightEntity* light = static_cast<LightEntity*>(entity);

			// add light to our structure
			light_info.add_light(light);
			break;
		}
		default:
			break;
		}
	}

	// camera eye is used to sort both opaque and transparent entities
	Vector3f ce = cam->eye;

	// sort opaque entities in ascending order (front to back)
	std::sort(draw_commands_opaque.begin(), draw_commands_opaque.end(), [&ce](s_DrawCommand& a, s_DrawCommand& b) {
		return a.model.getTranslation().distance(ce) < b.model.getTranslation().distance(ce);
		});

	// sort transparent entities in descending order (back to front)
	std::sort(draw_commands_transp.begin(), draw_commands_transp.end(), [&ce](s_DrawCommand& a, s_DrawCommand& b) {
		return a.model.getTranslation().distance(ce) > b.model.getTranslation().distance(ce);
		});
}

void Renderer::renderScene(SCN::Scene* scene, Camera* camera)
{
	this->scene = scene;
	setupScene();

	parseSceneEntities(scene, camera);
	
	shadow_info.generateShadowMaps(draw_commands_opaque, draw_commands_transp, light_info, front_face_culling_on);

	//set the clear color (the background color)
	glClearColor(scene->background_color.x, scene->background_color.y, scene->background_color.z, 1.0);

	// Clear the color and the depth buffer
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	GFX::checkGLErrors();

	//render skybox
	if (skybox_cubemap)
		renderSkybox(skybox_cubemap);

	if (pipeline_mode == FORWARD)
		renderSceneForward(scene, camera);
	else if (pipeline_mode == DEFERRED)
		renderSceneDeferred(scene, camera);
	else
		return;
}

void SCN::Renderer::renderSceneForward(SCN::Scene* scene, Camera* camera)
{
	// first render opaque entities
	for (s_DrawCommand& command : draw_commands_opaque) {
		renderMeshWithMaterial(command.model, command.mesh, command.material);
	}

	// then render transparent entities
	for (s_DrawCommand& command : draw_commands_transp) {
		renderMeshWithMaterial(command.model, command.mesh, command.material);
	}
}

void SCN::Renderer::renderSceneDeferred(SCN::Scene* scene, Camera* camera)
{
	// compute SSR first pass before gbuffer is overwritten in current iteration (unless first iteration)
	ScreenSpaceReflections::fill(scene, gbuffer_fbo, final_frame, linear_gamma_correction);

	fillGBuffer();

	SSAO::compute(scene, gbuffer_fbo);
	
	VolumetricRendering::compute(scene, gbuffer_fbo, light_info, shadow_info, linear_gamma_correction);

	gbuffer_fbo.depth_texture->copyTo(lighting_fbo.depth_texture);

	lighting_fbo.bind();

	glClear(GL_COLOR_BUFFER_BIT);

	//set the clear color (the background color)
	glClearColor(scene->background_color.x, scene->background_color.y, scene->background_color.z, 1.0);

	assert(glGetError() == GL_NO_ERROR);

	if (pass_setting == SINGLEPASS) {
		fillLightingFBOSinglepass(scene, camera); // directly illumination to screen
	}
	else {
		fillLightingFBOMultipass(scene, camera); // to FBO, then to screen
	}

	lighting_fbo.unbind();

	displayScene(scene);
}


void Renderer::renderSkybox(GFX::Texture* cubemap)
{
	Camera* camera = Camera::current;

	// Apply skybox necesarry config:
	// No blending, no dpeth test, we are always rendering the skybox
	// Set the culling aproppiately, since we just want the back faces
	glDisable(GL_BLEND);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);

	if (render_wireframe)
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

	GFX::Shader* shader = GFX::Shader::Get("skybox");
	if (!shader)
		return;
	shader->enable();

	// Center the skybox at the camera, with a big sphere
	Matrix44 m;
	m.setTranslation(camera->eye.x, camera->eye.y, camera->eye.z);
	m.scale(10, 10, 10);
	shader->setUniform("u_model", m);

	// Upload camera uniforms
	shader->setUniform("u_viewprojection", camera->viewprojection_matrix);
	shader->setUniform("u_camera_position", camera->eye);

	shader->setTexture("u_texture", cubemap, 0);

	sphere.render(GL_TRIANGLES);

	shader->disable();

	// Return opengl state to default
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	glEnable(GL_DEPTH_TEST);
}

// Renders a mesh given its transform and material
void Renderer::renderMeshWithMaterial(const Matrix44 model, GFX::Mesh* mesh, SCN::Material* material)
{
	//in case there is nothing to do
	if (!mesh || !mesh->getNumVertices() || !material )
		return;
	assert(glGetError() == GL_NO_ERROR);

	glEnable(GL_DEPTH_TEST);

	if (pipeline_mode == FORWARD) {
		renderMeshWithMaterialForward(model, mesh, material);
	}
	else if (pipeline_mode == DEFERRED) {
		renderMeshWithMaterialDeferred(model, mesh, material);
	}
	else {
		return;
	}

	// Render just the vertices as a wireframe
	if (render_wireframe)
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

	GFX::Shader::current->disable();

	//set the render state as it was before to avoid problems with future renders
	glDisable(GL_BLEND);
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	glDepthFunc(GL_LESS);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void SCN::Renderer::renderMeshWithMaterialDeferred(const Matrix44 model, GFX::Mesh* mesh, SCN::Material* material)
{
	Camera* camera = Camera::current;
	GFX::Shader* shader = GFX::Shader::Get("fill_gbuffer");

	assert(glGetError() == GL_NO_ERROR);

	//no shader? then nothing to render
	if (!shader)
		return;
	shader->enable();

	material->bind(shader);

	//upload uniforms
	shader->setUniform("u_model", model);

	// Upload camera uniforms
	shader->setUniform("u_viewprojection", camera->viewprojection_matrix);
	shader->setUniform("u_camera_position", camera->eye);

	// Upload time, for cool shader effects
	float t = getTime();
	shader->setUniform("u_time", t);

	shader->setTexture("u_shadow_atlas", shadow_info.shadow_atlas->depth_texture, 8);
	shader->setUniform("u_shadow_atlas_dims", shadow_info.shadow_atlas_dims);

	shader->setUniform("u_lgc_active", (int)linear_gamma_correction);

	if (pass_setting == SINGLEPASS) {
		//do the draw call that renders the mesh into the screen
		mesh->render(GL_TRIANGLES);
	}
	else if (pass_setting == MULTIPASS) {
		for (int i = 0; i < light_info.l_count; i++) {
			mesh->render(GL_TRIANGLES);
		}
	}
}

void SCN::Renderer::renderMeshWithMaterialForward(const Matrix44 model, GFX::Mesh* mesh, SCN::Material* material)
{
	Camera* camera = Camera::current;
	GFX::Shader* shader;
	
	if (pass_setting == SINGLEPASS && reflectance_model == PHONG) {
		shader = GFX::Shader::Get("singlepass_phong_forward");
	}
	else if (pass_setting == MULTIPASS && reflectance_model == PHONG) {
		shader = GFX::Shader::Get("multipass_phong_forward");
		glDepthFunc(GL_LEQUAL);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE);
	}
	else if (pass_setting == SINGLEPASS && reflectance_model == PBR) {
		shader = GFX::Shader::Get("singlepass_pbr_forward");
	}
	else {
		return;
	}

	assert(glGetError() == GL_NO_ERROR);

	//no shader? then nothing to render
	if (!shader)
		return;
	shader->enable();

	material->bind(shader);

	//upload uniforms
	shader->setUniform("u_model", model);

	// Upload camera uniforms
	shader->setUniform("u_viewprojection", camera->viewprojection_matrix);
	shader->setUniform("u_camera_position", camera->eye);

	// Upload time, for cool shader effects
	float t = getTime();
	shader->setUniform("u_time", t);

	shader->setTexture("u_shadow_atlas", shadow_info.shadow_atlas->depth_texture, 8);
	shader->setUniform("u_shadow_atlas_dims", shadow_info.shadow_atlas_dims);

	if (reflectance_model == PHONG) shader->setUniform("u_shininess", shininess);

	shader->setUniform("u_lgc_active", (int)linear_gamma_correction);

	if (pass_setting == SINGLEPASS) {
		// Upload all uniforms related to lighting
		light_info.bind(shader);

		shadow_info.bindShadowAtlasPositions(shader, light_info.shadow_lights_idxs);

		//do the draw call that renders the mesh into the screen
		mesh->render(GL_TRIANGLES);
	}
	else {
		for (int i = 0; i < light_info.l_count; i++) {
			if (i == 0) {
				glDisable(GL_BLEND);
			}
			else {
				glEnable(GL_BLEND);
			}

			light_info.bind_single(shader, i);

			shadow_info.bindShadowAtlasPosition(shader, light_info.shadow_lights_idxs, i);

			mesh->render(GL_TRIANGLES);
		}
	}
}

#ifndef SKIP_IMGUI

void Renderer::showUI()
{
		
	ImGui::Checkbox("Wireframe", &render_wireframe);
	ImGui::Checkbox("Boundaries", &render_boundaries);

	ImGui::Separator();

	// PIPELINE SETTINGS

	ImGui::Combo("Pipeline mode", (int*)&pipeline_mode, "FORWARD\0DEFERRED\0FORWARD_PLUS", COUNT_PIPELINEMODE);
	ImGui::Combo("Pass setting", (int*)&pass_setting, "SINGLEPASS\0MULTIPASS\0", COUNT_PASSSETTING);
	ImGui::Combo("Reflectance model", (int*)&reflectance_model, "PHONG\0PBR\0", COUNT_REFLECTANCEMODEL);
	
	if (reflectance_model == PHONG) {
		ImGui::SliderFloat("Phong Shininess", &shininess, 20.f, 80.f);
	}

	ImGui::Separator();

	// CULLING SETTINGS

	ImGui::Checkbox("Frustum Culling", &frustum_culling);
	ImGui::Checkbox("Front Face Culling", &front_face_culling_on);

	SSAO::showUI();

	ImGui::Checkbox("Linear / Gamma correction", &linear_gamma_correction);
	ImGui::Checkbox("Tonemapper", &tonemapper.active);
	if (tonemapper.active) {
		if (ImGui::TreeNode("Tonemapper settings")) {
			ImGui::SliderFloat("scale", &tonemapper.scale, 0.5f, 3.f);
			ImGui::SliderFloat("average_lum", &tonemapper.average_lum, 0.5f, 3.f);
			ImGui::SliderFloat("lumwhite2", &tonemapper.lumwhite2, 0.5f, 3.f);
			ImGui::SliderFloat("igamma", &tonemapper.igamma, 0.5f, 3.f);

			ImGui::TreePop();
		}
	}

	VolumetricRendering::showUI();

	ImGui::Separator();

	Shadows::showUI(shadow_info);

	ScreenSpaceReflections::showUI();
}

#else
void Renderer::showUI() {}
#endif