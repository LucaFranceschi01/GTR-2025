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

	shadow_atlas = new GFX::FBO();
	int nrows = MAX_LIGHTS / GFX::SHADOW_ATLAS_COLS + 1;
	shadow_atlas->setDepthOnly(GFX::SHADOW_RES * GFX::SHADOW_ATLAS_COLS, GFX::SHADOW_RES * nrows);
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

void Renderer::parseSceneEntities(SCN::Scene* scene, Camera* cam)
{
	// important to clear the list in each pass
	draw_commands_opaque.clear();
	draw_commands_transp.clear();
	
	light_info.count = 0;

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

void Renderer::generateShadowMaps()
{
	shadow_atlas->bind();
	
	glEnable(GL_DEPTH_TEST);
	glColorMask(false, false, false, false);

	glClear(GL_DEPTH_BUFFER_BIT);
	
	if (front_face_culling == false) {
		glDisable(GL_CULL_FACE);
	}
	else {
		glEnable(GL_CULL_FACE);
		//glFrontFace(GL_CW);
	}

	for (int i = 0; i < light_info.count; i++) {

		int row = i / GFX::SHADOW_ATLAS_COLS; // Integer division
		int col = i % GFX::SHADOW_ATLAS_COLS; // Modulo

		glViewport(col * GFX::SHADOW_RES, row * GFX::SHADOW_RES, GFX::SHADOW_RES, GFX::SHADOW_RES);
		/*glScissor(col * GFX::SHADOW_RES, row * GFX::SHADOW_RES, GFX::SHADOW_RES, GFX::SHADOW_RES);
		glEnable(GL_SCISSOR_TEST);*/

		LightEntity* light = light_info.entities[i];
		Camera light_camera;

		mat4 light_model = light->root.getGlobalMatrix();

		light_camera.lookAt(light_model.getTranslation(), light_model * vec3(0.f, 0.f, -1.f), vec3(0.f, 1.f, 0.f));

		float half_size = light->area / 2.f;

		if (light->light_type == SCN::eLightType::DIRECTIONAL) {
			light_camera.setOrthographic(
				-half_size, half_size,
				-half_size, half_size,
				light->near_distance, light->max_distance
			);
			light_info.viewprojections[i] = light_camera.viewprojection_matrix;
		}
		else if (light->light_type == SCN::eLightType::SPOT) {
			light_camera.setPerspective(light->cone_info.y, 1.f, light->near_distance, light->max_distance);
			light_info.viewprojections[i] = light_camera.viewprojection_matrix;
		}
		else {
			continue;
		}

		for (s_DrawCommand command : draw_commands_opaque) {
			renderPlain(i, &light_camera, command.model, command.mesh, command.material);
		}

		for (s_DrawCommand command : draw_commands_transp) {
			renderPlain(i, &light_camera, command.model, command.mesh, command.material);
		}

		//glDisable(GL_SCISSOR_TEST);
	}
	// Disable the default front face culling
	glDisable(GL_CULL_FACE);

	glDisable(GL_DEPTH_TEST);
	glColorMask(true, true, true, true);

	shadow_atlas->unbind();
}

void Renderer::renderScene(SCN::Scene* scene, Camera* camera)
{
	this->scene = scene;
	setupScene();

	parseSceneEntities(scene, camera);
	generateShadowMaps();

	//set the clear color (the background color)
	glClearColor(scene->background_color.x, scene->background_color.y, scene->background_color.z, 1.0);

	// Clear the color and the depth buffer
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	GFX::checkGLErrors();

	//render skybox
	if(skybox_cubemap)
		renderSkybox(skybox_cubemap);

	// first render opaque entities
	for (s_DrawCommand command : draw_commands_opaque) {
		renderMeshWithMaterial(command.model, command.mesh, command.material);
	}

	// then render transparent entities
	for (s_DrawCommand command : draw_commands_transp) {
		renderMeshWithMaterial(command.model, command.mesh, command.material);
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
void Renderer::renderMeshWithMaterial(const Matrix44 model, GFX::Mesh* mesh, SCN::Material* material)
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
	if (singlepass_on) {
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

	shader->setUniform("u_shadow_atlas", shadow_atlas->depth_texture, 8);

	if (singlepass_on) {
		// Upload all uniforms related to lighting
		light_info.bind(shader);

		//do the draw call that renders the mesh into the screen
		mesh->render(GL_TRIANGLES);
	}
	else {
		for (int i = 0; i < light_info.count; i++) {

			if (i == 0) {
				glDisable(GL_BLEND);
			}
			else {
				glEnable(GL_BLEND);
			}
			
			light_info.bind_single(shader, i);

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
	glPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
	glDepthFunc(GL_LESS);
}

void Renderer::renderPlain(int i, Camera* light_camera, const Matrix44 model, GFX::Mesh* mesh, SCN::Material* material)
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

	light_info.bind_single(shader, i);

	mesh->render(GL_TRIANGLES);

	//disable shader
	shader->disable();
}

#ifndef SKIP_IMGUI

void Renderer::showUI()
{
		
	ImGui::Checkbox("Wireframe", &render_wireframe);
	ImGui::Checkbox("Boundaries", &render_boundaries);

	//add here your stuff
	//...
	ImGui::Checkbox("Singlepass ON", &singlepass_on);
	ImGui::Checkbox("Front Face Culling ON", &front_face_culling);
}

#else
void Renderer::showUI() {}
#endif