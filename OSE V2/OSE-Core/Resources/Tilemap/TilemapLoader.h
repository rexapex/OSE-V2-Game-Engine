#pragma once

#include "Tilemap.h"

namespace ose
{
	class TilemapLoader
	{
	public:
		TilemapLoader(std::string const & project_path);
		virtual ~TilemapLoader();
		//TilemapLoader is NOT copyable
		TilemapLoader(TilemapLoader &) = delete;
		TilemapLoader & operator=(TilemapLoader &) = delete;
		//TilemapLoader IS movable
		TilemapLoader(TilemapLoader &&) noexcept = default;
		TilemapLoader & operator=(TilemapLoader &&) noexcept = default;

		// Loads the tilemap and sets the internal data of the tilemap object
		// Path is absolute and is guaranteed to exist
		virtual void LoadTilemap(std::string const & path, Tilemap & tilemap) = 0;
	private:
		std::string project_path_;
	};
}
