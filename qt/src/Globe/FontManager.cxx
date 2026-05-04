#include "FontManager.hxx"

namespace Globe
{
    void FontManager::loadArialFont (const QString& path)
    {
        if (path == nullptr)
        {
            SIM_LOG (LM_CRITICAL, "Font file path is null");
            return;
        }

        QFile file (path);

        if (!file.open (QIODevice::ReadOnly | QIODevice::Text))
        {
            SIM_LOG (LM_CRITICAL, QString ("Failed to open font file %1").arg (path.toStdString()));
            return;
        }

        QTextStream in (&file);

        while (!in.atEnd())
        {
            QString line = in.readLine().simplified();

            if (line.startsWith ("char "))
            {
                // Split and map keys like x, y, width, height, xoffset, yoffset, xadvance
                // Standard BMFont fields:
                Glyph g;
                g.id       = getValue (line, "id=");
                g.x        = getValue (line, "x=");
                g.y        = getValue (line, "y=");
                g.w        = getValue (line, "width=");
                g.h        = getValue (line, "height=");
                g.xOff     = getValue (line, "xoffset=");
                g.yOff     = getValue (line, "yoffset=");
                g.xAdv     = getValue (line, "xadvance=");

                m_glyphs[g.id] = g;
            }
        }

        SIM_LOG (LM_INFO, QString ("Font loaded: %1 glyphs ready.").arg (m_glyphs.size()));
    }

    int FontManager::getValue (const QString& line, const QString& tag)
    {
        int result = 0;

        int start = line.indexOf (tag) + tag.length();
        int end = line.indexOf (" ", start);

        result = line.mid (start, end - start).toInt();

        return result;
    }

    void FontManager::buildLabelVBO (const std::vector<Globe::City>& cities)
    {
        Globe::m_cityLabels.clear();
        float atlasW = 512.0f; //(float)Globe::mapSizes.stdFont[0]; // Update with the actual arial.png width
        float atlasH = 512.0f;

        for (const auto& city : cities)
        {
            float cursorX = 10.0f; // Current horizontal pen position
            
            for (QChar c : city.name)
            {
                int id = c.unicode();
                auto it = m_glyphs.find  (id);

                if (it != m_glyphs.end())
                {

                    const Glyph& g = m_glyphs[id];

                    float x1 = cursorX + g.xOff;
                    float x2 = x1 + g.w;
                    float y1 = -(float)g.yOff;          // Top
                    float y2 = -(float)(g.yOff + g.h);  // Bottom (further down, so more negative)

                    /// 2. Calculate UVs (Normalized 0.0 to 1.0)
                    float u1 = (float)g.x / atlasW;
                    float v1 = (float)g.y / atlasH;     // Top
                    float u2 = (float)(g.x + g.w) / atlasW;
                    float v2 = (float)(g.y + g.h) / atlasH; // Bottom

                    // Define the 4 corners of the character quad
                    // We use the city's 3D position as the anchor for all 4
                    QVector3D a = city.position; 
                    
                    // 3. Push the two triangles (CCW order)
                    // Triangle 1
                    Globe::m_cityLabels.push_back ({a.x(), a.y(), a.z(), u1, v1, x1, y1}); // TL
                    Globe::m_cityLabels.push_back ({a.x(), a.y(), a.z(), u2, v1, x2, y1}); // TR
                    Globe::m_cityLabels.push_back ({a.x(), a.y(), a.z(), u1, v2, x1, y2}); // BL

                    // Triangle 2
                    Globe::m_cityLabels.push_back ({a.x(), a.y(), a.z(), u2, v1, x2, y1}); // TR
                    Globe::m_cityLabels.push_back ({a.x(), a.y(), a.z(), u2, v2, x2, y2}); // BR
                    Globe::m_cityLabels.push_back ({a.x(), a.y(), a.z(), u1, v2, x1, y2}); // BL

                    cursorX += g.xAdv; // Move pen to next character
                }
            }
        }

        m_labelVertexCount = static_cast<int>(Globe::m_cityLabels.size());

        SIM_LOG (LM_DEBUG, QString ("Font configured with %1 vertices").arg (m_labelVertexCount));
    }
} // namespace Globe
