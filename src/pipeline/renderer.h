#pragma once
#include "scene.h"
#include "prefab.h"
#include "light.h"
#include "shadows.h"

#include "gfx/fbo.h"

//forward declarations
class Camera;
class Skeleton;
namespace GFX {
	class Shader;
	class Mesh;
}

namespace SCN {

	class Prefab;
	class Material;

	// minimal information for a draw call of a node
	struct s_DrawCommand {
		Matrix44 model;
		GFX::Mesh* mesh;
		SCN::Material* material;
	};

	struct s_TonemapperInfo {
		bool active = true;
		float scale = 1.f;
		float average_lum = 1.f;
		float lumwhite2 = 1.f;
		float igamma = 1.f;
	};

	enum e_PipelineMode {
		FORWARD,
		DEFERRED,
		FORWARD_PLUS,
		COUNT_PIPELINEMODE
	};

	enum e_PassSetting {
		SINGLEPASS,
		MULTIPASS,
		COUNT_PASSSETTING
	};

	enum e_ReflectanceModel {
		PHONG,
		PBR,
		COUNT_REFLECTANCEMODEL
	};

	// This class is in charge of rendering anything in our system.
	// Separating the render from anything else makes the code cleaner
	class Renderer
	{
	public:
		bool render_wireframe;
		bool render_boundaries;
		bool front_face_culling_on = false;
		bool frustum_culling = false;
		bool linear_gamma_correction = true;
		
		e_PipelineMode pipeline_mode = DEFERRED;
		e_PassSetting pass_setting = SINGLEPASS;
		e_ReflectanceModel reflectance_model = PBR;

		GFX::Texture* skybox_cubemap;

		Shadows shadow_info;

		SCN::Scene* scene;

		// setup opaque and transparent renderables
		std::vector<SCN::s_DrawCommand> draw_commands_opaque;
		std::vector<SCN::s_DrawCommand> draw_commands_transp;
		
		SCN::LightUniforms light_info;

		GFX::FBO gbuffer_fbo, lighting_fbo;

		float shininess = 30.f;

		s_TonemapperInfo tonemapper;

		//updated every frame
		Renderer(const char* shaders_atlas_filename );

		//just to be sure we have everything ready for the rendering
		void setupScene();

		void parseSceneEntities(SCN::Scene* scene, Camera* camera);

		//renders several elements of the scene
		void renderScene(SCN::Scene* scene, Camera* camera);
		void renderSceneForward(SCN::Scene* scene, Camera* camera);
		void renderSceneDeferred(SCN::Scene* scene, Camera* camera);

		//render the skybox
		void renderSkybox(GFX::Texture* cubemap);

		//to render one mesh given its material and transformation matrix
		void renderMeshWithMaterial(const Matrix44 model, GFX::Mesh* mesh, SCN::Material* material);
		void renderMeshWithMaterialDeferred(const Matrix44 model, GFX::Mesh* mesh, SCN::Material* material);
		void renderMeshWithMaterialForward(const Matrix44 model, GFX::Mesh* mesh, SCN::Material* material);

		void showUI();
		
		// Recursively iterate over all children of a node, adding the needed ones to renderables list
		void parseNodes(SCN::Node* node, Camera* cam);

		// Fill the G-Buffer with the information from opaque and transparent geometry
		void fillGBuffer();

		// Display the scene through deferred render using the G-Buffer information
		void fillLightingFBOSinglepass(SCN::Scene* scene, Camera* camera);
		void fillLightingFBOMultipass(SCN::Scene* scene, Camera* camera);

		void displayScene(SCN::Scene* scene);
	};
};