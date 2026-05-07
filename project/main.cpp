#include <GL/glew.h>
#include <cstdlib>
#include <chrono>

#include <labhelper.h>
#include <imgui.h>

#include <perf.h>

#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
using namespace glm;

#include <Model.h>
#include "hdr.h"
#include "fbo.h"

#include "ply.h"



///////////////////////////////////////////////////////////////////////////////
// Various globals
///////////////////////////////////////////////////////////////////////////////
SDL_Window* g_window = nullptr;
float currentTime = 0.0f;
float previousTime = 0.0f;
float deltaTime = 0.0f;
int windowWidth, windowHeight;

// Mouse input
ivec2 g_prevMouseCoords = { -1, -1 };
bool g_isMouseDragging = false;


///////////////////////////////////////////////////////////////////////////////
// Shader programs
///////////////////////////////////////////////////////////////////////////////
GLuint shaderProgram;       // Shader for rendering the final image
GLuint backgroundProgram;

GLuint splatProgram;       // Shader for rendering the final image

///////////////////////////////////////////////////////////////////////////////
// Environment
///////////////////////////////////////////////////////////////////////////////
float environment_multiplier = 1.5f;
GLuint environmentMap;
const std::string envmap_base_name = "001";


///////////////////////////////////////////////////////////////////////////////
// Camera parameters.
///////////////////////////////////////////////////////////////////////////////
vec3 cameraPosition(0.0f, 0.0f, 3.0f);
vec3 cameraDirection = normalize(vec3(0.0f) - cameraPosition);
float cameraSpeed = 10.f;

vec3 worldUp(0.0f, 1.0f, 0.0f);


///////////////////////////////////////////////////////////////////////////////
// Gaussian
///////////////////////////////////////////////////////////////////////////////
GLuint gaussianVAO;
GLuint gaussianVBO;
PLYModel gaussianModel;
GLsizei gaussianCount = 0;

std::vector<GaussianVertex> testGaussians = {
    // center, bright red, large round
    { {0.0f, 0.0f, 0.0f}, {1.0f, 0.1f, 0.1f}, 0.9f, {2.0f, 2.0f, 2.0f}, {1,0,0,0} },
    // offset right, green, stretched on X
    { {5.0f, 0.0f, 0.0f}, {0.1f, 1.0f, 0.1f}, 0.9f, {4.0f, 0.5f, 0.5f}, {1,0,0,0} },
    // offset up, blue, stretched on Y
    { {0.0f, 5.0f, 0.0f}, {0.1f, 0.1f, 1.0f}, 0.9f, {0.5f, 4.0f, 0.5f}, {1,0,0,0} },
    // offset forward, yellow, 45 degree rotation around Z
    { {0.0f, 0.0f, 5.0f}, {1.0f, 1.0f, 0.1f}, 0.9f, {3.0f, 0.5f, 0.5f},
      {0.9239f, 0.0f, 0.0f, 0.3827f} }, // 45deg around Z as (w,x,y,z)
};





void loadShaders(bool is_reload)
{
	GLuint shader = labhelper::loadShaderProgram("../project/background.vert", "../project/background.frag", is_reload);
	if(shader != 0)
	{
		backgroundProgram = shader;
	}

	shader = labhelper::loadShaderProgram("../project/shading.vert", "../project/shading.frag", is_reload);
	if(shader != 0)
	{
		shaderProgram = shader;
	}

	shader = labhelper::loadShaderProgram("../project/gSplat.vert", "../project/gSplat.frag", "../project/gSplat.geom", is_reload);
	if(shader != 0)
	{
		splatProgram = shader;
	}
}


