#include "Config.hxx"

#define DEBUG

float Config::getDefaultLiveOffset()
{
    DEFAULT_LIVEOFFSET = (float)getDouble ("defaultLiveOffset");

    return DEFAULT_LIVEOFFSET;
}

float Config::getDefaultTilt()
{
    DEFAULT_TILT = (float)getDouble ("defaultTilt");

    return DEFAULT_TILT;
}

QVector2D Config::getDefaultRotation()
{
    DEFAULT_ROTATION = QVector2D ((float)getDouble ("defaultRotation1"),
                                  (float)getDouble ("defaultRotation2")
                                 );

    return DEFAULT_ROTATION;
}

float Config::getDefaultAmbient()
{
    DEFAULT_AMBIENT = (float)getDouble ("defaultAmbient");

    return DEFAULT_AMBIENT;
}

float Config::getDefaultRadius()
{
    DEFAULT_RADIUS = (float)getDouble ("defaultRadius");

    return DEFAULT_RADIUS;
}

int Config::getDefaultSectors()
{
    DEFAULT_SECTORS = (int)getInt64 ("defaultSectors");

    return DEFAULT_SECTORS;
}

int Config::getDefaultStacks()
{
    DEFAULT_STACKS = (int)getInt64 ("defaultStacks");

    return DEFAULT_STACKS;
}

float Config::getDefaultPerspective()
{
    DEFAULT_PERSPECTIVE = (float)getDouble ("defaultPerspective");

    return DEFAULT_PERSPECTIVE;
}

int Config::getDefaultFontSize()
{
    DEFAULT_FONT_SIZE = (int)getInt64 ("defaultFontSize");

    return DEFAULT_FONT_SIZE;
}

float Config::getDefaultMarkerSize()
{
    DEFAULT_MARKER_SIZE = (float)getDouble ("defaultMarkerSize");

    return DEFAULT_MARKER_SIZE;
}

float Config::getDefaultZoom()
{
    DEFAULT_ZOOM = (float)getDouble ("defaultZoom");

    return DEFAULT_ZOOM;
}

// These are a QVector2D
QVector2D Config::getDefaultOffset()
{
    DEFAULT_OFFSET = QVector2D ((float)getDouble ("defaultOffset1"),
                                (float)getDouble ("defaultOffset2")
                               );

    return DEFAULT_OFFSET;
}

float Config::getLabelHeightOffset()
{
    LABEL_HEIGHT_OFFSET = (float)getDouble ("labelHeightOffset");

    return LABEL_HEIGHT_OFFSET;
}

int Config::getThreadSleepTime()
{
    THREAD_SLEEP_TIME = (int)getInt64 ("threadSleepTime");

    return THREAD_SLEEP_TIME;
}

int Config::getMaxThreads()
{
    MAX_THREADS = (int)getInt64 ("maxThreads");

    return MAX_THREADS;
}

QString Config::getDataDirPath()
{
    DATA_DIR_PATH = QString::fromStdString (getString ("dataDirPath"));

    return DATA_DIR_PATH;
}

QString Config::getDataFileSatSuffix()
{
    DATA_FILE_SAT_SUFFIX = QString::fromStdString (getString ("dataFileSatSuffix"));

    return DATA_FILE_SAT_SUFFIX;
}

QString Config::getCelestrakUrl()
{
    CELESTRAK_URL = QString::fromStdString (getString ("celestrakUrl"));

    return CELESTRAK_URL;
}

QString Config::getFontPath()
{
    FONT_PATH = QString::fromStdString (getString ("fontPath"));

    return FONT_PATH;
}

float Config::getDetectionRange()
{
    DETECTION_RANGE = (float)getDouble ("detectionRange");

    return DETECTION_RANGE;
}

QVector3D Config::getDefaultCenter()
{
    DEFAULT_CENTER = QVector3D ((float)getDouble ("defaultCenterx"),
                                (float)getDouble ("defaultCentery"),
                                (float)getDouble ("defaultCenterz")
                               );

    return DEFAULT_CENTER;
}

QVector3D Config::getSatColor()
{
    SATELLITE_COLOR = QVector3D ((float)getDouble ("satColorR"),
                                 (float)getDouble ("satColorG"),
                                 (float)getDouble ("satColorB")
                                );

    return SATELLITE_COLOR;
}

QVector3D Config::getMisColor()
{
    MISSILE_COLOR = QVector3D ((float)getDouble ("misColorR"),
                               (float)getDouble ("misColorG"),
                               (float)getDouble ("misColorB")
                              );

    return MISSILE_COLOR;
}

QVector3D Config::getCityColor()
{
    CITY_COLOR = QVector3D ((float)getDouble ("cityColorR"),
                            (float)getDouble ("cityColorG"),
                            (float)getDouble ("cityColorB")
                           );

    return CITY_COLOR;
}

