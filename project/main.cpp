#include <GL/glew.h>
#include <algorithm>
#include <chrono>
#include <array>

#include <labhelper.h>
#include <imgui.h>

#include <perf.h>

#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
#include <vector>
using namespace glm;


#include "ply.h"
#include <omp.h>



///////////////////////////////////////////////////////////////////////////////
// Various globals
///////////////////////////////////////////////////////////////////////////////
SDL_Window* g_window = nullptr;
float currentTime = 0.0f;
float previousTime = 0.0f;
float deltaTime = 0.0f;
int windowWidth, windowHeight;

static float sortTimeMs = 0.0f;
static float drawTimeMs = 0.0f;

// Mouse input
ivec2 g_prevMouseCoords = { -1, -1 };
bool g_isMouseDragging = false;

// frustum culling
bool enableFrustumCulling = true;

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
GLsizei visibleCount = 0;

GLuint gaussianEBO; // element buffer
std::vector<uint32_t> visibleIndices;
std::vector<glm::vec3> gaussianPositions;

GLuint shRestBuffer;  // the buffer
GLuint shRestTex;     // the texture handle
int sh_degree = 3;



int frustumCull(mat4 viewProjMatrix) {
    int count = 0;

    #pragma omp parallel for schedule(static)
    for (int i = 0; i < gaussianCount; i++) {
        vec4 clipPos = viewProjMatrix * vec4(gaussianPositions[i], 1);

        if (clipPos.w <= 0.0f) continue;
        float invW = 1.0 / clipPos.w;
        float x = clipPos.x * invW;
        float y = clipPos.y * invW;
        float z = clipPos.z * invW;
        float margin = 1.2f; // small margin to avoid popping at edges. Maybe find size with cov matrix?

        if (x < -margin || x > margin
         || y < -margin || y > margin
         || z < -1.0f || z > 1.0f)
            continue;

        int idx;
        #pragma omp atomic capture
        idx = count++;

        visibleIndices[idx] = i;
    }

    return count;
}

