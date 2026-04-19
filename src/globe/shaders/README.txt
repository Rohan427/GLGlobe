To implement an interactive 3D globe in your SDL-ROCm framework, you need to use GLSL shaders that transform a sphere mesh and map your ROCm-generated texture onto it.

1. GLSL Shader Pair
This set handles the 3D rotation, zooming, and solid-color rendering for your continents.

Vertex Shader (globe.vert)
This shader applies the Model-View-Projection (MVP) matrix to position the sphere and passes texture coordinates to the fragment shader.

Fragment Shader (globe.frag)
This shader samples your ROCm-generated texture to color the globe. 
