#include "stdafx.h"
#include "Game.h"

namespace ose::game
{
	Game::Game()
	{
		this->project_loader_ = std::make_unique<ProjectLoaderImpl>();
		this->scene_switch_mode_ = ESceneSwitchMode::REMOVE_ALL_ON_SWITCH;
		this->running_ = false;
		this->thread_manager_ = std::make_unique<ThreadManager>();
		
		this->window_manager_ = std::make_unique<WindowManagerImpl>();
		this->window_manager_->createWindow(1);
		int fbwidth { this->window_manager_->getFramebufferWidth() };
		int fbheight { this->window_manager_->getFramebufferHeight() };

		this->rendering_engine_ = std::make_unique<RenderingEngineImpl>();
		this->window_manager_->setEngineReferences(this->rendering_engine_.get());
		this->rendering_engine_->set_projection_mode_and_fbsize(EProjectionMode::ORTHOGRAPHIC, fbwidth, fbheight);

		this->render_pool_ = std::make_unique<RenderPool>();

		this->time_.init(this->window_manager_->getTimeSeconds());
	}


	Game::~Game() noexcept {}


	Game::Game(Game && other) noexcept : project_(std::move(other.project_)), project_loader_(std::move(other.project_loader_)),
										 active_scene_(std::move(other.active_scene_)), running_(other.running_),
										 thread_manager_(std::move(other.thread_manager_)), persistent_entities_(std::move(other.persistent_entities_)),
									     render_pool_(std::move(other.render_pool_)) {}


	Game & Game::operator=(Game && other) noexcept
	{
		this->project_ = std::move(other.project_);
		this->project_loader_ = std::move(other.project_loader_);
		this->active_scene_ = std::move(other.active_scene_);
		this->running_ = other.running_;
		this->thread_manager_ = std::move(other.thread_manager_);
		this->persistent_entities_ = std::move(other.persistent_entities_);
		this->render_pool_ = std::move(other.render_pool_);
		return *this;
	}


	// loads the project specified (does not load any scenes)
	// throws std::exception if the project could not be loaded
	// throws std::invalid_argument if the project FILE_FORMAT decleration file does not exist
	void Game::loadProject(const std::string & proj_name)
	{
		// before project can be loaded, the file format of the project files must de determined
		std::string proj_file_format = ProjectLoader::getProjectFileFormat(proj_name);

		if(proj_file_format == "XML") {
			this->project_ = this->project_loader_->loadProject(proj_name);

			if(this->project_ == nullptr) {
				throw std::exception("Error: Could not load Project");
			}
		} else {
			throw std::exception("Error: Unknown project file type");
		}
		//TODO - remove test (works)
		//resource_manager_->importFile("D:/James/Documents/Resources/2D Game Resources/rock.png", "sub");
		//resource_manager_->addTexture("rock.png");
		//resource_manager_->addTexture("sub/rock.png", "rock2.png");
	}


	// loads the scene into memory
	// throws std::exception if no project has been loaded
	// throws std::invalid_argument exception if the scene does exist
	// throws std::exception if the scene could not be loaded
	void Game::loadScene(const std::string & scene_name)
	{
		if(project_ == nullptr) {
			throw std::exception("Error: A Project must be loaded before a Scene can be loaded");
		}

		// first, check that the scene actually exists
		auto map = project_->get_scene_names_to_path_map();
		auto pos = map.find(scene_name);
		if(pos == map.end()) {
			throw std::invalid_argument("Error: The Scene " + scene_name + " does not exist in the current Project");
		}

		// load the scene using the project loader
		auto scene = this->project_loader_->loadScene(*project_, scene_name);

		// scene pointer will be nullptr if no scene exists with name scene_name or the scene file failed to load
		if(scene != nullptr)
		{
			scene->print();

			auto index = this->loaded_scenes_.find(scene_name);
		
			if(index == loaded_scenes_.end())
			{
				loaded_scenes_.emplace(scene_name, std::move(scene));
				//scene unique_ptr is no longer usable since its pointer has been moved
			}
			else
			{
				throw std::invalid_argument("Error: scene " + scene_name + " is already loaded");
			}
		}
		else
		{
			throw std::exception("Error: Failed to load scene");
		}
	}


