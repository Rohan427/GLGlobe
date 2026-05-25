#pragma once

#ifndef UTILITY_HXX
#define UTILITY_HXX

#include "Globe.hxx"
#include <QtMath>
#include <QVector3D>
#include <QString>
#include <QDateTime>
#include <QImageReader>
#include <ace/Log_Msg.h>

// Forward declare the proxy function inside the namespace
namespace SimCore
{
    void routeLogToGui(int level, const QString& msg);
}

#define SIM_LOG(level, msg) \
do { \
    QString qmsg = QString(msg); \
    SimCore::routeLogToGui (level, qmsg); \
    ACE_DEBUG ((level, ACE_TEXT("[%T][%M][TID:%t] %s\n"), qmsg.toUtf8().constData())); \
} while (0)

namespace SimCore
{
    class Utility
    {
        public:
            static QVector3D latLonToXYZ (float m_liveOffset, float lat, float lon, float radius)
            {
                float latRad = qDegreesToRadians (lat);
                float lonRad = qDegreesToRadians (lon + m_liveOffset);

                // Matches the North Pole logic: 
                // At lat=90, sin(90)=1, so y = radius. 
                // At lat=0 (equator), sin(0)=0, so y = 0.
                float x = radius * cos (latRad) * sin (lonRad);
                float y = radius * sin (latRad);
                float z = radius * cos (latRad) * cos (lonRad);

                return QVector3D (x, y, z);
            }

            // For Satellites (Radians - faster for 20,000+ objects)
            static QVector3D latLonToXYZRad (float m_liveOffset, float latRad, float lonRad, float radius)
            {
                // Apply your -90 degree offset (converted to radians)
                float offsetRad = qDegreesToRadians (m_liveOffset);
                float adjustedLon = lonRad + offsetRad;

                return QVector3D (radius * cos (latRad) * sin (adjustedLon),
                                  radius * sin (latRad),
                                  radius * cos (latRad) * cos (adjustedLon)
                                 );
            }

            // Cube with normals X, Y, Z, U, V, NX, NY, NZ (8 floats per vertex)
            static float* createNormalCube()
            {
                static float cubeNormalData[] =
                {
                    // Front face
                    -1.0f, -1.0f,  1.0f, 0.0f, 0.0f, 0,0,1,    1.0f, -1.0f,  1.0f, 1.0f, 0.0f, 0,0,1,   1.0f,  1.0f,  1.0f, 1.0f, 1.0f, 0,0,1,
                    -1.0f, -1.0f,  1.0f, 0.0f, 0.0f, 0,0,1,    1.0f,  1.0f,  1.0f, 1.0f, 1.0f, 0,0,1,   -1.0f,  1.0f,  1.0f, 0.0f, 1.0f, 0,0,1,

                    // Back face
                    -1.0f, -1.0f, -1.0f, 1.0f, 0.0f, 0,0,-1,   -1.0f,  1.0f, -1.0f, 1.0f, 1.0f, 0,0,-1,    1.0f,  1.0f, -1.0f, 0.0f, 1.0f, 0,0,-1,
                    -1.0f, -1.0f, -1.0f, 1.0f, 0.0f, 0,0,-1,    1.0f,  1.0f, -1.0f, 0.0f, 1.0f, 0,0,-1,    1.0f, -1.0f, -1.0f, 0.0f, 0.0f, 0,0,-1,

                    // Top face
                    -1.0f,  1.0f, -1.0f, 0.0f, 1.0f, 0,1,0,   -1.0f,  1.0f,  1.0f, 0.0f, 0.0f, 0,1,0,    1.0f,  1.0f,  1.0f, 1.0f, 0.0f, 0,1,0,
                    -1.0f,  1.0f, -1.0f, 0.0f, 1.0f, 0,1,0,    1.0f,  1.0f,  1.0f, 1.0f, 0.0f, 0,1,0,    1.0f,  1.0f, -1.0f, 1.0f, 1.0f, 0,1,0,

                    // Bottom face
                    -1.0f, -1.0f, -1.0f, 1.0f, 1.0f, 0,-1,0,    1.0f, -1.0f, -1.0f, 0.0f, 1.0f, 0,-1,0,    1.0f, -1.0f,  1.0f, 0.0f, 0.0f, 0,-1,0,
                    -1.0f, -1.0f, -1.0f, 1.0f, 1.0f, 0,-1,0,    1.0f, -1.0f,  1.0f, 0.0f, 0.0f, 0,-1,0,   -1.0f, -1.0f,  1.0f, 1.0f, 0.0f, 0,-1,0,

                    // Right face
                    1.0f, -1.0f, -1.0f, 1.0f, 0.0f, 1,0,0,     1.0f,  1.0f, -1.0f, 1.0f, 1.0f, 1,0,0,    1.0f,  1.0f,  1.0f, 0.0f, 1.0f, 1,0,0,
                    1.0f, -1.0f, -1.0f, 1.0f, 0.0f, 1,0,0,     1.0f,  1.0f,  1.0f, 0.0f, 1.0f, 1,0,0,    1.0f, -1.0f,  1.0f, 0.0f, 0.0f, 1,0,0,

                    // Left face
                    -1.0f, -1.0f, -1.0f, 0.0f, 0.0f, -1,0,0,   -1.0f, -1.0f,  1.0f, 1.0f, 0.0f, -1,0,0,   -1.0f,  1.0f,  1.0f, 1.0f, 1.0f, -1,0,0,
                    -1.0f, -1.0f, -1.0f, 0.0f, 0.0f, -1,0,0,   -1.0f,  1.0f,  1.0f, 1.0f, 1.0f, -1,0,0,   -1.0f,  1.0f, -1.0f, 0.0f, 1.0f, -1,0,0
                };

                return cubeNormalData;
            }

