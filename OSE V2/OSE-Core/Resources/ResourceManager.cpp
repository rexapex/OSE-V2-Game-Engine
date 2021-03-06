#include "stdafx.h"
#include "ResourceManager.h"
#include "OSE-Core/EngineReferences.h"
#include "OSE-Core/Rendering/RenderingFactory.h"
#include "Texture/TextureLoaderFactory.h"
#include "Texture/Texture.h"
#include "Texture/TextureLoader.h"
#include "Texture/TextureMetaData.h"
#include "Tilemap/TilemapLoaderFactory.h"
#include "Tilemap/Tilemap.h"
#include "Tilemap/TilemapLoader.h"
#include "Mesh/Mesh.h"
#include "Mesh/MeshLoader.h"
#include "Mesh/MeshLoaderFactory.h"
#include "Material/Material.h"
#include "OSE-Core/File System/FileSystemUtil.h"

#include "OSE-Core/Shader/ShaderProg.h"
#include "OSE-Core/Shader/Shaders/ShaderGraph2D.h"
#include "OSE-Core/Shader/Shaders/ShaderGraph3D.h"

namespace ose
{
	ResourceManager::ResourceManager(std::string const & project_path) : project_path_(project_path),
		texture_loader_(TextureLoaderFactories[0]->NewTextureLoader(project_path)),
		tilemap_loader_(TilemapLoaderFactories[0]->NewTilemapLoader(project_path)),
		mesh_loader_(MeshLoaderFactories[0]->NewMeshLoader(project_path))
	{
		// Create the default 2d engine materials and shader programs
		uptr<Material> default_opaque_2d { Material::NewDefaultOpaqueSpriteMaterial() };
		uptr<Material> default_alpha_2d  { Material::NewDefaultAlphaSpriteMaterial() };
		AddShaderProg("OSE-Default2dShaderProg");
		default_opaque_2d->SetShaderProg(GetShaderProg("OSE-Default2dShaderProg"));
		default_alpha_2d->SetShaderProg(GetShaderProg("OSE-Default2dShaderProg"));
		materials_.emplace(default_opaque_2d->GetName(), std::move(default_opaque_2d));
		materials_.emplace(default_alpha_2d->GetName(), std::move(default_alpha_2d));

		// Create the default 3d engine materials and shader programs
		uptr<Material> default_opaque_3d { Material::NewDefaultOpaqueMeshMaterial() };
		AddShaderProg("OSE-Default3dShaderProg");
		default_opaque_3d->SetShaderProg(GetShaderProg("OSE-Default3dShaderProg"));
		materials_.emplace(default_opaque_3d->GetName(), std::move(default_opaque_3d));
	}

	ResourceManager::~ResourceManager() noexcept {}

	ResourceManager::ResourceManager(ResourceManager && other) noexcept : project_path_(std::move(other.project_path_)),
		textures_without_Gpu_memory_(std::move(other.textures_without_Gpu_memory_)), textures_with_Gpu_memory_(std::move(other.textures_with_Gpu_memory_)),
		texture_loader_(std::move(other.texture_loader_)), tilemap_loader_(std::move(other.tilemap_loader_)),
		mesh_loader_(std::move(other.mesh_loader_)) {}
	
	//import a file into the project resources directory
	//sub_dir is a sub directory within the resources directory
	void ResourceManager::ImportFile(std::string const & file_path, std::string const & sub_dir)
	{
		//TODO - don't accept .meta files
		//TODO - auto generate a .meta file for the new resources if successfully imported (but I don't know it's type here!!!)
		fs::CopyFile_(file_path, project_path_ + "/Resources/" + sub_dir + "/" + fs::GetFilenameFromPath(file_path));
	}

	//imports multiple files into project resources directory
	//sub_dir is a sub directory within the resources directory
	void ResourceManager::ImportFiles(std::vector<std::string> const & file_paths, std::string const & sub_dir)
	{
		for(auto const & path : file_paths)
		{
			ImportFile(path, sub_dir);
		}
	}

