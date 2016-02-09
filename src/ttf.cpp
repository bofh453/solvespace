//-----------------------------------------------------------------------------
// Routines to read a TrueType font as vector outlines, and generate them
// as entities, since they're always representable as either lines or
// quadratic Bezier curves.
//
// Copyright 2008-2013 Jonathan Westhues.
//-----------------------------------------------------------------------------
#include "solvespace.h"
#include <sys/stat.h>

/* Yecch. Irritatingly, you need to do this nonsense to get the error string table, since nobody thought to put this exact function into FreeType itsself. */
#include <ft2build.h>
/* concatenate C tokens */
#define FT_ERR_XCAT( x, y )  x ## y
#define FT_ERR_CAT( x, y )   FT_ERR_XCAT( x, y )
#define FT_ERR( e )  FT_ERR_CAT( FT_ERR_PREFIX, e )
#include FT_ERRORS_H

#undef __FTERRORS_H__
#define FT_ERRORDEF(e, v, s) { (e), (s) },
#define FT_ERROR_START_LIST
#define FT_ERROR_END_LIST { 0, NULL }

struct ft_error
{
	int err;
	char *str;
};

static const struct ft_error ft_errors[] =
{
#include FT_ERRORS_H
};

extern "C" char *ft_error_string(int err)
{
	const struct ft_error *e;
	for (e = ft_errors; e->str; e++)
		if (e->err == err)
			return e->str;
	return "Unknown error";
}

/* Okay, now that we're done with that, grab the rest of the headers. */
#undef FT_ERR
#undef FT_ERR_CAT
#undef FT_ERR_XCAT
#undef FT_ERRORDEF
#undef FT_ERROR_START_LIST
#undef FT_ERROR_END_LIST
#include FT_FREETYPE_H
#include FT_OUTLINE_H
#include FT_ADVANCES_H

//-----------------------------------------------------------------------------
// Get the list of available font filenames, and load the name for each of
// them. Only that, though, not the glyphs too.
//-----------------------------------------------------------------------------
TtfFont::TtfFont(void) {
    FT_Init_FreeType(&ftlib);
}

TtfFont::~TtfFont() {
    FT_Done_FreeType(ftlib);
}

void TtfFontList::LoadAll(void) {
    if(loaded) return;

    // Get the list of font files from the platform-specific code.
    LoadAllFontFiles();

    int i;
    for(i = 0; i < l.n; i++) {
        TtfFont *tf = &(l.elem[i]);
        tf->LoadFontFromFile(true);
    }

    loaded = true;
}

void TtfFontList::PlotString(const std::string &font, const char *str, float spacing, SBezierList *sbl,
                             Vector origin, Vector u, Vector v)
{
    LoadAll();

    int i;
    for(i = 0; i < l.n; i++) {
        TtfFont *tf = &(l.elem[i]);
        if(tf->FontFileBaseName() == font) {
            tf->LoadFontFromFile(false);
            tf->PlotString(str, spacing, sbl, origin, u, v);
            return;
        }
    }

    // Couldn't find the font; so draw a big X for an error marker.
    SBezier sb;
    sb = SBezier::From(origin, origin.Plus(u).Plus(v));
    sbl->l.Add(&sb);
    sb = SBezier::From(origin.Plus(v), origin.Plus(u));
    sbl->l.Add(&sb);
}

//-----------------------------------------------------------------------------
// Return the basename of our font filename; that's how the requests and
// entities that reference us will store it.
//-----------------------------------------------------------------------------
std::string TtfFont::FontFileBaseName(void) {
    std::string baseName = fontFile;
    size_t pos = baseName.rfind(PATH_SEP);
    if(pos != std::string::npos)
        return baseName.erase(0, pos + 1);
    return "";
}

//-----------------------------------------------------------------------------
// Load a TrueType font into memory. We care about the curves that define
// the letter shapes, and about the mappings that determine which glyph goes
// with which character.
//-----------------------------------------------------------------------------
bool TtfFont::LoadFontFromFile(bool nameOnly) {
    if(loaded) return true;

    FILE   *fh;
    struct stat st;
    int fterr;

    stat(fontFile.c_str(), &st);
    fh = ssfopen(fontFile, "rb");
    if(!fh) {
        return false;
    }
    fontdata = (unsigned char*)malloc(st.st_size+1);
    fontdatalen = st.st_size;
    fread(fontdata, 1, fontdatalen, fh);
    fclose(fh);

    if ((fterr = FT_New_Memory_Face(ftlib, fontdata, fontdatalen, 0, &font_face)) != 0) {
        dbp("ttf: file %s failed: '%s'", fontFile.c_str(), ft_error_string(fterr));
        free(fontdata);
        return false;
    }

    if (nameOnly) {
        name = std::string(font_face->family_name);
        FT_Done_Face(font_face);
        free(fontdata);
        return true;
    }

    if ((fterr = FT_Select_Charmap(font_face, FT_ENCODING_UNICODE)) != 0) {
        dbp("loading unicode CMap failed: %s", ft_error_string(fterr));
    }
    loaded = true;
    return true;
}

