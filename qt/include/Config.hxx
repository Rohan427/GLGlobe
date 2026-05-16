#pragma once

#ifndef CONFIG_HXX
#define CONFIG_HXX

#include "Json.hxx"
#include "Configuration.hxx"

// Standard includes
#include <QOpenGLShaderProgram>
#include <QPainter>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <iostream>
#include <string>


class Config : public configuration::Configuration
{
    public:
        json::Json::JsonType jType;

        float DEFAULT_LIVEOFFSET;
        float DEFAULT_TILT;
        QVector2D DEFAULT_ROTATION; // x = pitch, y = yaw
        float DEFAULT_AMBIENT;
        float DEFAULT_RADIUS;
        int DEFAULT_SECTORS;
        int DEFAULT_STACKS;
        float DEFAULT_PERSPECTIVE;
        int DEFAULT_FONT_SIZE;
        float DEFAULT_MARKER_SIZE;
        float DEFAULT_ZOOM;
        QVector2D DEFAULT_OFFSET;
        float LABEL_HEIGHT_OFFSET;
        int THREAD_SLEEP_TIME; // 50ms tick
        int MAX_THREADS;
        QString DATA_DIR_PATH;
        QString DATA_FILE_SAT_SUFFIX;
        QString CELESTRAK_URL;
        QString FONT_PATH;
        float DETECTION_RANGE; // Kilometers
        QVector3D DEFAULT_CENTER;
        QVector3D SATELLITE_COLOR;
        QVector3D MISSILE_COLOR;
        QVector3D CITY_COLOR;
        QVector3D RANGE_RING_COLOR;
        float RANGE_RING_DELTA;

        struct ConfigErr
        {
            std::string message;
            int err;
            json::Json::JsonType type;
            std::string varName;
        };

        static Config& getInstance()
        {
            static Config instance;
            return instance;
        }

        // Delete copy constructor and assignment operator
        Config (const Config&) = delete;
        Config& operator= (const Config&) = delete;

        void init (const std::string filePath)
        {
            current = false;
            timestamp = 0;
            cfgFilePath = filePath;
            current = Config::update();
        };

        std::string getFilePath()
        {
            return cfgFilePath;
        }

        float getDefaultLiveOffset();
        float getDefaultTilt();
        QVector2D getDefaultRotation(); //  x : pitch, y : yaw
        float getDefaultAmbient();
        float getDefaultRadius();
        int getDefaultSectors();
        int getDefaultStacks();
        float getDefaultPerspective();
        int getDefaultFontSize();
        float getDefaultMarkerSize();
        float getDefaultZoom();
        // These are a QVector2D
        QVector2D getDefaultOffset();
        float getLabelHeightOffset(); // City label height above surface
        int getThreadSleepTime(); // 50ms tick
        int getMaxThreads(); // The maximum number of threads allocated to the application
        QString getDataDirPath(); // Ralative to the executable dir
        QString getDataFileSatSuffix();
        QString getCelestrakUrl(); // URL for Celestrak API
        QString getFontPath();
        float getDetectionRange(); // Kilometers
        QVector3D getDefaultCenter();
        QVector3D getSatColor();
        QVector3D getMisColor();
        QVector3D getCityColor();
        QVector3D getRangeRingColor();
        float getRangeRingDelta();

        //bool isCurrent();
        //time_t getLastReadTime();
        bool update();
        ConfigErr getErrObj();
        bool isCurrent();
        time_t getLastReadTime();
        bool save (const std::string& filename, bool changePath);

    private:
        Config() {};

        std::string getString (const std::string& name);
        double getDouble (const std::string& name);
        int64_t getInt64 (const std::string& name);
        uint64_t getUint64 (const std::string& name);
        bool isBool (const std::string& name);
        bool isNull (const std::string& name);

        bool setString (const std::string& name, const std::string& value);
        bool setDouble (const std::string& name, double value);
        bool setInt64 (const std::string& name, int64_t value);
        bool setUint64 (const std::string& name, uint64_t value);
        bool setBool (const std::string& name, bool value);
        bool setNull (const std::string& name);

        ConfigErr errObj;
        json::Json parser;
};

#endif // CONFIG_HXX