	// get the texture from either map
	// given the name of the texture, return the texture object
	Texture const * ResourceManager::GetTexture(std::string const & name)
	{
		// search the textures_with_GPU_memory_ list
		auto const & tex_iter { textures_with_Gpu_memory_.find(name) };
		if(tex_iter != textures_with_Gpu_memory_.end()) {
			return tex_iter->second.get();
		}

		// then, if texture couldn't be found, search the textures_without_GPU_memory_ list
		auto const & tex_iter2 { textures_without_Gpu_memory_.find(name) };
		if(tex_iter2 != textures_without_Gpu_memory_.end()) {
			return tex_iter2->second.get();
		}

		return nullptr;
	}

	// adds the texture at path to the list of active textures, the texture must be in the project's resources directory
	// path is relative to ProjectPath/Resources
	// if no name is given, the filepath will be used
	// TODO - either remove name altogether or come up with something clever
	void ResourceManager::AddTexture(std::string const & path, std::string const & name)
	{
		std::string abs_path { project_path_ + "/Resources/" + path };

		if(fs::DoesFileExist(abs_path))
		{
			// if no name is given, use the filename
			std::string name_to_use { name };
			if(name_to_use == "")
			{
				name_to_use = path;//fs::FilenameFromPath(abs_path);
			}

			// only add the new texture if the name is not taken (in either map)
			auto & iter = textures_without_Gpu_memory_.find(name_to_use);
			auto & iter2 = textures_with_Gpu_memory_.find(name_to_use);
			if(iter == textures_without_Gpu_memory_.end() && iter2 == textures_with_Gpu_memory_.end())
			{
				textures_without_Gpu_memory_.emplace(name_to_use, RenderingFactories[0]->NewTexture(name_to_use, abs_path));
				DEBUG_LOG("Added texture", name_to_use, "to ResourceManager");
				
				// get a references to the newly created texture
				auto & tex = textures_without_Gpu_memory_.at(name_to_use);

				// load or generate the texture's meta data
				std::string meta_abs_path { abs_path + ".meta" };
				bool success = false;
				TextureMetaData meta_data;	//object will have default values
				if(fs::DoesFileExist(meta_abs_path))
				{
					//load meta data file
					try
					{
						LoadTextureMetaFile(meta_abs_path, meta_data);
						success = true;
					}
					catch(std::exception const &) {}	// success will be false, therefore, meta file will be created
				}
				else if(!success)
				{
					// create meta data file
					// NOTE - compiler auto concatenates adjacent string literals
					fs::WriteTextFile(meta_abs_path,	"mag_filter_mode 0\n"
																	"min_filter_mode 0\n"
																	"mip_mapping_enabled 1\n"
																	"min_LOD 0\n"
																	"max_LOD 0\n"
																	"LOD_bias 0");
				}

				// set the texture's meta data
				tex->SetMetaData(meta_data);

				// TODO - do the loading with multi-threading
				int32_t w, h, channels;
				IMGDATA d;
				texture_loader_->LoadTexture(abs_path, &d, &w, &h, &channels);
				if(d == NULL)
					tex->SetImgData(NULL, 0, 0, 0);
				else
					tex->SetImgData(d, w, h, channels);
			}
			else
			{
				LOG("error: texture name " + name_to_use + " is already taken");
			}
		}
	}

	// create the GPU memory for an already loaded (added) texture
	// returns an iterator to the next texture in the textures_without_GPU_memory map
	// IMPORANT - can only be called from the thread which contains the render context
	std::map<std::string, uptr<Texture>>::const_iterator ResourceManager::CreateTexture(std::string const & tex_name)
	{
		// get the texture if it exists
		// only texture with no representation in GPU memory can be created
		auto const & tex_iter { textures_without_Gpu_memory_.find(tex_name) };

		// if the texture exists, then create its GPU representation
		if(tex_iter != textures_without_Gpu_memory_.end())
		{
			try {
				// try to create the texture on the GPU
				tex_iter->second->CreateTexture();
				// iff the creation succeeds, move the texture from one map to the other
				textures_with_Gpu_memory_.insert({ tex_name, std::move(tex_iter->second) });
				// remove the texture from the original map
				return textures_without_Gpu_memory_.erase(tex_iter);
			} catch(std::exception const & e) {
				LOG_ERROR(e.what());
			}
		}

		return tex_iter;
	}

