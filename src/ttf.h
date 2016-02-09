#include <ft2build.h>
#include FT_FREETYPE_H

class TtfFont {
public:
    typedef struct {
        int x, y;
    } IntPoint;

    std::string           fontFile;
    std::string           name;
    bool                  loaded;

    // The font itself, plus the mapping from ASCII codes to glyphs
    FT_Library ftlib;
    FT_Face font_face;
    int                   maxPoints;
    int                   scale;

    // The filehandle, while loading
    unsigned char *fontdata;
    unsigned int fontdatalen;

    // Some state while rendering a character to curves
    enum {
        NOTHING   = 0,
        ON_CURVE  = 1,
        OFF_CURVE = 2
    };
    int                   lastWas;
    IntPoint              lastOnCurve;
    IntPoint              lastOffCurve;

    // And the state that the caller must specify, determines where we
    // render to and how
    SBezierList *beziers;
    Vector      origin, u, v;

    TtfFont();
    ~TtfFont();

    void LoadGlyph(int index){}
    bool LoadFontFromFile(bool nameOnly);
    std::string FontFileBaseName(void);

    void PlotCharacter(int *dx, uint32_t c, uint32_t gli, float spacing);
    void PlotString(const char *str, float spacing,
                    SBezierList *sbl, Vector origin, Vector u, Vector v);

    Vector TransformFloatPoint(float x, float y);
    void LineSegment(float x0, float y0, float x1, float y1);
    void Bezier(float x0, float y0, float x1, float y1, float x2, float y2);
};

class TtfFontList {
public:
    bool                loaded;
    List<TtfFont>       l;

    void LoadAll(void);

    void PlotString(const std::string &font, const char *str, float spacing,
                    SBezierList *sbl, Vector origin, Vector u, Vector v);
};


