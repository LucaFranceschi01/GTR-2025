#pragma once

#include "gfx/fbo.h"
#include "core/math.h"

class GFX::Shader;

namespace SCN {
	class Scene;

	const int VOLUMETRIC_MAX_STEP_COUNT = 256;
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

		bool is_active;
		int steps;
		float max_ray_len, air_density;

		GFX::FBO fbo;

		static void create_fbo(int width, int height);
		static void showUI();
		static void bind(GFX::Shader* shader);

		static void compute(SCN::Scene* scene, const GFX::FBO& gbuffer_fbo);
	};
}