	// remove the texture from the textures list and free the texture's resources
	// IMPORANT - can only be called from the thread which contains the render context
	void ResourceManager::RemoveTexture(std::string const & tex_name)
	{
		// get the texture if it exists
		// only texture without memory allocated on the GPU can be removed
		auto const & tex_iter { textures_without_Gpu_memory_.find(tex_name) };

		// if the texture exists, then free its resources and remove it from map
		if(tex_iter != textures_without_Gpu_memory_.end())
		{
			// free the image data resource
			texture_loader_->FreeTexture(tex_iter->second->GetImgData());
			// remove the texture from the map
			textures_without_Gpu_memory_.erase(tex_iter);
		}
	}

	// free the GPU memory of the texture
	// IMPORANT - can only be called from the thread which contains the render context
	void ResourceManager::DestroyTexture(std::string const & tex_name)
	{
		// get the texture if it exists
		// only texture with memory allocated on the GPU can be destroyed
		auto const & tex_iter { textures_with_Gpu_memory_.find(tex_name) };

		// if the texture exists, then free its resources and remove it from map
		if(tex_iter != textures_with_Gpu_memory_.end())
		{
			// free the image data resource
			tex_iter->second->DestroyTexture();
			// iff the creation succeeds, move the texture from one map to the other
			textures_without_Gpu_memory_.insert({ tex_name, std::move(tex_iter->second) });
			// remove the texture from the map
			textures_with_Gpu_memory_.erase(tex_iter);
		}
	}

	// create all textures which are currently lacking a GPU representation
	// IMPORTANT - can only be called from the thread which contains the render context
	void ResourceManager::CreateTextures()
	{
		std::map<std::string, uptr<Texture>>::const_iterator it;

		// create a GPU texture for each texture without a GPU texture representation
		for(it = textures_without_Gpu_memory_.begin(); it != textures_without_Gpu_memory_.end(); )
		{
			// delegate to this method because why not
			it = CreateTexture(it->first);
		}
	}

	//loads a meta file for some texture, meta files map properties to values
	void ResourceManager::LoadTextureMetaFile(std::string const & abs_path, TextureMetaData & meta_data)
	{
		// Load the file (OSE stores meta files as property files)
		auto props { LoadPropertyFile(abs_path) };

		// Parse the properties
		for(auto & [property, value_str] : props)
		{
			try
			{
				uint32_t value = std::stoi(value_str);

				if(property == "mag_filter_mode") {
					meta_data.mag_filter_mode_ = static_cast<ETextureFilterMode>(value);
				} else if(property == "min_filter_mode") {
					meta_data.min_filter_mode_ = static_cast<ETextureFilterMode>(value);
				} else if(property == "mip_mapping_enabled") {
					meta_data.mip_mapping_enabled_ = static_cast<bool>(value);
				} else if(property == "min_LOD") {
					meta_data.min_lod_ = value;
				} else if(property == "max_LOD") {
					meta_data.max_lod_ = value;
				} else if(property == "LOD_bias") {
					meta_data.lod_bias_ = value;
				}
			}
			catch(...)
			{
				LOG_ERROR("Failed to convert texture meta property", property, "of value", value_str, "to uint32 in meta file", abs_path);
			}
		}
	}

	// Get the tilemap from the resources manager
	// Given the name of the tilemap, return the tilemap object
	Tilemap const * ResourceManager::GetTilemap(std::string const & name)
	{
		// search the tilemaps_ list
		auto const & iter { tilemaps_.find(name) };
		if(iter != tilemaps_.end()) {
			return iter->second.get();
		}

		return nullptr;
	}