void radixSortGaussians(const glm::vec3& camPos, const glm::vec3& camDir, int n) {
    int numThreads = omp_get_max_threads();

    std::vector<float>    depths(n);
    std::vector<uint32_t> keys(n);
    std::vector<uint32_t> tempIndices(n);
    std::vector<std::array<uint32_t, 256>> threadCounts(numThreads);

    // Step 1: compute depths
    #pragma omp parallel for schedule(static)
    for (int i = 0; i < n; i++)
        depths[i] = glm::dot(gaussianPositions[visibleIndices[i]] - camPos, camDir);

    // Step 2: float -> sortable descending uint32 key
    #pragma omp parallel for schedule(static)
    for (int i = 0; i < n; i++) {
        uint32_t bits;
        memcpy(&bits, &depths[i], sizeof(float));
        keys[i] = ~((bits & 0x80000000) ? ~bits : (bits ^ 0x80000000));
    }

    // Step 3: 4-pass radix sort
    std::vector<uint32_t> keyTemp(n);
    for (int pass = 0; pass < 4; pass++) {
        int shift = pass * 8;

        // Count: each thread counts its own chunk
        #pragma omp parallel
        {
            int tid = omp_get_thread_num();
            threadCounts[tid].fill(0);
            #pragma omp for schedule(static)
            for (int i = 0; i < n; i++)
                threadCounts[tid][(keys[i] >> shift) & 0xFF]++;
        }

        // Prefix sum: compute where each thread's chunk starts per bucket
        // threadCounts[t][b] becomes the write offset for thread t, bucket b
        uint32_t running = 0;
        for (int b = 0; b < 256; b++) {
            for (int t = 0; t < numThreads; t++) {
                uint32_t cnt = threadCounts[t][b];
                threadCounts[t][b] = running; // thread t writes bucket b starting here
                running += cnt;
            }
        }

        // Scatter: each thread writes its own elements using its pre-computed offsets
        #pragma omp parallel
        {
            int tid = omp_get_thread_num();
            std::array<uint32_t, 256> offsets = threadCounts[tid]; // private copy

            #pragma omp for schedule(static) nowait
            for (int i = 0; i < n; i++) {
                uint8_t bucket = (keys[i] >> shift) & 0xFF;
                uint32_t dst = offsets[bucket]++;
                tempIndices[dst] = visibleIndices[i];
                keyTemp[dst] = keys[i];
            }
        }

        std::swap(visibleIndices, tempIndices);
        std::swap(keys, keyTemp);
    }
}



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
    gaussianModel = loadPLY("../scenes/ply/truck_scene.ply");
    // gaussianModel = loadPLY("../scenes/ply/3DGS_PLY_sample_data/PLY(postshot)/cactus_splat3_11kSteps_1.5M_splats.ply");
    // gaussianModel = loadPLY("../scenes/ply/drift_scene.ply");
    // gaussianModel = loadPLY("../scenes/ply/tree_scene.ply");
    // gaussianModel = loadPLY("../scenes/ply/iron_age_roundhouse_scene.ply");


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
                          (void*)offsetof(GaussianVertex, x));

    // color: location 1, 3 floats
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(GaussianVertex),
                          (void*)offsetof(GaussianVertex, f_dc));

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

    // After uploading the VBO:
    // Pack f_rest into a flat buffer: [g0_rest0..g0_rest44, g1_rest0..g1_rest44, ...]
    gaussianCount = (GLsizei)gaussianModel.gaussians.size();
    std::vector<float> restData(gaussianCount * 45);
    for (int i = 0; i < gaussianCount; i++) {
        for (int j = 0; j < 45; j++)
            restData[i * 45 + j] = gaussianModel.gaussians[i].f_rest[j];
    }

    glGenBuffers(1, &shRestBuffer);
    glBindBuffer(GL_TEXTURE_BUFFER, shRestBuffer);
    glBufferData(GL_TEXTURE_BUFFER, restData.size() * sizeof(float), restData.data(), GL_STATIC_DRAW);

    glGenTextures(1, &shRestTex);
    glBindTexture(GL_TEXTURE_BUFFER, shRestTex);
    glTexBuffer(GL_TEXTURE_BUFFER, GL_R32F, shRestBuffer); // one float per texel


    gaussianPositions.resize(gaussianCount);
    for (int i = 0; i < gaussianCount; i++) {
        gaussianPositions[i] = glm::vec3(
                gaussianModel.gaussians[i].x,
                gaussianModel.gaussians[i].y,
                gaussianModel.gaussians[i].z);
    }

    visibleIndices.resize(gaussianCount);

    glGenBuffers(1, &gaussianEBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gaussianEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
            gaussianCount * sizeof(uint32_t),
            visibleIndices.data(),
            GL_DYNAMIC_DRAW); // dynamic since we update each frame


    glBindVertexArray(0);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
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
	mat4 projMatrix = perspective(radians(45.0f), float(windowWidth) / float(windowHeight), 1.f, 1000.0f);
	mat4 viewMatrix = lookAt(cameraPosition, cameraPosition + cameraDirection, worldUp);
    mat4 viewProjMatrix = projMatrix * viewMatrix;

	///////////////////////////////////////////////////////////////////////////
	// Draw from camera
	///////////////////////////////////////////////////////////////////////////

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(0, 0, windowWidth, windowHeight);
	glClearColor(0.f, 0.f, 0.f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	///////////////////////////////////////////////////////////////////////////
	// Draw Gaussians
	///////////////////////////////////////////////////////////////////////////

	glUseProgram(splatProgram);
    
	labhelper::setUniformSlow(splatProgram, "projectionMatrix", projMatrix);
	labhelper::setUniformSlow(splatProgram, "viewMatrix", viewMatrix);
	labhelper::setUniformSlow(splatProgram, "viewportWidth", windowWidth);
	labhelper::setUniformSlow(splatProgram, "viewportHeight", windowHeight);
	labhelper::setUniformSlow(splatProgram, "cameraPos", cameraPosition);
	labhelper::setUniformSlow(splatProgram, "sh_degree", sh_degree);


    // Create list of gaussian indices to be rendered
    if (enableFrustumCulling) {
        visibleCount = frustumCull(viewProjMatrix);

    } else {

        visibleCount = gaussianCount;

        #pragma omp parallel for schedule(static)
        for (int i = 0; i < gaussianCount; i++) {
            visibleIndices[i] = i;
        }
    }

    // Sort gaussiand back-to-front
    auto t0 = std::chrono::high_resolution_clock::now();
    radixSortGaussians(cameraPosition, cameraDirection, visibleCount);
    auto t1 = std::chrono::high_resolution_clock::now();
    sortTimeMs = std::chrono::duration<float, std::milli>(t1 - t0).count();

    // Upload sorted list of indices
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gaussianEBO);
    glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0,
            visibleCount * sizeof(uint32_t),
            visibleIndices.data());


    // Load f_rest to texture buffer
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_BUFFER, shRestTex);
    labhelper::setUniformSlow(splatProgram, "shRestData", 0); // texture unit 0

    // Draw using indices
    glBindVertexArray(gaussianVAO);

    t0 = std::chrono::high_resolution_clock::now();
    glDrawElements(GL_POINTS, visibleCount, GL_UNSIGNED_INT, 0);
    t1 = std::chrono::high_resolution_clock::now();
    drawTimeMs = std::chrono::duration<float, std::milli>(t1 - t0).count();

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
	// // ----------------- Set variables --------------------------
	// ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate,
	//             ImGui::GetIO().Framerate);
	// // ----------------------------------------------------------
    ImGui::Begin("Stats");
    
    ImGui::Text("FPS: %.1f (%.2f ms/frame)",
        ImGui::GetIO().Framerate,
        1000.0f / ImGui::GetIO().Framerate);
    
    ImGui::Separator();
    ImGui::Text("Gaussians total:   %d", gaussianCount);
    ImGui::Text("Gaussians visible: %d (%.1f%%)", visibleCount, 100.0f * visibleCount / (float)gaussianCount);
    ImGui::Text("Sort time:         %.2f ms", sortTimeMs);
    ImGui::Text("Draw time:         %.2f ms", drawTimeMs);
    ImGui::Separator();
    ImGui::Text("Camera pos: (%.2f, %.2f, %.2f)",
        cameraPosition.x, cameraPosition.y, cameraPosition.z);

    ImGui::Checkbox("Frustum culling", &enableFrustumCulling);
    ImGui::SliderFloat("Camera speed", &cameraSpeed, 0.1f, 50.0f);
    ImGui::SliderInt("SH degree", &sh_degree, 0, 3);
    
    ImGui::End();
    
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
