
#include "material.h"

#include "../core/includes.h"
#include "../gfx/texture.h"
#include "../gfx/shader.h"

using namespace SCN;

std::map<std::string, Material*> Material::sMaterials;
uint32 Material::s_last_index = 0;
Material Material::default_material;

const char* SCN::texture_channel_str[] = { "ALBEDO","EMISSIVE","OPACITY","METALLIC_ROUGHNESS","OCCLUSION","NORMALMAP" };


Material* Material::Get(const char* name)
{
	assert(name);
	std::map<std::string, Material*>::iterator it = sMaterials.find(name);
	if (it != sMaterials.end())
		return it->second;
	return NULL;
}

void Material::registerMaterial(const char* name)
{
	this->name = name;
	sMaterials[name] = this;
}

Material::~Material()
{
	if (name.size())
	{
		auto it = sMaterials.find(name);
		if (it != sMaterials.end())
			sMaterials.erase(it);
	}
}

void Material::Release()
{
	std::vector<Material *>mats;

	for (auto mp : sMaterials)
	{
		Material *m = mp.second;
		mats.push_back(m);
	}

	for (Material *m : mats)
	{
		delete m;
	}
	sMaterials.clear();
}

void Material::bind(GFX::Shader* shader) {
	// First, configure the OpenGL state with the material settings =======================
	{
		// Select the blending
		if (alpha_mode == SCN::eAlphaMode::BLEND)
		{
			glEnable(GL_BLEND);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		}
		else
			glDisable(GL_BLEND);

		// Select if render both sides of the triangles
		if (two_sided)
			glDisable(GL_CULL_FACE);
		else
			glEnable(GL_CULL_FACE);

		// Check if any error
		assert(glGetError() == GL_NO_ERROR);
	}

	// Bind the textures and set uniforms =======================
	{
		int flags[SCN::eTextureChannel::ALL] = { 0 };
		GFX::Texture* texture = textures[SCN::eTextureChannel::ALBEDO].texture;

		// We always force a default albedo texture
		if (texture == NULL)
			texture = GFX::Texture::getWhiteTexture(); //a 1x1 white texture

		shader->setUniform("u_color", color);

		if (texture) {
			shader->setTexture("u_texture", texture, 0);
			flags[SCN::eTextureChannel::ALBEDO] = 1;
		}

		// start metallic-roughness texture
		texture = textures[SCN::eTextureChannel::METALLIC_ROUGHNESS].texture;

		if (texture) {
			shader->setTexture("u_texture_metallic_roughness", texture, 1);
			flags[SCN::eTextureChannel::METALLIC_ROUGHNESS] = 1;
		}
		// end metallic-roughness texture

		// start normal texture
		texture = textures[SCN::eTextureChannel::NORMALMAP].texture;

		if (texture) {
			shader->setTexture("u_normal_map", texture, 2);
			flags[SCN::eTextureChannel::NORMALMAP] = 1;
		}
		// end normal texture

		// HERE =====================
		// TODO: Expand rfor the rest of materials (when you need to)
		//	texture = emissive_texture;
		//	texture = occlusion_texture;
		// ==========================

		// This is used to say which is the alpha threshold to what we should not paint a pixel on the screen (to cut polygons according to texture alpha)
		shader->setUniform("u_alpha_cutoff", alpha_mode == SCN::eAlphaMode::MASK ? alpha_cutoff : 0.001f);

		shader->setUniform1Array("u_maps", (int*)flags, SCN::eTextureChannel::ALL);
	}

	// Upload some material properties =======================
	{
		shader->setUniform("u_roughness", roughness_factor);
		//shader->setUniform("u_metallic", metallic_factor);
	}
}
