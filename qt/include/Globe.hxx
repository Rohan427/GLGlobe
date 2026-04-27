#pragma once

#include <QElapsedTimer>
#include <QOpenGLShaderProgram>
#include <QPainter>

namespace Globe
{
    //class Globe
    //{
    //    public:
            // Shared matrix variables
            inline QMatrix4x4 modelMatrix;
            inline QMatrix4x4 viewMatrix;
            inline QMatrix4x4 projectMatrix;

            // Shader program for compute tasks
            inline QOpenGLShaderProgram* m_computeProgram;
            inline QElapsedTimer timer;

            // Initial globe settings
            inline float m_liveOffset = -90.0f;
            inline float m_liveTilt = 23.5f;
            inline float m_ambientLevel = 0.15f;

             // Radius 1.5, 64 sectors/stacks
            inline const float globeRadius = 1.5f;
            inline const int globeSectors = 64;
            inline const int globeStacks = 64;

            // Height of labels above the globe. Put labels above globe, but not too far or they will "slide" due
            // to perspective and zoom changes
            inline float cityLabelHeight = globeRadius + 0.0001f;

            struct texSizes
            {
                std::array<int, 2> huge = {16200, 8100};
                std::array<int, 2> large = {8192, 4096};
                std::array<int, 2> medium = {6144, 3072};
                std::array<int, 2> small = {4096, 2048};
            };

            inline std::map<std::string, QString> TextureFiles =
            {
                {"simple", "textures/natural_earth.png"},                   // PLAIN_EARTH, 8K can be scaled to 6K or 4K
                {"earth8k", "textures/1_earth_8k.jpg"},                     // EARTH_CL8K_DAY
                {"earth16k", "textures/1_earth_16k.jpg"},                   // EARTH_CL16K_DAY
                {"earthnc8k", "textures/2_no_clouds_8k.jpg"},               // EARTH_NC8K_DAY
                {"earthnc16k", "textures/2_no_clouds_16k.jpg"},             // EARTH_NC16K_DAY
                {"earthncice8k", "textures/4_no_ice_clouds_mts_8k.jpg"},    // EARTH8K_DAY
                {"earthncice16k", "textures/4_no_ice_clouds_mts_16k.jpg"},  // EARTH16K_DAY
                {"earthnight8k", "textures/5_night_8k.jpg"},                // EARTH8K_NIGHT
                {"earthnight16k", "textures/5_night_16k.jpg"},              // EARTH16K_NIGHT
                {"earthbump8k", "textures/elev_bump_8k.jpg"},               // EARTH8K_BUMP
                {"earthbump16k", "textures/elev_bump_16k.jpg"}              // EARTH16k_BUMP
            };

            inline texSizes mapSizes;

            inline std::map<std::string, GLuint> textureMap;

            // Store multiple programs by name
            inline QMap<QString, QOpenGLShaderProgram*> m_shaders;
            inline QOpenGLShaderProgram* m_currentProgram = nullptr;

            // Map label fonts
            inline QColor m_cityColor = Qt::cyan;
            inline QColor m_textColor = Qt::white;
            inline QColor m_shadowColor = Qt::black;
            inline float m_markerSize = 5.0f;
            inline int m_fontSize = 16; // Default size

            // City handling
            struct City
            {
                QString name;
                float lat;
                float lon;
                QString extraInfo; // For future data
            };

            inline std::vector<City> m_capitals;
            inline const City* m_selectedCity = nullptr;
            inline bool m_showCities = true;
    //};
} // namespace Globe