            // X, Y, Z, U, V
            static float* createCube()
            {
                static float cubeData[] =
                {
                    // Front face
                    -1.0f, -1.0f,  1.0f, 0.0f, 0.0f,  1.0f, -1.0f,  1.0f, 1.0f, 0.0f,  1.0f,  1.0f,  1.0f, 1.0f, 1.0f,
                    -1.0f, -1.0f,  1.0f, 0.0f, 0.0f,  1.0f,  1.0f,  1.0f, 1.0f, 1.0f, -1.0f,  1.0f,  1.0f, 0.0f, 1.0f,

                    // Back face
                    -1.0f, -1.0f, -1.0f, 1.0f, 0.0f, -1.0f,  1.0f, -1.0f, 1.0f, 1.0f,  1.0f,  1.0f, -1.0f, 0.0f, 1.0f,
                    -1.0f, -1.0f, -1.0f, 1.0f, 0.0f,  1.0f,  1.0f, -1.0f, 0.0f, 1.0f,  1.0f, -1.0f, -1.0f, 0.0f, 0.0f,

                    // Top face
                    -1.0f,  1.0f, -1.0f, 0.0f, 1.0f, -1.0f,  1.0f,  1.0f, 0.0f, 0.0f,  1.0f,  1.0f,  1.0f, 1.0f, 0.0f,
                    -1.0f,  1.0f, -1.0f, 0.0f, 1.0f,  1.0f,  1.0f,  1.0f, 1.0f, 0.0f,  1.0f,  1.0f, -1.0f, 1.0f, 1.0f,

                    // Bottom face
                    -1.0f, -1.0f, -1.0f, 1.0f, 1.0f,  1.0f, -1.0f, -1.0f, 0.0f, 1.0f,  1.0f, -1.0f,  1.0f, 0.0f, 0.0f,
                    -1.0f, -1.0f, -1.0f, 1.0f, 1.0f,  1.0f, -1.0f,  1.0f, 0.0f, 0.0f, -1.0f, -1.0f,  1.0f, 1.0f, 0.0f,

                    // Right face
                    1.0f, -1.0f, -1.0f, 1.0f, 0.0f,  1.0f,  1.0f, -1.0f, 1.0f, 1.0f,  1.0f,  1.0f,  1.0f, 0.0f, 1.0f,
                    1.0f, -1.0f, -1.0f, 1.0f, 0.0f,  1.0f,  1.0f,  1.0f, 0.0f, 1.0f,  1.0f, -1.0f,  1.0f, 0.0f, 0.0f,

                    // Left face
                    -1.0f, -1.0f, -1.0f, 0.0f, 0.0f, -1.0f, -1.0f,  1.0f, 1.0f, 0.0f, -1.0f,  1.0f,  1.0f, 1.0f, 1.0f,
                    -1.0f, -1.0f, -1.0f, 0.0f, 0.0f, -1.0f,  1.0f,  1.0f, 1.0f, 1.0f, -1.0f,  1.0f, -1.0f, 0.0f, 1.0f
                };

                return cubeData;
            }

            // Plane X, Y, U, V
            static float* createLargePlane()
            {
                static float data[] =
                { 
                    -1.0, -1.0, 0.0,  0.0, 0.0,
                    1.0, -1.0, 0.0,  1.0, 0.0,
                    1.0,  1.0, 0.0,  1.0, 1.0,
                    -1.0,  1.0, 0.0,  0.0, 1.0 
                };

                return data;
            }