QVector3D Config::getRangeRingColor()
{
    RANGE_RING_COLOR = QVector3D ((float)getDouble ("rngRingColorR"),
                                  (float)getDouble ("rngRingColorG"),
                                  (float)getDouble ("rngRingColorB")
                                 );

    return RANGE_RING_COLOR;
}

float Config::getRangeRingDelta()
{
    RANGE_RING_DELTA = (float)getDouble ("rangeRingDelta");

    return RANGE_RING_DELTA;
}

int Config::getMaxObjects()
{
    MAX_OBJECTS = (int)getUint64 ("maxObjects");

    return MAX_OBJECTS;
}

int Config::getMaxMissiles()
{
    MAX_MISSILES = (int)getUint64 ("maxMissiles");

    return MAX_MISSILES;
}




std::string Config::getString (const std::string& name)
{
    return parser.getString (name, "");
}

double Config::getDouble (const std::string& name)
{
    return parser.getDouble (name, 0.0);
}

int64_t Config::getInt64 (const std::string& name)
{
    return parser.getInt64 (name, 0);
}

uint64_t Config::getUint64 (const std::string& name)
{
    return parser.getUint64 (name, 0);
}

bool Config::isBool (const std::string& name)
{
    return parser.isBool (name, false);
}

bool Config::isCurrent()
{
    return current;
}

bool Config::isNull (const std::string& name)
{
    return parser.isNull (name);
}

time_t Config::getLastReadTime()
{
    return timestamp;
}


// Setters

bool Config::setString (const std::string& name, const std::string& value)
{
    return parser.setString (name, value);
}

bool Config::setDouble (const std::string& name, double value)
{
    return parser.setDouble (name, value);
}

bool Config::setInt64 (const std::string& name, int64_t value)
{
    return parser.setInt64 (name, value);
}

bool Config::setUint64 (const std::string& name, uint64_t value)
{
    return parser.setUint64 (name, value);
}

bool Config::setBool (const std::string& name, bool value)
{
    return parser.setBool (name, value);
}

bool Config::setNull (const std::string& name)
{
    return parser.setNull (name);
}

bool Config::update()
{
    struct stat fileInfo;

    if (!current)
    {
        // read file if exists
#ifdef DEBUG
        std::cout << "Parse JSON file with comments" << std::endl;
#endif
        if (stat (getFilePath().c_str(), &fileInfo) == 0)
        {
            // st_mtime is the time of last modification of file content
            time_t mtime = fileInfo.st_mtime;

            if ((getLastReadTime() == 0) || (mtime > getLastReadTime()))
            {
                if ((current = parser.read2 (getFilePath().c_str())))
                {
                    getDefaultLiveOffset();
                    getDefaultTilt();
                    getDefaultRotation(); //  x : pitch, y : yaw
                    getDefaultAmbient();
                    getDefaultRadius();
                    getDefaultSectors();
                    getDefaultStacks();
                    getDefaultPerspective();
                    getDefaultFontSize();
                    getDefaultMarkerSize();
                    getDefaultZoom();
                    // These are a QVector2D
                    getDefaultOffset();
                    getLabelHeightOffset(); // City label height above surface
                    getThreadSleepTime(); // 50ms tick
                    getMaxThreads(); // The maximum number of threads allocated to the application
                    getDataDirPath(); // Ralative to the executable dir
                    getDataFileSatSuffix();
                    getCelestrakUrl(); // URL for Celestrak API
                    getFontPath();
                    getDetectionRange(); // Kilometers
                    getDefaultCenter();
                    getSatColor();
                    getMisColor();
                    getCityColor();
                    getRangeRingColor();
                    getRangeRingDelta();
                    getMaxObjects();
                    getMaxMissiles();

                    std::time (&timestamp);
                }
            } // if ((getLastReadTime() == 0) || (mtime > getLastReadTime()))
            else
            {
                std::cerr << getFilePath() << " not updated" << std::endl;
                errObj.err = parser.getJsonError().err;
                errObj.message = parser.getJsonError().message;
                errObj.varName = parser.getJsonError().varName;
                errObj.type = parser.getJsonError().type;
            } // END IF-ELSE: if ((getLastReadTime() == 0) || (mtime > getLastReadTime()))
        }
        else
        {
            std::cerr << "Error getting file info " << getFilePath() << ": " << strerror (errno) << std::endl;
            errObj.err = errno;
            errObj.message = strerror (errno);
            errObj.varName = getFilePath();
            errObj.type = json::Json::TYPE_NULL;
        } // END IF-ELSE: if (stat (getFilePath(), &fileInfo) != 0)
#ifdef DEBUG
        std::cout << "File update complete: " << std::endl << std::endl;
#endif
    }
    // else don't read

    return (current);
}

Config::ConfigErr Config::getErrObj()
{
    errObj.err = parser.getJsonError().err;
    errObj.message = parser.getJsonError().message;
    errObj.varName = parser.getJsonError().varName;
    errObj.type = parser.getJsonError().type;

    return (errObj);
}

bool Config::save (const std::string& filename, bool changePath)
{
    return parser.save (filename, changePath);
}
