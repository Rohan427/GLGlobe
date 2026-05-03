#pragma once

#include <QElapsedTimer>
#include <QOpenGLShaderProgram>
#include <QPainter>

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
            // Initial settings constants
            inline static const float DEFAULT_LIVEOFFSET = -90.0f;
            inline static const float DEFAULT_TILT = 23.5f;
            inline static const QVector2D DEFAULT_ROTATION = QVector2D (0.0f, 0.0f); // x = pitch, y = yaw
            inline static const float DEFAULT_AMBIENT = 0.15f;
            inline static const float DEFAULT_RADIUS = 1.5f;
            inline static const int DEFAULT_SECTORS = 64;
            inline static const int DEFAULT_STACKS = 64;
            inline static const float DEFAULT_PERSPECTIVE = 45.0f;
            inline static const int DEFAULT_FONT_SIZE = 16;
            inline static const float DEFAULT_MARKER_SIZE = 5.0f;
            inline static const float DEFAULT_ZOOM = 1.0F;
            inline static const QVector2D DEFAULT_OFFSET = QVector2D (0.0f, 0.0f);
            inline static const float LABEL_HEIGHT_OFFSET = 0.0001f;
            inline static const int THREAD_SLEEP_TIME = 100000;  // 50ms tick
            inline static const int MAX_THREADS = 32;
            inline static const QString DATA_DIR_PATH = "data";
            inline static const QString DATA_FILE_SAT_SUFFIX = "sat.tle";
            inline static const QString CELESTRAK_URL = "https://celestrak.org/NORAD/elements/gp.php?";

            // Shared matrix variables
            inline QMatrix4x4 modelMatrix;
            inline QMatrix4x4 viewMatrix;
            inline QMatrix4x4 projectMatrix;
            inline float g_perspective = DEFAULT_PERSPECTIVE;

            // Shader program for compute tasks
            inline QOpenGLShaderProgram* m_computeProgram;
            inline QElapsedTimer timer;            

            // globe settings (with defaults from above)
            inline float m_liveOffset = DEFAULT_LIVEOFFSET;
            inline float m_liveTilt = DEFAULT_TILT;
            inline QVector2D m_rotation = DEFAULT_ROTATION; // x = pitch, y = yaw
            inline float m_zoom = DEFAULT_ZOOM;
            inline QVector2D m_offset = DEFAULT_OFFSET;   // for dragging
            inline float m_ambientLevel = DEFAULT_AMBIENT;

             // Radius 1.5, 64 sectors/stacks
            inline const float globeRadius = DEFAULT_RADIUS;
            inline const int globeSectors = DEFAULT_SECTORS;
            inline const int globeStacks = DEFAULT_STACKS;

            // Height of labels above the globe. Put labels above globe, but not too far or they will "slide" due
            // to perspective and zoom changes
            inline float cityLabelHeight = globeRadius + LABEL_HEIGHT_OFFSET;

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
            inline float m_markerSize = DEFAULT_MARKER_SIZE;
            inline int m_fontSize = DEFAULT_FONT_SIZE; // Default size

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
