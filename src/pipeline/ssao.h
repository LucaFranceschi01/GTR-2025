#pragma once

#include "gfx/fbo.h"
#include "core/math.h"

class GFX::Shader;

namespace SCN {
	class Scene;

	const int MAX_SSAO_SAMPLES = 64;

	class SSAO {
	private:
		SSAO();

	public:
		static SSAO& instance()
		{
			static SSAO INSTANCE;
			return INSTANCE;
		}

		bool is_active;
		bool is_ssao_plus_active;
		int samples;
		float sample_radius;
		bool is_half_active;

		std::vector<Vector3f> ao_sample_points;

		GFX::FBO fbo, fbo_full;

		static void create_fbo(int width, int height);
		static void showUI();
		static void bind(GFX::Shader* shader);

		void pointGenerator();
		static const std::vector<Vector3f> generateSpherePoints(int num, float radius = 1.f, bool hemi = false);

		static void compute(SCN::Scene* scene, const GFX::FBO& gbuffer_fbo);
	};
}
