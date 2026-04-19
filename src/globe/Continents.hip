/* Implementation Checkpoints
   Aspect Ratio: For our sphere UVs to look correct, ensure tex_width is exactly twice tex_height (e.g., 2048x1024).

   Pinned Memory: Use hipHostMalloc(&host_ptr, size, hipHostMallocMapped) to ensure the host_ptr passed to the kernel is
                  the same one used by your OpenGL glTexSubImage2D call.

   Performance: Since we aren't doing clouds or lights yet, this kernel is extremely fast. We can even vary the seed
                or time parameter to animate the continents shifting.

   Implementation on RHEL 10

   Permissions: Ensure your user is in the render group to access the HIP hardware texture units.

   Cleanup: Always destroy the texture object with hipDestroyTextureObject(texObj) and free the array with
            hipFreeArray(cuArray) when the application closes.

   HIP Kernel "Seam-Aware" Logic
   When your ROCm kernel reads from the Natural Earth binary mask, use the hipAddressModeWrap setting we defined in the
   Texture Object step. This ensures that when the kernel samples near the edge of your accurate data, it
   mathematically "sees" the other side of the map.

   Implementation Check on RHEL 10
   Coordinate System: Ensure our generateSphere function starts the U = 0 coordinate at the Prime Meridian
                      (0° Longitude). If our map data starts at -180°, simply offset our U coordinate by 0.5 in the
                      shader.

   Performance: Since we're using HIP Texture Objects and OpenGL PBOs, the transition across the seam will be handled
                entirely by the GPU's hardware texture units, costing zero extra CPU cycles.

*/
#include "sdl-imgui.hxx"
#include <hip/hip_runtime.h>
#include <math.h>
#include <hip/hip_vector_types.h>
#include <fstream>
#include <iostream>

// Scalar lerp for floats
__device__ inline float lerp(float a, float b, float w)
{
    return a + w * (b - a);
}

// Vector lerp for float3
__device__ inline float3 lerp (float3 a, float3 b, float w)
{
    return make_float3 (
                        lerp (a.x, b.x, w),
                        lerp (a.y, b.y, w),
                        lerp (a.z, b.z, w)
                       );
}

