#pragma once

#ifndef GLOBE_HXX
#define GLOBE_HXX

#include <glad/gl.h>
#include <utility>
#include <map>
#include <string>
#include "Config.hxx"
#include <QElapsedTimer>
#include <QOpenGLShaderProgram>
#include <QPainter>
#include <SGP4.h>

struct ParsingTaskData
{
    QString data;
    QString group;
};


struct FileTaskData
{
    QString path;
    QString group;
};

namespace Globe
{
    //class Globe
    //{
    //    public:
            // Shared matrix variables
            inline QMatrix4x4 modelMatrix;
            inline QMatrix4x4 viewMatrix;
            inline QMatrix4x4 projectMatrix;
            inline QMatrix4x4 m_mvp;
            inline float g_perspective;

            // Shader program for compute tasks
            inline QOpenGLShaderProgram* m_computeProgram;
            inline QElapsedTimer timer;            

            // globe settings (with defaults from above)
            inline float m_liveOffset;
            inline float m_liveTilt;
            inline QVector2D m_rotation; // x = pitch, y = yaw
            inline float m_zoom;
            inline QVector2D m_offset;   // for dragging
            inline float m_ambientLevel;

             // Radius ::Config::DEFAULT_RADIUS, ::Config::DEFAULT_SECTORS sectors, ::Config::DEFAULT_STACKS stacks
            inline float globeRadius;
            inline int globeSectors;
            inline int globeStacks;

            // Set to SGP4
            inline double earthRadiusKm;
            inline float glScaleFactor;
            inline float glGravityConstant;

            // Real earth standard gravitational parameter (μ) = 398600.4418 km^3/s^2
            inline double earthMu = 398600.4418;

            // Height of labels and points above the globe. Put labels above globe, but not too far or they will "slide"
            // due to perspective and zoom changes
            inline float cityLabelHeight;

            // Global missile variables
            inline int m_trailCapacity;
            inline int glTrailBufferCap;

            struct texSizes
            {
                std::array<int, 2> huge = {16200, 8100};
                std::array<int, 2> large = {8192, 4096};
                std::array<int, 2> medium = {6144, 3072};
                std::array<int, 2> small = {4096, 2048};
                std::array<int, 2> stdFont = {512, 512};
            };

            static inline std::map<std::string, QString> TextureFiles =
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

            static inline std::map<std::string, QString> FontFiles =
            {
                 {"arialFont", "fonts/arial.png"}                            // Arial font atlas
            };

            static inline std::vector<std::map<std::string, QString>> TextureList = {TextureFiles, FontFiles};

            inline texSizes mapSizes;

            inline std::map<std::string, GLuint> textureMap;

            // Store multiple programs by name
            inline QMap<QString, QOpenGLShaderProgram*> m_shaders;
            inline QOpenGLShaderProgram* m_currentProgram = nullptr;

            // Map label fonts
            inline QColor m_cityColor = Qt::cyan;
            inline QColor m_textColor = Qt::white;
            inline QColor m_shadowColor = Qt::black;
            inline float m_markerSize;
            inline int m_fontSize; // Default size

            // City handling
            struct City
            {
                QVector3D position;
                QString name;
                float lat;
                float lon;
                QString extraInfo; // For future data
            };

            inline std::vector<City> m_capitals;
            inline const City* m_selectedCity = nullptr;
            inline bool m_showCities = true;
            inline int m_cityCount;

            struct LabelVertex
            {
                float ax, ay, az;
                float u, v;      // Texture coordinates (From arial.png)
                float offX, offY; // 2D offset from the anchor (To layout the letters)
            };

            inline std::vector<LabelVertex> m_cityLabels;
    //};
} // namespace Globe

#endif // GLOBE_HXX