#if 0
void TtfFont::Flush(void) {
    lastWas = NOTHING;
}

void TtfFont::Handle(int *dx, int x, int y, bool onCurve) {
    x = ((x + *dx)*scale + 512) >> 10;
    y = (y*scale + 512) >> 10;

    if(lastWas == ON_CURVE && onCurve) {
        // This is a line segment.
        LineSegment(lastOnCurve.x, lastOnCurve.y, x, y);
    } else if(lastWas == ON_CURVE && !onCurve) {
        // We can't do the Bezier until we get the next on-curve point,
        // but we must store the off-curve point.
    } else if(lastWas == OFF_CURVE && onCurve) {
        // We are ready to do a Bezier.
        Bezier(lastOnCurve.x, lastOnCurve.y,
               lastOffCurve.x, lastOffCurve.y, x, y);
    } else if(lastWas == OFF_CURVE && !onCurve) {
        // Two consecutive off-curve points implicitly have an on-point
        // curve between them, and that should trigger us to generate a
        // Bezier.
        IntPoint fake;
        fake.x = (x + p->x) / 2;
        fake.y = (y + p->y) / 2;
        Bezier(lastOnCurve.x, lastOnCurve.y,
               lastOffCurve.x, lastOffCurve.y,
               fake.x, fake.y);
        lastOnCurve.x = fake.x;
        lastOnCurve.y = fake.y;
    }

    if(onCurve) {
        lastOnCurve.x = x;
        lastOnCurve.y = y;
        lastWas = ON_CURVE;
    } else {
        lastOffCurve.x = x;
        lastOffCurve.y = y;
        lastWas = OFF_CURVE;
    }
}
#endif

typedef struct OutlineData {
    TtfFont *ttf;
    float px, py; /* Current point, needed to handle quadratic Beziers. */
    float dx; /* x offset */
} OutlineData;

static int move_to(const FT_Vector *p, void *cc)
{
    OutlineData *data = (OutlineData *) cc;
    data->px = p->x / 64.0f;
    data->py = p->y / 64.0f;
	return 0;
}

static int line_to(const FT_Vector *p, void *cc)
{
    OutlineData *data = (OutlineData *) cc;
    data->ttf->LineSegment(data->px, data->py, p->x, p->y);
    data->px = p->x;
    data->py = p->y;
	return 0;
}

static int conic_to(const FT_Vector *c, const FT_Vector *p, void *cc)
{
    OutlineData *data = (OutlineData *) cc;
	float cx = (c->x / 32.0f), cy = (c->y / 32.0f);
    float px = p->x / 64.0f, py = p->y / 64.0f;
	data->ttf->Bezier((p->x + data->px + cx)/3.0f, (data->py + cy)/3.0f, (p->x + px + cx)/3.0f, (py + cy)/3.0f, p->x + px, py);
    data->px = px;
    data->py = py;
	return 0;
}

static int cubic_to(const FT_Vector *c1, const FT_Vector *c2, const FT_Vector *p, void *cc)
{
    OutlineData *data = (OutlineData *) cc;
	data->ttf->Bezier(c1->x/64.0f, c1->y/64.0f, c2->x/64.0f, c2->y/64.0f, p->x/64.0f, p->y/64.0f);
    data->px = p->x / 64.0f;
    data->py = p->y / 64.0f;
	return 0;
}

static const FT_Outline_Funcs outline_funcs = {
	move_to, line_to, conic_to, cubic_to, 0, 0
};