	// Adds the tilemap at path to the list of active tilemaps, the tilemap must be in the project's resources directory
	// Path is relative to ProjectPath/Resources
	// If no name is given, the relative path will be used
	// IMPORTANT - Can be called from any thread (TODO)
	void ResourceManager::AddTilemap(std::string const & path, std::string const & name)
	{
		std::string abs_path { project_path_ + "/Resources/" + path };

		if(fs::DoesFileExist(abs_path))
		{
			// if no name is given, use the filename
			std::string name_to_use { name };
			if(name_to_use == "")
			{
				name_to_use = path;//fs::FilenameFromPath(abs_path);
			}

			// only add the new tilemaps if the name is not taken
			auto & iter = tilemaps_.find(name_to_use);
			if(iter == tilemaps_.end())
			{
				tilemaps_.emplace(name_to_use, ose::make_unique<Tilemap>(name_to_use, abs_path));
				DEBUG_LOG("Added tilemap", name_to_use, "to ResourceManager");

				// get a references to the newly created tilemap
				auto & tilemap = tilemaps_.at(name_to_use);

				// TODO - Do the loading with multi-threading
				tilemap_loader_->LoadTilemap(abs_path, *tilemap);
			}
			else
			{
				LOG("error: tilemap name " + name_to_use + " is already taken");
			}
		}
	}

	// Remove the tilemap from the tilemaps list and free the tilemap's resources
	// IMPORANT - can be called from any thread (TODO)
	void ResourceManager::RemoveTilemap(std::string const & name)
	{
		// Get the tilemap if it exists
		auto const & iter { tilemaps_.find(name) };

		// If the tilemap exists, remove it from the map
		if(iter != tilemaps_.end())
		{
			// Remove the texture from the map
			tilemaps_.erase(iter);
		}
	}

	// Get the mesh from the resource manager
	// Given the name of the mesh, return the mesh object
	Mesh const * ResourceManager::GetMesh(std::string const & name)
	{
		// Search the meshes_ list
		auto const & iter { meshes_.find(name) };
		if(iter != meshes_.end()) {
			return iter->second.get();
		}

		return nullptr;
	}

	// Adds the mesh at path to the list of active meshes, the mesh must be in the project's resources directory
	// Path is relative to ProjectPath/Resources
	// If no name is given, the relative path will be used
	// IMPORTANT - Can be called from any thread (TODO)
	void ResourceManager::AddMesh(std::string const & path, std::string const & name)
	{
		std::string abs_path { project_path_ + "/Resources/" + path };

		if(fs::DoesFileExist(abs_path))
		{
			// If no name is given, use the filename
			std::string name_to_use { name };
			if(name_to_use == "")
			{
				name_to_use = path;//fs::FilenameFromPath(abs_path);
			}

			// Only add the new mesh if the name is not taken
			auto & iter = meshes_.find(name_to_use);
			if(iter == meshes_.end())
			{
				meshes_.emplace(name_to_use, ose::make_unique<Mesh>(name_to_use, abs_path));
				DEBUG_LOG("Added mesh", name_to_use, "to ResourceManager");

				// Get a references to the newly created mesh
				auto & mesh = meshes_.at(name_to_use);

				// TODO - Do the loading with multi-threading
				mesh_loader_->LoadMesh(abs_path, mesh.get());
			}
			else
			{
				LOG("error: mesh name " + name_to_use + " is already taken");
			}
		}
	}

	// Remove the mesh from the meshes list and free the meshes resources
	// IMPORTANT - Can be called from any thread (TODO)
	void ResourceManager::RemoveMesh(std::string const & name)
	{
		// Get the mesh if it exists
		auto const & iter { meshes_.find(name) };

		// If the mesh exists, remove it from the map
		if(iter != meshes_.end())
		{
			// Remove the mesh from the map
			meshes_.erase(iter);
		}
	}

	// Get the material from the resource manager
	// Given the name of the material, return the material object
	Material const * ResourceManager::GetMaterial(std::string const & name)
	{
		// Search the materials_ list
		auto const & iter { materials_.find(name) };
		if(iter != materials_.end()) {
			return iter->second.get();
		}

		return nullptr;
	}

