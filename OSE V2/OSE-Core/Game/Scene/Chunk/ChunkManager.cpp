#include "stdafx.h"
#include "ChunkManager.h"
#include "Chunk.h"
#include "OSE-Core/Entity/Entity.h"
#include "OSE-Core/Project/ProjectLoader.h"
#include "OSE-Core/Math/ITransform.h"

namespace ose
{
	ChunkManager::ChunkManager() : settings_()
	{
	
	}

	ChunkManager::~ChunkManager()
	{
	
	}

	ChunkManager::ChunkManager(ChunkManager const & other)
	{
		settings_ = other.settings_;
		// Perform a deep copy of all chunks
		loaded_chunks_.clear();
		unloaded_chunks_.clear();
		for(auto const & c : other.loaded_chunks_)
			loaded_chunks_.push_back(std::make_unique<Chunk>(*c));
		for(auto const & c : other.unloaded_chunks_)
			unloaded_chunks_.push_back(std::make_unique<Chunk>(*c));
	}
	
	// Determine whether chunks should be loaded/unloaded
	void ChunkManager::UpdateChunks()
	{
		if(settings_.agent_)
		{
			for(auto & iter = unloaded_chunks_.begin(); iter != unloaded_chunks_.end();)
			{
				std::unique_ptr<Chunk> & chunk = *iter;
				if(glm::distance2(chunk->GetGlobalTransform().GetTranslation(), settings_.agent_->GetGlobalTransform().GetTranslation()) <= std::pow(settings_.load_distance_, 2))
				{
					chunk->Load();
					OnChunkActivated(*chunk);
					std::move(iter, iter+1, std::back_inserter(loaded_chunks_));
					iter = unloaded_chunks_.erase(iter);
				}
				else
				{
					++iter;
				}
			}

			for(auto & iter = loaded_chunks_.begin(); iter != loaded_chunks_.end();)
			{
				std::unique_ptr<Chunk> & chunk = *iter;
				if(glm::distance2(chunk->GetGlobalTransform().GetTranslation(), settings_.agent_->GetGlobalTransform().GetTranslation()) >= std::pow(settings_.unload_distance_, 2))
				{
					OnChunkDeactivated(*chunk);
					chunk->Unload();
					std::move(iter, iter+1, std::back_inserter(unloaded_chunks_));
					iter = loaded_chunks_.erase(iter);
				}
				else
				{
					++iter;
				}
			}
		}
	}
	
	std::vector<Entity *> ChunkManager::FindLoadedChunkEntitiesWithName(std::string_view name)
	{
		std::vector<Entity *> vec;
		for(auto & chunk : loaded_chunks_)
		{
			std::vector<Entity *> v { chunk->FindDescendentEntitiesWithName(name) };
			std::copy(v.begin(), v.end(), vec.end());
		}
		return vec;
	}
}
