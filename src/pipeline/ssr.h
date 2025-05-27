#pragma once

#include "gfx/fbo.h"

#include "renderer.h"

class GFX::Shader;

namespace SCN {

	const int SSR_MAX_STEP_COUNT = 1000;
	const int SSR_MAX_RAY_LEN = 20;

	class ScreenSpaceReflections {
	private:
		ScreenSpaceReflections();

	public:
		static ScreenSpaceReflections& instance()
		{
			static ScreenSpaceReflections INSTANCE;
			return INSTANCE;
		}

		bool is_active;
		int steps;
		float max_ray_len;
		float hidden_offset, step_size;

		GFX::FBO fbo;

		static void create_fbo(int width, int height);
		static void showUI();
		static void bind(GFX::Shader* shader);

		static void fill(Scene* scene, const GFX::FBO& prev_gbuffer_fbo, const GFX::FBO& prev_frame, bool lgc_active);
	};
}