///////////////////////////////////////////////////////////////////////////////
/// This function is called once at the start of the program and never again
///////////////////////////////////////////////////////////////////////////////
void initialize()
{
	ENSURE_INITIALIZE_ONLY_ONCE();


	///////////////////////////////////////////////////////////////////////
	//		Load Shaders
	///////////////////////////////////////////////////////////////////////
	loadShaders(false);

	///////////////////////////////////////////////////////////////////////
	// Load models and set up model matrices
	///////////////////////////////////////////////////////////////////////
    gaussianModel = loadPLY("../scenes/ply/point_cloud.ply");

	///////////////////////////////////////////////////////////////////////
	// Load environment map
	///////////////////////////////////////////////////////////////////////
	environmentMap = labhelper::loadHdrTexture("../scenes/envmaps/" + envmap_base_name + ".hdr");


	///////////////////////////////////////////////////////////////////////
    // Upload Gaussian model
	///////////////////////////////////////////////////////////////////////

    // Upload
    glGenVertexArrays(1, &gaussianVAO);
    glGenBuffers(1, &gaussianVBO);

    glBindVertexArray(gaussianVAO);
    glBindBuffer(GL_ARRAY_BUFFER, gaussianVBO);
    glBufferData(GL_ARRAY_BUFFER,
        gaussianModel.gaussians.size() * sizeof(GaussianVertex),
        gaussianModel.gaussians.data(),
        GL_STATIC_DRAW);

    // glBufferData(GL_ARRAY_BUFFER,
    //         testGaussians.size() * sizeof(GaussianVertex),
    //         testGaussians.data(),
    //         GL_STATIC_DRAW);

    // pos: location 0, 3 floats
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(GaussianVertex),
                          (void*)offsetof(GaussianVertex, pos));

    // color: location 1, 3 floats
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(GaussianVertex),
                          (void*)offsetof(GaussianVertex, color));

    // opacity: location 2, 1 float
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, sizeof(GaussianVertex),
                          (void*)offsetof(GaussianVertex, opacity));

    // scale: location 3, 3 floats
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(GaussianVertex),
                          (void*)offsetof(GaussianVertex, scale));

    // rot: location 4, 4 floats
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, sizeof(GaussianVertex),
                          (void*)offsetof(GaussianVertex, rot));

    glBindVertexArray(0);


    gaussianCount = (GLsizei)gaussianModel.gaussians.size();
    gaussianModel.gaussians = std::vector<GaussianVertex>(); // Free memory

	glEnable(GL_DEPTH_TEST); // enable Z-buffering

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glEnable(GL_PROGRAM_POINT_SIZE); // Point rendering
}


void drawBackground(const mat4& viewMatrix, const mat4& projectionMatrix)
{
	glUseProgram(backgroundProgram);
	labhelper::setUniformSlow(backgroundProgram, "environment_multiplier", environment_multiplier);
	labhelper::setUniformSlow(backgroundProgram, "inv_PV", inverse(projectionMatrix * viewMatrix));
	labhelper::setUniformSlow(backgroundProgram, "camera_pos", cameraPosition);
	labhelper::drawFullScreenQuad();
}