	// Adds the material at path to the list of active materials, the material must be in the project's resources directory
	// Path is relative to ProjectPath/Resources
	// If no name is given, the relative path will be used
	// IMPORTANT - Can be called from any thread (TODO)
	void ResourceManager::AddMaterial(std::string const & path, std::string const & name)
	{
		std::string abs_path { project_path_ + "/Resources/" + path };

		if(fs::DoesFileExist(abs_path))
		{
			// If no name is given, use the filename
			std::string name_to_use { name };
			if(name_to_use == "")
			{
				name_to_use = path;//FileHandlingUtil::filenameFromPath(abs_path);
			}

			// Only add the new material if the name is not taken
			auto & iter = materials_.find(name_to_use);
			if(iter == materials_.end())
			{
				materials_.emplace(name_to_use, ose::make_unique<Material>(name_to_use, abs_path));
				DEBUG_LOG("Added material", name_to_use, "to ResourceManager");

				// Get a references to the newly created material
				auto & material = materials_.at(name_to_use);

				// Load the material data
				auto props { LoadPropertyFile(abs_path) };
				for(auto & [key, value] : props)
				{
					if(key == "tex")
					{
						AddTexture(value);
						auto texture { GetTexture(value) };
						// Add texture to material even if nullptr to preserve texture indices
						material->AddTexture(texture);
					}
					else if(key == "shader")
					{
						AddShaderProg(value);
						auto shader_prog { GetShaderProg(value) };
						// Add shader to material
						material->SetShaderProg(shader_prog);
					}
				}
			}
			else
			{
				LOG("Error: material name " + name_to_use + " is already taken");
			}
		}
	}

	// Remove the material from the materials list and free the material's resources
	// IMPORTANT - Can be called from any thread (TODO)
	void ResourceManager::RemoveMaterial(std::string const & name)
	{
		// Get the material if it exists
		auto const & iter { materials_.find(name) };

		// If the material exists, remove it from the map
		if(iter != materials_.end())
		{
			// Remove the material from the map
			materials_.erase(iter);
		}
	}

	// Get the shader program from the resource manager
	// Given the name of the shader program, return the shader program object
	ShaderProg const * ResourceManager::GetShaderProg(std::string const & name)
	{
		// Search the shader_progs_with_gpu_memory_ list
		auto const & iter1 { shader_progs_with_gpu_memory_.find(name) };
		if(iter1 != shader_progs_with_gpu_memory_.end()) {
			return iter1->second.get();
		}

		// Search the shader_progs_without_gpu_memory_ list
		auto const & iter2 { shader_progs_without_gpu_memory_.find(name) };
		if(iter2 != shader_progs_without_gpu_memory_.end()) {
			return iter2->second.get();
		}

		return nullptr;
	}

	// Adds the shader program at path to the list of active shader programs, the shader program must be in the project's resources directory
	// Path is relative to ProjectPath/Resources
	// If path starts with OSE then path will be interpreted as the name of a builtin shader graph
	// If no name is given, the relative path will be used
	// IMPORTANT - Can be called from any thread (TODO)
	void ResourceManager::AddShaderProg(std::string const & path)
	{
		// Only add the new shader program if the name is not taken
		auto & iter1 = shader_progs_with_gpu_memory_.find(path);
		auto & iter2 = shader_progs_without_gpu_memory_.find(path);
		if(iter1 == shader_progs_with_gpu_memory_.end() && iter2 == shader_progs_without_gpu_memory_.end())
		{
			std::string builtin_prefix { "OSE" };
			if(!path.compare(0, builtin_prefix.size(), builtin_prefix))
			{
				// Load a built-in shader
				if(path == "OSE-Default3dShaderProg")
					shader_progs_without_gpu_memory_.emplace(path, RenderingFactories[0]->NewShaderProg(ose::make_unique<ShaderGraph3D>()));
				else if(path == "OSE-Default2dShaderProg")
					shader_progs_without_gpu_memory_.emplace(path, RenderingFactories[0]->NewShaderProg(ose::make_unique<ShaderGraph2D>()));
				else
					LOG_ERROR("Built-in shader", path, "does not exist\nCustom shader paths cannot start with OSE");
			}
			else
			{
				// Load a custom shader from the filesystem
				// TODO
			}
		}
		else
		{
			LOG_ERROR("Shader name", path, "is already taken");
		}
	}