	void Game::unloadScene(const std::string & scene_name)
	{
		auto iter = this->loaded_scenes_.find(scene_name);

		if(iter == loaded_scenes_.end())
		{
			throw std::invalid_argument("Error: scene " + scene_name + " is NOT loaded");
		}
		else
		{
			this->loaded_scenes_.erase(iter);
		}
	}


	void Game::unloadAllLoadedScenes()
	{
		this->loaded_scenes_.clear();		//easy... I hope
	}


	void Game::setActiveScene(const std::string & scene_name)
	{
		auto iter = this->loaded_scenes_.find(scene_name);

		if(iter == loaded_scenes_.end())
		{
			throw std::invalid_argument("Error: scene " + scene_name + " is NOT loaded");
		}
		else
		{
			std::cerr << "Setting active scene to " << scene_name << std::endl;

			auto new_scene = std::move(iter->second);		//move scene ptr to new_scene pointer
			this->loaded_scenes_.erase(iter);				//then remove the entry in the map

			//decide what to do about other scenes
			switch(this->scene_switch_mode_)
			{
				case ESceneSwitchMode::REMOVE_ALL_ON_SWITCH:
				{
					//active scene will be auto removed from active_scene_ since using unique_ptr
					this->loaded_scenes_.clear();
					break;
				}
				case ESceneSwitchMode::REMOVE_LOADED_ON_SWITCH:
				{
					//remove all loaded scene then add the active scene to the loaded scenes list
					this->loaded_scenes_.clear();
					this->loaded_scenes_.emplace(active_scene_->get_name(), std::move(active_scene_));
					break;
				}
				case ESceneSwitchMode::REMOVE_NONE_ON_SWITCH:
				{
					//add the active scene to the loaded scenes list so all other scenes are now in loaded list
					this->loaded_scenes_.emplace(active_scene_->get_name(), std::move(active_scene_));
					break;
				}
				//no case neeeded for -> ESceneSwitchMode::REMOVE_ACTIVE_ON_SWITCH
				//as active_scene_ is auto deleted when new scene is made active
			}

			this->active_scene_ = std::move(new_scene);		//finally, move the new_scene to the active_scene pointer
		}



		// IMPORTANT - the following code can only be run on the same thread as the render context

		// create GPU memory for the new resources
		this->project_->get_resource_manager().createTextures();

		// create GPU memory for the new render objects
		for(auto const & entity : this->active_scene_->entities().get())
		{
			initEntity(*entity);
		}
	}

	void Game::startGame()
	{
		if(!running_)
		{
			running_ = true;
			runGame();
		}
		else
		{
			LOG("Error: cannot start game, game is already running");
		}
	}

	void Game::runGame()
	{
		// get current time in seconds
		// TODO - should I use this or window_manager_ timing
		// time_t t = time(0);

		while(running_)
		{
			// renders previous frame to window and poll for new event
			window_manager_->update();

			// update all timing variables
			time_.update(window_manager_->getTimeSeconds());

			// render the game
			RenderObjectImpl * render_object = nullptr;
			while((render_object = render_pool_->getNextDataObject()) != nullptr)
			{
				rendering_engine_->update(*render_object);
			}
		}
	}

	// initialise components of an entity along with its sub-entities
	void Game::initEntity(const Entity & entity)
	{
		// for each sprite renderer component of the entity		
		for(auto const & comp : entity.getComponents<SpriteRenderer>())
		{
			// initialise the component
			comp->init();
			DEBUG_LOG("Initialised SpriteRenderer");

			// then add the component to the render pool
			render_pool_->addEngineDataObject(static_cast<RenderObjectImpl *>(comp->get_render_object()));
		}

		// initialise all sub entities
		for(auto const & sub_entity : entity.sub_entities().get())
		{
			initEntity(*sub_entity);
		}
	}
}

