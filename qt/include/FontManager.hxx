#pragma once

#ifndef FONTMANAGER_HXX
#define FONTMANAGER_HXX

#include "MainWindow.hxx"
#include "Utility.hxx"
#include "Globe.hxx"

namespace Globe
{
    struct Glyph
    {
        int id, x, y, w, h, xOff, yOff, xAdv;
    };

    class FontManager
    {
        private:

        public:
            int m_labelVertexCount;

            // This is the map that translates a character (like 'A')
            // into its texture metrics from arial.fnt
            std::map<int, Glyph> m_glyphs;
            GLuint m_labelVbo;
            GLuint m_labelVao;

            void loadFnt (const QString& path);
            void loadArialFont (const QString& path);
            void buildLabelVBO (const std::vector<City>& cities);
            int getValue (const QString& line, const QString& tag);


            
    };
} // namespace Globe

#endif // FONTMANAGER_HXX
