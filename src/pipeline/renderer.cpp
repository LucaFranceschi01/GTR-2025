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

	gbuffer_fbo.create(CORE::BaseApplication::instance->window_width,
		CORE::BaseApplication::instance->window_height,
		2, // Create two textures to render to
		GL_RGBA, // Each texture has an R G B and A channels
		GL_UNSIGNED_BYTE, // Uses 8 bits per channel
		true); // Stores the depth, to a texture
}

void Renderer::setupScene()
{
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
	bool in_frustum = cam->testBoxInFrustum(node->aabb.center, node->aabb.halfsize); // note: aabb is the bounding box

	if (!in_frustum) return;
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
		renderMeshWithMaterial(command.model, command.mesh, command.material, true);
	}

	for (s_DrawCommand& command : draw_commands_transp) {
		renderMeshWithMaterial(command.model, command.mesh, command.material, true);
	}

	gbuffer_fbo.unbind();
}

void SCN::Renderer::displayScene(Camera* camera)
{
	GFX::Mesh* quad = GFX::Mesh::getQuad();

	GFX::Shader* shader = GFX::Shader::Get("singlepass_phong_deferred");

	assert(glGetError() == GL_NO_ERROR);

	//no shader? then nothing to render
	if (!shader)
		return;
	shader->enable();

	if (singlepass_on) {
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

		shader->setUniform("u_camera_position", camera->viewprojection_matrix.getTranslation());
		shader->setUniform("u_inv_vp_mat", camera->inverse_viewprojection_matrix);
		shader->setUniform("u_shininess", shininess);

		shader->setUniform("u_shadow_atlas", shadow_info.shadow_atlas->depth_texture, 8);
		shader->setUniform("u_shadow_atlas_dims", shadow_info.shadow_atlas_dims);
		
		quad->render(GL_TRIANGLES);
	}
	shader->disable();
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
	if(skybox_cubemap)
		renderSkybox(skybox_cubemap);

	if (deferred_on) {
		fillGBuffer();

		displayScene(camera);
	}
	else {
		// first render opaque entities
		for (s_DrawCommand& command : draw_commands_opaque) {
			renderMeshWithMaterial(command.model, command.mesh, command.material);
		}

		// then render transparent entities
		for (s_DrawCommand& command : draw_commands_transp) {
			renderMeshWithMaterial(command.model, command.mesh, command.material);
		}
	}
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

	shader->setUniform("u_texture", cubemap, 0);

	sphere.render(GL_TRIANGLES);

	shader->disable();

	// Return opengl state to default
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	glEnable(GL_DEPTH_TEST);
}

// Renders a mesh given its transform and material
void Renderer::renderMeshWithMaterial(const Matrix44 model, GFX::Mesh* mesh, SCN::Material* material, bool gbuffer_pass)
{
	//in case there is nothing to do
	if (!mesh || !mesh->getNumVertices() || !material )
		return;
	assert(glGetError() == GL_NO_ERROR);

	//define locals to simplify coding
	GFX::Shader* shader = NULL;
	Camera* camera = Camera::current;

	glEnable(GL_DEPTH_TEST);

	//chose a shader
	if (gbuffer_pass) {
		shader = GFX::Shader::Get("texture_deferred");
	}
	else if (singlepass_on) {
		shader = GFX::Shader::Get("singlepass");
	}
	else {
		shader = GFX::Shader::Get("multipass");
		glDepthFunc(GL_LEQUAL);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE);
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
	shader->setUniform("u_time", t );

	shader->setUniform("u_shadow_atlas", shadow_info.shadow_atlas->depth_texture, 8);
	shader->setUniform("u_shadow_atlas_dims", shadow_info.shadow_atlas_dims);

	if (singlepass_on) {
		// Upload all uniforms related to lighting
		if (!gbuffer_pass)
		{
			light_info.bind(shader);

			shadow_info.bindShadowAtlasPositions(shader, light_info.shadow_lights_idxs);
		}

		//do the draw call that renders the mesh into the screen
		mesh->render(GL_TRIANGLES);
	}
	else {
		for (int i = 0; i < light_info.l_count; i++) {

			if (!gbuffer_pass) {
				if (i == 0) {
					glDisable(GL_BLEND);
				}
				else {
					glEnable(GL_BLEND);
				}

				light_info.bind_single(shader, i);

				shadow_info.bindShadowAtlasPosition(shader, light_info.shadow_lights_idxs, i);
			}

			mesh->render(GL_TRIANGLES);
		}
	}

	// Render just the verticies as a wireframe
	if (render_wireframe)
		glPolygonMode( GL_FRONT_AND_BACK, GL_LINE );

	//disable shader
	shader->disable();

	//set the render state as it was before to avoid problems with future renders
	glDisable(GL_BLEND);
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	glDepthFunc(GL_LESS);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

#ifndef SKIP_IMGUI

void Renderer::showUI()
{
		
	ImGui::Checkbox("Wireframe", &render_wireframe);
	ImGui::Checkbox("Boundaries", &render_boundaries);

	//add here your stuff
	//...
	ImGui::Checkbox("Singlepass ON", &singlepass_on);
	ImGui::Checkbox("Front Face Culling ON", &front_face_culling_on);

	// for shadow atlas
	Shadows::showUI(shadow_info);

	ImGui::Checkbox("Deferred rendering", &deferred_on);
	ImGui::SliderFloat("Phong Shininess", &shininess, 20.f, 80.f);
}

#else
void Renderer::showUI() {}
#endif