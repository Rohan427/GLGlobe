Converting Natural Earth map to RGBA raw binary:

sudo dnf install gdal

gdal_translate -of ENVI -ot Byte -expand rgba input_map.tif output_map.bin

Command Breakdown:
-of ENVI: Outputs a headerless, flat binary file (a .hdr file will also be created, which you can safely delete).

-ot Byte: Ensures the output uses 8-bit (1-byte) unsigned integers per channel.

-expand rgba: Converts indexed or grayscale maps into full 4-channel (Red, Green, Blue, Alpha) data.

# Example for a 2048x1024 raw map
gdal_translate -of ENVI -ot Byte -expand rgba -outsize 2048 1024 input_map.tif output_map.bin

The data will be ordered as R, G, B, A, R, G, B, A... across the entire image.
Byte Order: Natural Earth images often use Macintosh byte order, but gdal_translate generally handles this during conversion to standard raw bytes.
Memory Size: For a 2048x1024 RGBA image, your buffer size must be 2048x1024x4 = 8,388,608 bytes.


Handling the GDAL Output
The gdal_translate -of ENVI -expand rgba command defaults to BIP (Band Interleaved by Pixel) format. This means your raw binary file is perfectly arranged as R0 G0 B0 A0, R1 G1 B1 A1..., which matches the hipChannelFormatDesc(8, 8, 8, 8, ...) used.

4. Implementation Check on RHEL 10
Memory Management: Since you are using hipLaunchKernelGGL, ensure you synchronize with hipStreamSynchronize before updating your OpenGL texture to avoid race conditions.

Vectorization: The float4 read is a vectorized operation, which is significantly faster than reading four separate unsigned char values from global memory.