///////////////////////////////////////////////////////////////////////////////
/// This function will be called once per frame, so the code to set up
/// the scene for rendering should go here
///////////////////////////////////////////////////////////////////////////////
void display(void)
{
	labhelper::perf::Scope s( "Display" );

	///////////////////////////////////////////////////////////////////////////
	// Check if window size has changed and resize buffers as needed
	///////////////////////////////////////////////////////////////////////////
	{
		int w, h;
		SDL_GetWindowSize(g_window, &w, &h);
		if(w != windowWidth || h != windowHeight)
		{
			windowWidth = w;
			windowHeight = h;
		}
	}

	///////////////////////////////////////////////////////////////////////////
	// setup matrices
	///////////////////////////////////////////////////////////////////////////
	mat4 projMatrix = perspective(radians(45.0f), float(windowWidth) / float(windowHeight), 5.0f, 2000.0f);
	mat4 viewMatrix = lookAt(cameraPosition, cameraPosition + cameraDirection, worldUp);

	///////////////////////////////////////////////////////////////////////////
	// Bind the environment map(s) to unused texture units
	///////////////////////////////////////////////////////////////////////////
	glActiveTexture(GL_TEXTURE6);
	glBindTexture(GL_TEXTURE_2D, environmentMap);
	glActiveTexture(GL_TEXTURE0);


	///////////////////////////////////////////////////////////////////////////
	// Draw from camera
	///////////////////////////////////////////////////////////////////////////
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(0, 0, windowWidth, windowHeight);
	glClearColor(0.2f, 0.2f, 0.8f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	{
		labhelper::perf::Scope s( "Background" );
		drawBackground(viewMatrix, projMatrix);
	}


	///////////////////////////////////////////////////////////////////////////
	// Draw Gaussians
	///////////////////////////////////////////////////////////////////////////
	glUseProgram(splatProgram);
    
	labhelper::setUniformSlow(splatProgram, "projectionMatrix", projMatrix);
	labhelper::setUniformSlow(splatProgram, "viewMatrix", viewMatrix);
	labhelper::setUniformSlow(splatProgram, "viewportWidth", windowWidth);
	labhelper::setUniformSlow(splatProgram, "viewportHeight", windowHeight);

    glDepthMask(GL_FALSE);
    glBindVertexArray(gaussianVAO);

    glDrawArrays(GL_POINTS, 0, gaussianCount);

    glDepthMask(GL_TRUE);

}


///////////////////////////////////////////////////////////////////////////////
/// This function is used to update the scene according to user input
///////////////////////////////////////////////////////////////////////////////
bool handleEvents(void)
{
	// check events (keyboard among other)
	SDL_Event event;
	bool quitEvent = false;
	while(SDL_PollEvent(&event))
	{
		labhelper::processEvent( &event );

		if(event.type == SDL_QUIT || (event.type == SDL_KEYUP && event.key.keysym.sym == SDLK_ESCAPE))
		{
			quitEvent = true;
		}
		if(event.type == SDL_KEYUP && event.key.keysym.sym == SDLK_g)
		{
			if ( labhelper::isGUIvisible() )
			{
				labhelper::hideGUI();
			}
			else
			{
				labhelper::showGUI();
			}
		}
		if(event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT
		   && (!labhelper::isGUIvisible() || !ImGui::GetIO().WantCaptureMouse))
		{
			g_isMouseDragging = true;
			int x;
			int y;
			SDL_GetMouseState(&x, &y);
			g_prevMouseCoords.x = x;
			g_prevMouseCoords.y = y;
		}

		if(!(SDL_GetMouseState(NULL, NULL) & SDL_BUTTON(SDL_BUTTON_LEFT)))
		{
			g_isMouseDragging = false;
		}

		if(event.type == SDL_MOUSEMOTION && g_isMouseDragging)
		{
			// More info at https://wiki.libsdl.org/SDL_MouseMotionEvent
			int delta_x = event.motion.x - g_prevMouseCoords.x;
			int delta_y = event.motion.y - g_prevMouseCoords.y;
			float rotationSpeed = 0.1f;
			mat4 yaw = rotate(rotationSpeed * deltaTime * -delta_x, worldUp);
			mat4 pitch = rotate(rotationSpeed * deltaTime * -delta_y,
			                    normalize(cross(cameraDirection, worldUp)));
			cameraDirection = vec3(pitch * yaw * vec4(cameraDirection, 0.0f));
			g_prevMouseCoords.x = event.motion.x;
			g_prevMouseCoords.y = event.motion.y;
		}
	}

	// check keyboard state (which keys are still pressed)
	const uint8_t* state = SDL_GetKeyboardState(nullptr);
	vec3 cameraRight = cross(cameraDirection, worldUp);

	if(state[SDL_SCANCODE_W])
	{
		cameraPosition += cameraSpeed * deltaTime * cameraDirection;
	}
	if(state[SDL_SCANCODE_S])
	{
		cameraPosition -= cameraSpeed * deltaTime * cameraDirection;
	}
	if(state[SDL_SCANCODE_A])
	{
		cameraPosition -= cameraSpeed * deltaTime * cameraRight;
	}
	if(state[SDL_SCANCODE_D])
	{
		cameraPosition += cameraSpeed * deltaTime * cameraRight;
	}
	if(state[SDL_SCANCODE_Q])
	{
		cameraPosition -= cameraSpeed * deltaTime * worldUp;
	}
	if(state[SDL_SCANCODE_E])
	{
		cameraPosition += cameraSpeed * deltaTime * worldUp;
	}
	return quitEvent;
}


///////////////////////////////////////////////////////////////////////////////
/// This function is to hold the general GUI logic
///////////////////////////////////////////////////////////////////////////////
void gui()
{
	// ----------------- Set variables --------------------------
	ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate,
	            ImGui::GetIO().Framerate);
	// ----------------------------------------------------------


	////////////////////////////////////////////////////////////////////////////////
	////////////////////////////////////////////////////////////////////////////////

	labhelper::perf::drawEventsWindow();
}

int main(int argc, char* argv[])
{
	g_window = labhelper::init_window_SDL("OpenGL Project");

	initialize();

	bool stopRendering = false;
	auto startTime = std::chrono::system_clock::now();

	while(!stopRendering)
	{
		//update currentTime
		std::chrono::duration<float> timeSinceStart = std::chrono::system_clock::now() - startTime;
		previousTime = currentTime;
		currentTime = timeSinceStart.count();
		deltaTime = currentTime - previousTime;

		// check events (keyboard among other)
		stopRendering = handleEvents();

		// Inform imgui of new frame
		labhelper::newFrame( g_window );

		// render to window
		display();

		// Render overlay GUI.
		gui();

		// Finish the frame and render the GUI
		labhelper::finishFrame();

		// Swap front and back buffer. This frame will now been displayed.
		SDL_GL_SwapWindow(g_window);
	}

	// Shut down everything. This includes the window and all other subsystems.
	labhelper::shutDown(g_window);
	return 0;
}