void TtfFont::PlotCharacter(int *dx, uint32_t c, uint32_t gli, float spacing) {
    OutlineData cc;
    FT_Fixed advanceWidth = 0;
	FT_BBox cbox;
	FT_Matrix m;
    FT_Vector v;
    int scale = font_face->units_per_EM;
    float invscale = 1.0f/(float)scale;
    int fterr = 0;

    if(c == ' ') {
        FT_Get_Advance(font_face, gli, FT_LOAD_NO_SCALE | FT_LOAD_NO_HINTING, &advanceWidth);
        *dx += advanceWidth;
        return;
    }

    /*
     * Stupid hacks: if we want fake-bold, use FT_Outline_Embolden(). This actually looks quite good.
     *               if we want fake-italic, apply a shear transform [1 s s 1 0 0] here.
     *               this looks decent at small font sizes and bad at larger ones,
     *               antialiasing mitigates this considerably though.
     */

    /* init transform matrix to identity */
	m.xx = m.yy = 1;
    m.yx = m.xy = 0;

	v.x = *dx;
	v.y = 0;

	fterr = FT_Set_Char_Size(font_face, scale, scale, 72, 72);
	if (fterr)
		dbp("freetype setting character size: %s", ft_error_string(fterr));
	FT_Set_Transform(font_face, &m, &v);

	fterr = FT_Load_Glyph(font_face, gli, FT_LOAD_NO_BITMAP | FT_LOAD_IGNORE_TRANSFORM);
	if (fterr) {
		dbp("freetype load glyph (gid %d): %s", gli, ft_error_string(fterr));
		return;
	}

    int dx0 = *dx;

    // A point that has x = xMin should be plotted at (dx0 + lsb); fix up
    // our x-position so that the curve-generating code will put stuff
    // at the right place.
    /* There's no point in getting the glyph BBox here - not only can it be needlessly slow sometimes, but because we're about to render the single glyph,
     * What we want actually *is* the CBox.
     *
     * This is notwithstanding that this makes extremely little sense, this looks like a workaround for either mishandling the start glyph on a line,
     * or as a really hacky pseudo-track-kerning (in which case it works better than one would expect! especially since most fonts don't set track kerning).
     */
	FT_Outline_Get_CBox(&font_face->glyph->outline, &cbox);
    *dx = dx0 - cbox.xMin;
    /* Yes, this is what FreeType calls left-side bearing. Then interchangeably uses that with "left-side bearing". Sigh. */
	*dx += font_face->glyph->metrics.horiBearingX;

    cc.ttf = this; /* Bleh. */
    cc.px = *dx;
    cc.py = 0;
    cc.dx = *dx;
	if ((fterr = FT_Outline_Decompose(&font_face->glyph->outline, &outline_funcs, &cc))) {
		dbp("freetype bezier decomposition failed for gid %d: %s", gli, ft_error_string(fterr));
    }

    // And we're done, so advance our position by the requested advance
    // width, plus the user-requested extra advance.
	advanceWidth = font_face->glyph->advance.x;
#ifndef _MSC_VER
    *dx = dx0 + advanceWidth + lrintf(spacing);
#else
    *dx = dx0 + advanceWidth + (int)(spacing + 0.5f);
#endif
}

void TtfFont::PlotString(const char *str, float spacing, SBezierList *sbl,
                         Vector porigin, Vector pu, Vector pv)
{
    beziers = sbl;
    u = pu;
    v = pv;
    origin = porigin;

    if(!loaded || !str || *str == '\0') {
        LineSegment(0, 0, 1024, 0);
        LineSegment(1024, 0, 1024, 1024);
        LineSegment(1024, 1024, 0, 1024);
        LineSegment(0, 1024, 0, 0);
        return;
    }

    int dx = 0;
    while(*str) {
        uint32_t chr, gid;
        str = ReadUTF8(str, (int*)&chr); // why does this return a signed int?
        gid = FT_Get_Char_Index(font_face, chr);
        if (gid < 0) {
            dbp("CID-to-GID mapping for CID 0x%04x failed: %s (using CID as GID)", chr, ft_error_string(gid));
        }
        PlotCharacter(&dx, chr, gid, spacing); /* FIXME: chr is only needed to handle spaces, which should be handled outside */
    }
}

Vector TtfFont::TransformFloatPoint(float x, float y) {
    Vector r = origin;
    r = r.Plus(u.ScaledBy(x));
    r = r.Plus(v.ScaledBy(y));
    return r;
}

void TtfFont::LineSegment(float x0, float y0, float x1, float y1) {
    SBezier sb = SBezier::From(TransformFloatPoint(x0, y0),
                               TransformFloatPoint(x1, y1));
    beziers->l.Add(&sb);
}

void TtfFont::Bezier(float x0, float y0, float x1, float y1, float x2, float y2) {
    SBezier sb = SBezier::From(TransformFloatPoint(x0, y0), TransformFloatPoint(x1, y1), TransformFloatPoint(x2, y2));
    beziers->l.Add(&sb);
}