// Define dot product for float3 in ROCm/HIP
__device__ inline float dot (float3 a, float3 b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

// Define fract (fractional part)
__device__ inline float fract (float x)
{
    return x - floorf (x);
}

// Simple hash for noise generation
__device__ float hash (float3 p)
{
    p = make_float3 (dot (p, make_float3 (127.1, 311.7, 74.7)),
                     dot (p, make_float3 (269.5, 183.3, 246.1)),
                     dot (p, make_float3 (113.5, 271.9, 124.6))
                    );
    return fract (sin (p.x + p.y + p.z) * 43758.5453123);
}

// 3D Noise function for organic shapes
__device__ float noise (float3 p) 
{
    float3 i = make_float3 (floor (p.x), floor (p.y), floor(p.z));
    float3 f = make_float3 (p.x - i.x, p.y - i.y, p.z - i.z);
    f = make_float3 (f.x*f.x*(3.0-2.0*f.x), f.y*f.y*(3.0-2.0*f.y), f.z*f.z*(3.0-2.0*f.z));
    
    // Mix 8 corners of the cube
    return lerp (lerp (lerp (hash(i), hash (i + make_float3 (1,0,0)), f.x),
                 lerp (hash (i + make_float3 (0,1,0)), hash (i + make_float3 (1,1,0)), f.x), f.y),
                 lerp (lerp (hash (i + make_float3 (0,0,1)), hash (i + make_float3 (1,0,1)), f.x),
                 lerp (hash (i + make_float3 (0,1,1)), hash (i + make_float3 (1,1,1)), f.x), f.y),
                 f.z
                );
}

unsigned char* loadMapToPinned (const std::string& filename, size_t width, size_t height)
{
    size_t size = width * height; // Assuming 8-bit land/water mask
    unsigned char* pinned_ptr = nullptr;

    // Allocate pinned host memory mapped into GPU address space
    hipHostMalloc (&pinned_ptr, size, hipHostMallocMapped);

    std::ifstream file (filename, std::ios::binary);

    if (file.is_open())
    {
        file.read (reinterpret_cast<char*>(pinned_ptr), size);
        file.close();
    }
    else
    {
        std::cerr << "Failed to open map file!" << std::endl;
    }

    return pinned_ptr;
}

hipTextureObject_t createTexture (unsigned char* dev_ptr, int width, int height)
{
    // 1. Create a HIP Array (required for hardware filtering)
    hipArray_t cuArray;
    hipChannelFormatDesc channelDesc = hipCreateChannelDesc (8, 0, 0, 0, hipChannelFormatKindUnsigned);
    hipMallocArray (&cuArray, &channelDesc, width, height);
    hipMemcpy2DToArray (cuArray, 0, 0, dev_ptr, width, width, height, hipMemcpyDeviceToDevice);

    // 2. Resource Descriptor
    hipResourceDesc resDesc = {};
    resDesc.resType = hipResourceTypeArray;
    resDesc.res.array.array = cuArray;

    // 3. Texture Descriptor (ENABLES SMOOTHING)
    hipTextureDesc texDesc = {};
    texDesc.addressMode[0] = hipAddressModeWrap;   // Smooth wrapping across 0/360 lon
    texDesc.addressMode[1] = hipAddressModeClamp;  // No wrapping at poles
    texDesc.filterMode     = hipFilterModeLinear; // KEY: Hardware Bilinear Filtering
    texDesc.readMode       = hipReadModeNormalizedFloat; // Map 0-255 to 0.0-1.0
    texDesc.normalizedCoords = 1;                  // Use 0.0 to 1.0 UVs

    hipTextureObject_t texObj;
    hipCreateTextureObject (&texObj, &resDesc, &texDesc, nullptr);
    return texObj;
}

__global__ void render_smoothed_earth (unsigned char* output, hipTextureObject_t texObj, int width, int height) {
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;

    if (x < width && y < height)
    {
        // Calculate normalized UV coordinates (0.0 to 1.0)
        float u = (float)x / width;
        float v = (float)y / height;

        // Fetch smoothed land/water value from hardware texture units
        float land_val = tex2D<float> (texObj, u, v);

        int idx = (y * width + x) * 4;

        if (land_val > 0.5f)
        { // Smooth thresholding
            output[idx] = 34; output[idx+1] = 139; output[idx+2] = 34; // Forest Green
        }
        else
        {
            output[idx] = 0;  output[idx+1] = 105; output[idx+2] = 148; // Deep Blue
        }

        output[idx+3] = 255;
    }
}

extern "C" __global__ void render_rgba_earth (unsigned char* output, hipTextureObject_t texObj, int width, int height)
{
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;

    if (x < width && y < height)
    {
        // Calculate normalized UV coordinates
        float u = (float)x / width;
        float v = (float)y / height;

        // Hardware-accelerated fetch of all 4 channels (RGBA) at once
        float4 pixel = tex2D<float4>(texObj, u, v);

        // Simple example: Apply your solid-color continent logic
        // (Assuming Green channel identifies land in your specific map)
        int idx = (y * width + x) * 4;

        if (pixel.y > 0.5f)
        { // Thresholding on the Green channel
            output[idx] = 34; output[idx+1] = 139; output[idx+2] = 34; // Forest Green
        }
        else
        {
            output[idx] = 0;  output[idx+1] = 105; output[idx+2] = 148; // Deep Blue
        }

        output[idx+3] = 255;
    }
}

/* 'map_data' is a 2D array of the accurate world map loaded in GPU memory

    # Add a command to copy your binary map to the build directory
add_custom_command(TARGET MyGlobeApp POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/data/world_mask.bin $<TARGET_FILE_DIR:MyGlobeApp>/data/
)
*/
__global__ void render_accurate_earth (unsigned char* output, const unsigned char* map_data, int width, int height)
{
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;

    if (x < width && y < height)
    {
        int idx = y * width + x;
        
        // Sample the accurate map data
        unsigned char is_land = map_data[idx]; 

        int out_idx = idx * 4;

        if (is_land > 128)
        { // Threshold for land
            output[out_idx] = 34; output[out_idx+1] = 139; // Forest Green
        }
        else
        {
            output[out_idx] = 0; output[out_idx+1] = 105; // Deep Ocean
        }

        output[out_idx+3] = 255;
    }
}


extern "C" __global__ void generate_continents (unsigned char* ptr, int width, int height, float time)
{
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;

    if (x < width && y < height)
    {
        // 1. Convert pixel (x,y) to spherical UV (0 to 1)
        float u = (float)x / width;
        float v = (float)y / height;

        // 2. Convert to 3D Cartesian coordinates for seamless wrapping
        float theta = u * 2.0f * 3.14159f;
        float phi = v * 3.14159f;
        float3 p = make_float3 (sinf (phi) * cosf (theta), cosf (phi), sinf (phi) * sinf (theta));

        // 3. Layered noise (fBm) for continent detail
        float land_mask = 0.0f;
        float freq = 3.0f;
        float amp = 0.5f;

        for (int i=0; i<4; i++)
        {
            land_mask += noise (make_float3 (p.x*freq, p.y*freq, p.z*freq)) * amp;
            freq *= 2.0f;
            amp *= 0.5f;
        }

        // 4. Threshold to determine Land vs Water
        int idx = (y * width + x) * 4;

        if (land_mask > 0.45f)
        { 
            // Land (Solid Green)
            ptr[idx]   = 40;  ptr[idx+1] = 180; ptr[idx+2] = 40; ptr[idx+3] = 255;
        }
        else
        {
            // Water (Solid Blue)
            ptr[idx]   = 10;  ptr[idx+1] = 50;  ptr[idx+2] = 150; ptr[idx+3] = 255;
        }
    }
}

void launchProceduralKernel (int tex_width, int tex_height, unsigned char* host_ptr, time_t current_time)
{
	// 1. Set block and grid size (RHEL 10 / ROCm 7.2 standard is 256 threads/block)
	dim3 blockSize (16, 16);
	dim3 gridSize ((tex_width + blockSize.x - 1) / blockSize.x, 
                   (tex_height + blockSize.y - 1) / blockSize.y
                  );

	// 2. Launch (host_ptr is the ROCm-pinned memory)
	hipLaunchKernelGGL (generate_continents, gridSize, blockSize, 0, 0, 
                        host_ptr, tex_width, tex_height, current_time);
    hipError_t err = hipGetLastError();

    if (err != hipSuccess)
    {
        gConsole.addLog ("[ROCm ERROR] %s\n", hipGetErrorString (err));
    }

	// 3. Synchronize before the PBO/Texture update
	hipStreamSynchronize (0);
}

void LaunchEarthRGBAKernel (int tex_width, int tex_height, unsigned char* host_ptr)
{
	// 1. Describe the 4-channel 8-bit format (RGBA8888)
    hipArray_t cuArray;
	hipChannelFormatDesc channelDesc = hipCreateChannelDesc (8, 8, 8, 8, hipChannelFormatKindUnsigned);

	// 2. Resource descriptor for a 2D array
	hipResourceDesc resDesc = {};
	resDesc.resType = hipResourceTypeArray;
	resDesc.res.array.array = cuArray;

	// 3. Texture descriptor with linear filtering and normalized floats
	hipTextureDesc texDesc = {};
	texDesc.addressMode[0]   = hipAddressModeWrap;   // Longitude wrap
	texDesc.addressMode[1]   = hipAddressModeClamp;  // Latitude clamp
	texDesc.filterMode       = hipFilterModeLinear;
	texDesc.readMode         = hipReadModeNormalizedFloat; // Returns 0.0 to 1.0
	texDesc.normalizedCoords = 1;

    hipTextureObject_t texObj;
	hipCreateTextureObject (&texObj, &resDesc, &texDesc, nullptr);

	// 1. Set block and grid size (RHEL 10 / ROCm 7.2 standard is 256 threads/block)
    dim3 blockSize (16, 16);
    dim3 gridSize ((tex_width + blockSize.x - 1) / blockSize.x,
                   (tex_height + blockSize.y - 1) / blockSize.y
                  );

    // 2. Launch (host_ptr is the ROCm-pinned memory)
    hipLaunchKernelGGL (render_rgba_earth, gridSize, blockSize, 0, 0,
                        host_ptr, texObj, tex_width, tex_height);
    
    hipError_t err = hipGetLastError();

    if (err != hipSuccess)
    {
        gConsole.addLog ("[ROCm ERROR] %s\n", hipGetErrorString(err));
    }

    // 3. Synchronize before the PBO/Texture update
    hipStreamSynchronize (0);
}


