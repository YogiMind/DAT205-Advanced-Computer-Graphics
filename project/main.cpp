#include <GL/glew.h>
#include <algorithm>
#include <chrono>

#include <labhelper.h>
#include <imgui.h>

#include <numeric>
#include <perf.h>

#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
using namespace glm;


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
GLuint splatProgram; // Shader program 


///////////////////////////////////////////////////////////////////////////////
// Camera parameters.
///////////////////////////////////////////////////////////////////////////////
vec3 cameraPosition(2.0f, 0.0f, 5.0f);
vec3 cameraDirection = normalize(vec3(0.0f) - cameraPosition);
float cameraSpeed = 4.f;

vec3 worldUp(0.0f, 1.0f, 0.0f);


///////////////////////////////////////////////////////////////////////////////
// Gaussian
///////////////////////////////////////////////////////////////////////////////
GLuint gaussianVAO;
GLuint gaussianVBO;
GLsizei gaussianCount = 0;

std::vector<glm::vec3> gaussianPositions;
GLuint gaussianEBO; // element buffer
std::vector<uint32_t> sortedIndices;


void loadShaders(bool is_reload)
{
	GLuint shader = labhelper::loadShaderProgram("../project/gSplat.vert", "../project/gSplat.frag", "../project/gSplat.geom", is_reload);
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
    PLYModel gaussianModel;
    gaussianModel = loadPLY("../scenes/ply/point_cloud.ply");


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



    gaussianCount = (GLsizei)gaussianModel.gaussians.size();
    gaussianPositions.resize(gaussianCount);
    for (int i = 0; i < gaussianCount; i++) {
        gaussianPositions[i] = glm::vec3(
                gaussianModel.gaussians[i].pos[0],
                gaussianModel.gaussians[i].pos[1],
                gaussianModel.gaussians[i].pos[2]);
    }

    sortedIndices.resize(gaussianCount);
    std::iota(sortedIndices.begin(), sortedIndices.end(), 0);

    glGenBuffers(1, &gaussianEBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gaussianEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
            sortedIndices.size() * sizeof(uint32_t),
            sortedIndices.data(),
            GL_DYNAMIC_DRAW); // dynamic since we update each frame


    glBindVertexArray(0);

	// glEnable(GL_DEPTH_TEST); // enable Z-buffering

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glEnable(GL_PROGRAM_POINT_SIZE); // Point rendering
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
	mat4 projMatrix = perspective(radians(45.0f), float(windowWidth) / float(windowHeight), 1.f, 100.0f);
	mat4 viewMatrix = lookAt(cameraPosition, cameraPosition + cameraDirection, worldUp);

	///////////////////////////////////////////////////////////////////////////
	// Draw from camera
	///////////////////////////////////////////////////////////////////////////

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(0, 0, windowWidth, windowHeight);
	glClearColor(0.f, 0.f, 0.f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	//
	// {
	// 	labhelper::perf::Scope s( "Background" );
	// 	drawBackground(viewMatrix, projMatrix);
	// }


	///////////////////////////////////////////////////////////////////////////
	// Draw Gaussians
	///////////////////////////////////////////////////////////////////////////
	glUseProgram(splatProgram);
    
	labhelper::setUniformSlow(splatProgram, "projectionMatrix", projMatrix);
	labhelper::setUniformSlow(splatProgram, "viewMatrix", viewMatrix);
	labhelper::setUniformSlow(splatProgram, "viewportWidth", windowWidth);
	labhelper::setUniformSlow(splatProgram, "viewportHeight", windowHeight);

    // glDepthMask(GL_FALSE);

    // Re-upload sorted indices
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gaussianEBO);

    static int frameCount = 0;

    // Sort back to front
    if ((frameCount) % 300 == 1) {
        std::sort(sortedIndices.begin(), sortedIndices.end(), [&](uint32_t a, uint32_t b) {
                float da = glm::dot(gaussianPositions[a] - cameraPosition, cameraDirection);
                float db = glm::dot(gaussianPositions[b] - cameraPosition, cameraDirection);
                return da > db; // further first
                });


        glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0,
                sortedIndices.size() * sizeof(uint32_t),
                sortedIndices.data());
    }
    frameCount++;

    // Draw using indices instead of glDrawArrays
    glBindVertexArray(gaussianVAO);
    glDrawElements(GL_POINTS, gaussianCount, GL_UNSIGNED_INT, 0);

    // glBindVertexArray(gaussianVAO);
    //
    // glDrawArrays(GL_POINTS, 0, gaussianCount);

    // glDepthMask(GL_TRUE);

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
