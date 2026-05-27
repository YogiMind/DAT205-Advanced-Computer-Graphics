# 3D Gaussian Splatting Renderer
Real-time 3D Gaussian Splatting renderer implemented in C++ and OpenGL, based on Kerbl et al. 2023. Implements the full forward rendering pass including screen-space covariance projection, view-dependent spherical harmonics, and depth-sorted alpha compositing.

## Implementation
Rendering pipeline

- Vertex → Geometry → Fragment shader pipeline for Gaussian rasterization
- Screen-space covariance projection via Jacobian approximation of the perspective transform
- Eigenvalue decomposition of the projected covariance for oriented bounding quads, minimizing overdraw
- Alpha blending with back-to-front depth sorting for correct transparency compositing
- View-dependent shading using degree-3 spherical harmonics (48 coefficients/Gaussian), uploaded via OpenGL texture buffer objects

## CPU processing

- Binary PLY parser with OpenMP-parallelized unpacking
- Parallel CPU radix sort (4-pass, 8-bit buckets) operating on depth-cached view-space projections
- Frustum culling before sort to reduce sort input size

## Performance

- Radix sort scales to ~1.5M Gaussians
- Frustum culling reduces sort input by typically 30–60% depending on scene
- ImGui overlay with live FPS, sort time, visible/total Gaussian counts, and runtime controls


## Tech Stack
C++ · OpenGL 4.2 · GLSL · OpenMP · GLM

<!-- ## Build -->
<!-- TODO -->

## References
Kerbl et al., 3D Gaussian Splatting for Real-Time Radiance Field Rendering, SIGGRAPH 2023.