            // Square Geometry (X, Y, U, V)
            static float* createPlane()
            {
                static float data[] = {
                    -0.5, -0.5,  0, 0, 
                    0.5,  -0.5,  1, 0, 
                    0.5,   0.5,  1, 1,
                    -0.5,  0.5,  0, 1
                };

                return data;
            }

            // To test compute shader in pipeline
            static GLuint createDynamicTexture (int w, int h)
            {
                GLuint id;
                glGenTextures (1, &id);
                glBindTexture (GL_TEXTURE_2D, id);

                // REQUIRED for compute shaders: Allocate immutable storage
                // We use GL_RGBA8 to match the image2D layout in the shader
                glTexStorage2D (GL_TEXTURE_2D, 1, GL_RGBA8, w, h);

                glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                return id;
            }

            static GLuint createSimpleTexture (int w, int h)
            {
                GLuint id;
                glGenTextures (1, &id);
                glBindTexture (GL_TEXTURE_2D, id);

                std::vector<unsigned char> px (w * h * 4);

                for (int i=0; i<w*h; ++i)
                {
                    int x = i % w, y = i / w;
                    unsigned char c = ((x/32 + y/32) % 2 == 0) ? 255 : 100;
                    px[i*4]=c; px[i*4+1]=0; px[i*4+2]=255-c; px[i*4+3]=255;
                }

                glTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
                glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                return id;
            }

            static GLuint loadTexture (std::array<int, 2>& mapSize, const QString& filePath, const int type = 1)
            {
                if (type < 1)
                {
                    SIM_LOG (LM_CRITICAL, "Invalid texture type");
                    return 0;
                }

                if (filePath == nullptr)
                {
                    SIM_LOG (LM_CRITICAL, "Empty or null texture file path");
                }

                QImageReader reader (filePath);
                
                // Bypass the default 128MB limit for an 8k texture
                reader.setAllocationLimit (1024); 

                if (!reader.canRead())
                {
                    SIM_LOG (LM_CRITICAL, QString ("Cannot read image: %1").arg (reader.errorString()));
                    return 0;
                }

                if (type == 1)
                {
                    // Optional: Downscale during load to stay within ROCm memory stability limits
                    //if (reader.size().width() > 8192)
                    {
                        reader.setScaledSize (QSize (mapSize[0], mapSize[1]));
                    }
                }

                QImage img = reader.read();

                if (img.isNull())
                {
                    SIM_LOG (LM_CRITICAL, QString ("Load failed: %1").arg (reader.errorString().toStdString().c_str()));
                    return 0;
                }
                else
                {
                    SIM_LOG (LM_INFO, QString ("Reading texture file %1").arg (filePath));
                }

                if (type == 1)
                {
                    // Convert to RGBA8888 for GL_RGBA8 compatibility
                    // Use flipped() to move the origin from top-left to bottom-left for OpenGL
                    img = img.convertToFormat (QImage::Format_RGBA8888).flipped (Qt::Horizontal);
                }
                else if (type == 2)
                {
                    img = img.convertToFormat (QImage::Format_RGBA8888);
                }

                GLuint textureID;
                glGenTextures (1, &textureID);
                glBindTexture (GL_TEXTURE_2D, textureID);

                if (type == 1)
                {
                    // Texture parameters for the globe
                    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
                    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
                    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                }
                else if (type == 2)
                {
                    // SDF Font Specifics: LINEAR filtering is mandatory for smooth scaling.
                    // We disable Mipmaps for SDF fonts to keep the distance field edges sharp.
                    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                }

                // Upload to the RX 7800XT
                glTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA8, 
                              img.width(), img.height(), 0, 
                              GL_RGBA, GL_UNSIGNED_BYTE, img.constBits());

                glGenerateMipmap (GL_TEXTURE_2D);

                return textureID;
            }

            static bool loadTextureFiles (std::array<int, 2>& mapSize)
            {
                for (int i = 0; i < Globe::TextureList.size(); i++)
                {
                    std::map<std::string, QString> texture = Globe::TextureList.at (i);

                    if (texture.empty())
                    {
                        SIM_LOG (LM_CRITICAL, "No map found");
                        return false;
                    }

                    for (const auto& pair : Globe::TextureList.at (i))
                    {
                        // Indexes start at 0, but types start at 1
                        GLuint textureID = loadTexture (mapSize, pair.second, i+1);
                        
                        if (textureID > 0)
                        {
                            Globe::textureMap.insert ({pair.first, textureID});
                        }
                        else
                        {
                            SIM_LOG (LM_CRITICAL, "Fatal error: Texure ID is 0"); 
                            return false;
                        }
                    }
                }

                return true;
            }
    };

} // namspace Utility

#endif // UTILITY_HXX