	// Create the GPU memory for an already loaded (added) shader program
	// Returns an iterator to the next shader program in the shader_progs_without_gpu_memory map
	// IMPORANT - can only be called from the thread which contains the render context
	std::map<std::string, uptr<ShaderProg>>::const_iterator ResourceManager::CreateShaderProg(std::string const & prog_name)
	{
		// Get the shader program if it exists
		// Only shader program with no representation in GPU memory can be created
		auto const & iter { shader_progs_without_gpu_memory_.find(prog_name) };

		// If the shader program exists, then create its GPU representation
		if(iter != shader_progs_without_gpu_memory_.end())
		{
			try {
				// Try to create the shader program on the GPU
				iter->second->CreateShaderProg();
				// Iff the creation succeeds, move the shader program from one map to the other
				shader_progs_with_gpu_memory_.insert({ prog_name, std::move(iter->second) });
				// Remove the shader program from the original map
				return shader_progs_without_gpu_memory_.erase(iter);
			} catch(std::exception const & e) {
				LOG_ERROR(e.what());
			}
		}

		return iter;
	}

	// Remove the shader program from the shader programs list and free the shader program's resources
	// IMPORTANT - Can be called from any thread (TODO)
	void ResourceManager::RemoveShaderProg(std::string const & name)
	{
		// Get the shader program if it exists
		auto const & iter { shader_progs_without_gpu_memory_.find(name) };

		// If the shader program exists, remove it from the map
		if(iter != shader_progs_without_gpu_memory_.end())
		{
			// Remove the shader program from the map
			shader_progs_without_gpu_memory_.erase(iter);
		}
	}

	// Free the GPU memory of the shader program
	// IMPORANT - can only be called from the thread which contains the render context
	void ResourceManager::DestroyShaderProg(std::string const & prog_name)
	{
		// Get the shader program if it exists
		// Only shader programs with memory allocated on the GPU can be destroyed
		auto const & iter { shader_progs_with_gpu_memory_.find(prog_name) };

		// If the shader program exists, then free its resources and remove it from map
		if(iter != shader_progs_with_gpu_memory_.end())
		{
			// Destroy the shader program gpu memory
			iter->second->DestroyShaderProg();
			// Iff the creation succeeds, move the shader program from one map to the other
			shader_progs_without_gpu_memory_.insert({ prog_name, std::move(iter->second) });
			// Remove the shader program from the map
			shader_progs_with_gpu_memory_.erase(iter);
		}
	}

	// Create all shader programs which are currently lacking a GPU representation
	// IMPORTANT - can only be called from the thread which contains the render context
	void ResourceManager::CreateShaderProgs()
	{
		std::map<std::string, uptr<ShaderProg>>::const_iterator it;

		// Create a GPU shader program object for each shader program without a GPU texture representation
		for(it = shader_progs_without_gpu_memory_.begin(); it != shader_progs_without_gpu_memory_.end(); )
		{
			// Delegate to this method because why not
			it = CreateShaderProg(it->first);
		}
	}

	// Load a property file (similar to an ini file)
	// Returns properties as a map from key to value
	std::unordered_multimap<std::string, std::string> ResourceManager::LoadPropertyFile(std::string const & abs_path)
	{
		std::unordered_multimap<std::string, std::string> props;

		// Load the file into a string
		std::string contents;
		try
		{
			fs::LoadTextFile(abs_path, contents);
		}
		catch(std::exception const & e)
		{
			// Error occurred, therefore, return an empty project info stub
			LOG("fs::LoadTextFile ->", e.what());
			throw e;
		}

		contents.erase(std::remove(contents.begin(), contents.end(), '\r'), contents.end());
		std::stringstream iss { contents };
		std::string line;
		while(std::getline(iss, line, '\n'))
		{
			std::stringstream lss { line };
			std::string property;
			lss >> property;
			std::string value;
			if(line.size() > property.size() + 1)
				value = line.substr(property.size()+1, line.size()-property.size());
			if(property != "")
				props.insert({ property, value });//[property] = value;
		}

		return props;
	}
}
