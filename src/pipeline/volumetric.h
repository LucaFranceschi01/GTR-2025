#pragma once

#include "gfx/fbo.h"
#include "core/math.h"

#include "renderer.h"

class GFX::Shader;

namespace SCN {
	class Scene;

	const int VOLUMETRIC_MAX_STEP_COUNT = 1000;
	const int VOLUMETRIC_MAX_RAY_LEN = 20;

	class VolumetricRendering {
	private:
		VolumetricRendering();

	public:
		static VolumetricRendering& instance()
		{
			static VolumetricRendering INSTANCE;
			return INSTANCE;
		}

		bool is_active, is_half_active;
		int steps;
		float max_ray_len, air_density, vertical_density_factor;

		GFX::FBO fbo, fbo_full;

		static void create_fbo(int width, int height);
		static void showUI();
		static void bind(GFX::Shader* shader);

		static void compute(SCN::Scene* scene, const GFX::FBO& gbuffer_fbo, SCN::LightUniforms& light_info, Shadows& shadow_info, bool lgc_active);
	};
}
