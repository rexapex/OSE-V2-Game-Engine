#pragma once

#include "OSE-Core/Rendering/RenderingEngine.h"
#include "OSE-Core/EngineDependencies/glm/glm.hpp"
#include "RenderPoolGL.h"
#include "TextureGL.h"

namespace ose
{
	class Camera;
}

namespace ose::rendering
{
	class RenderingEngineGL final : public RenderingEngine
	{
	public:
		RenderingEngineGL(int fbwidth, int fbheight);
		~RenderingEngineGL();

		// Render one frame to the screen
		void Render(Camera const & active_camera) override;

		// Get a reference to the render pool, s.t. new render objects can be added
		RenderPool & GetRenderPool() override { return render_pool_; }
		
	private:
		// Load OpenGL functions using GLEW
		// Return of 0 = success, return of -1 = error
		static int InitGlew();

		// The projection matrix, can be a perspective or an orthographic projection matrix
		glm::mat4 projection_matrix_;

		// The pool of object rendered each engine update
		RenderPoolGL render_pool_;

		// Child functions to update the projection matrix to either orthographic or perspective
		void UpdateOrthographicProjectionMatrix(int fbwidth, int fbheight) override;
		void UpdatePerspectiveProjectionMatrix(float hfov_deg, int fbwidth, int fbheight, float znear, float zfar) override;
	};
}

