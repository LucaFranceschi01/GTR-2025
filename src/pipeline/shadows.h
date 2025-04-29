#pragma once

#include "math.h"
#include "light.h"

class Camera;

namespace GFX {
	class FBO;
	class Mesh;
}

namespace SCN {
	class Material;
	struct s_DrawCommand;

	class Shadows {
	public:
		int shadow_map_res = 1024;
		int shadow_map_count = MAX_LIGHTS;
		Vector2<int> shadow_atlas_dims; // (cols, rows)

		GFX::FBO* shadow_atlas;

		// ctor
		Shadows();

		// Updates the FBO if the dimensions have changed somehow
		void updateShadowAtlasSize(int shadows_count);

		// Fills up the shadow atlas
		void generateShadowMaps(std::vector<s_DrawCommand>& opaque, std::vector<s_DrawCommand>& transparent,
			LightUniforms& light_info, bool ffc);

		// Renders the mesh depth into the shadowmap
		void renderPlain(Camera* light_camera, const Matrix44 model, GFX::Mesh* mesh, SCN::Material* material);

		// Searches shadowmap position for a given light index and binds it with the shader
		void bindShadowAtlasPosition(GFX::Shader* shader, std::vector<int>& shadow_indices, int light_index);

		// Prepare the shadow indices array and bind it with the shader
		void bindShadowAtlasPositions(GFX::Shader* shader, std::vector<int>& shadow_indices);

		// Shows UI elements
		static void showUI(Shadows& shadow_info);
	};
}