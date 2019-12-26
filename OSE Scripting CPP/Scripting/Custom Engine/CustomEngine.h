#pragma once
#include "OSE-Core/Scripting/ScriptingEngine.h"

namespace ose
{
	namespace entity
	{
		class Entity;
		class CustomComponent;
	}
}

namespace ose::scripting
{
	class CustomEngine;
	using CustomEngineFactory = std::unique_ptr<ose::scripting::CustomEngine>(*)();

	class CustomEngine
	{
	protected:
		CustomEngine();

	public:
		virtual ~CustomEngine();

		virtual std::string GetComponentTypeName() const = 0;
		virtual void AddCustomComponent(ose::entity::Entity & entity, ose::entity::CustomComponent & comp) = 0;

		virtual void Init() {}
		virtual void Update() {}

		// If name and factory are given, links the engine name to the engine factory
		// Returns the map from engine name to engine factory
		static std::unordered_map<std::string, CustomEngineFactory> const & GetSetCustomEngineFactory(std::string const & name = "", CustomEngineFactory factory = nullptr);
	};
}