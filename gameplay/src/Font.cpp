#include "Base.h"
#include "Font.h"
#include "Text.h"
#include "Game.h"
#include "FileSystem.h"
#include "Bundle.h"
#include "Material.h"

// Default font shaders
#define FONT_VSH "res/shaders/font.vert"
#define FONT_FSH "res/shaders/font.frag"

namespace gameplay
{

std::vector<Font*> Font::__fontCache = std::vector<Font*>();

Effect* Font::__fontEffect = NULL;

Font::Font() :
    _format(BITMAP), _style(PLAIN), _size(0), _spacing(0.0f), _texture(NULL), _batch(NULL), _cutoffParam(NULL)
{
}

Font::~Font()
{
    // Remove this Font from the font cache.
    std::vector<Font*>::iterator itr = std::find(__fontCache.begin(), __fontCache.end(), this);
    if (itr != __fontCache.end())
    {
        __fontCache.erase(itr);
    }

    SAFE_DELETE(_batch);
    SAFE_RELEASE(_texture);

    // Free child fonts
    for (size_t i = 0, count = _sizes.size(); i < count; ++i)
    {
        SAFE_RELEASE(_sizes[i]);
    }
}

Font* Font::create(const char* path, const char* id)
{
    GP_ASSERT(path);

    // Search the font cache for a font with the given path and ID.
    for (size_t i = 0, count = __fontCache.size(); i < count; ++i)
    {
        Font* f = __fontCache[i];
        GP_ASSERT(f);
        if (f->_path == path && (id == NULL || f->_id == id))
        {
            // Found a match.
            f->addRef();
            return f;
        }
    }

    // Load the bundle.
    Bundle* bundle = Bundle::create(path);
    if (bundle == NULL)
    {
        GP_WARN("Failed to load font bundle '%s'.", path);
        return NULL;
    }

    Font* font = NULL;
    if (id == NULL)
    {
        // Get the ID of the first object in the bundle (assume it's a Font).
        const char* id;
        if ((id = bundle->getObjectId(0)) == NULL)
        {
            GP_WARN("Failed to load font without explicit id; the first object in the font bundle has a null id.");
            return NULL;
        }

        // Load the font using the ID of the first object in the bundle.
        font = bundle->loadFont(bundle->getObjectId(0));
    }
    else
    {
        // Load the font with the given ID.
        font = bundle->loadFont(id);
    }

    if (font)
    {
        // Add this font to the cache.
        __fontCache.push_back(font);
    }

    SAFE_RELEASE(bundle);

    return font;
}

unsigned int Font::getSize(unsigned int index) const
{
    GP_ASSERT(index <= _sizes.size());

    // index zero == this font
    return index == 0 ? _size : _sizes[index - 1]->_size;
}

unsigned int Font::getSizeCount() const
{
    return _sizes.size() + 1; // +1 for "this" font
}

Font::Format Font::getFormat()
{
    return _format;
}


void Font::start()
{
    // no-op : fonts now are lazily started on the first draw call
}

void Font::lazyStart()
{
    if (_batch->isStarted())
        return; // already started

    // Update the projection matrix for our batch to match the current viewport
    const Rectangle& vp = Game::getInstance()->getViewport();
    if (!vp.isEmpty())
    {
        Game* game = Game::getInstance();
        Matrix projectionMatrix;
        Matrix::createOrthographicOffCenter(vp.x, vp.width, vp.height, vp.y, 0, 1, &projectionMatrix);
        _batch->setProjectionMatrix(projectionMatrix);
    }

    _batch->start();
}

void Font::finish()
{
    // Finish any font batches that have been started
    if (_batch->isStarted())
        _batch->finish();

    for (size_t i = 0, count = _sizes.size(); i < count; ++i)
    {
        SpriteBatch* batch = _sizes[i]->_batch;
        if (batch->isStarted())
            batch->finish();
    }
}

Font* Font::findClosestSize(int size)
{
    if (size == (int)_size)
        return this;

    int diff = abs(size - (int)_size);
    Font* closest = this;
    for (size_t i = 0, count = _sizes.size(); i < count; ++i)
    {
        Font* f = _sizes[i];
        int d = abs(size - (int)f->_size);
        if (d < diff || (d == diff && f->_size > closest->_size)) // prefer scaling down instead of up
        {
            diff = d;
            closest = f;
        }
    }

    return closest;
}

float Font::getCharacterSpacing() const
{
    return _spacing;
}

void Font::setCharacterSpacing(float spacing)
{
    _spacing = spacing;
}

SpriteBatch* Font::getSpriteBatch(unsigned int size) const
{
    if (size == 0)
        return _batch;

    // Find the closest sized child font
    return const_cast<Font*>(this)->findClosestSize(size)->_batch;
}

Font::Justify Font::getJustify(const char* justify)
{
    if (!justify)
    {
        return Font::ALIGN_TOP_LEFT;
    }

    if (strcmpnocase(justify, "ALIGN_LEFT") == 0)
    {
        return Font::ALIGN_LEFT;
    }
    else if (strcmpnocase(justify, "ALIGN_HCENTER") == 0)
    {
        return Font::ALIGN_HCENTER;
    }
    else if (strcmpnocase(justify, "ALIGN_RIGHT") == 0)
    {
        return Font::ALIGN_RIGHT;
    }
    else if (strcmpnocase(justify, "ALIGN_TOP") == 0)
    {
        return Font::ALIGN_TOP;
    }
    else if (strcmpnocase(justify, "ALIGN_VCENTER") == 0)
    {
        return Font::ALIGN_VCENTER;
    }
    else if (strcmpnocase(justify, "ALIGN_BOTTOM") == 0)
    {
        return Font::ALIGN_BOTTOM;
    }
    else if (strcmpnocase(justify, "ALIGN_TOP_LEFT") == 0)
    {
        return Font::ALIGN_TOP_LEFT;
    }
    else if (strcmpnocase(justify, "ALIGN_VCENTER_LEFT") == 0)
    {
        return Font::ALIGN_VCENTER_LEFT;
    }
    else if (strcmpnocase(justify, "ALIGN_BOTTOM_LEFT") == 0)
    {
        return Font::ALIGN_BOTTOM_LEFT;
    }
    else if (strcmpnocase(justify, "ALIGN_TOP_HCENTER") == 0)
    {
        return Font::ALIGN_TOP_HCENTER;
    }
    else if (strcmpnocase(justify, "ALIGN_VCENTER_HCENTER") == 0)
    {
        return Font::ALIGN_VCENTER_HCENTER;
    }
    else if (strcmpnocase(justify, "ALIGN_BOTTOM_HCENTER") == 0)
    {
        return Font::ALIGN_BOTTOM_HCENTER;
    }
    else if (strcmpnocase(justify, "ALIGN_TOP_RIGHT") == 0)
    {
        return Font::ALIGN_TOP_RIGHT;
    }
    else if (strcmpnocase(justify, "ALIGN_VCENTER_RIGHT") == 0)
    {
        return Font::ALIGN_VCENTER_RIGHT;
    }
    else if (strcmpnocase(justify, "ALIGN_BOTTOM_RIGHT") == 0)
    {
        return Font::ALIGN_BOTTOM_RIGHT;
    }
    else
    {
        GP_WARN("Invalid alignment string: '%s'. Defaulting to ALIGN_TOP_LEFT.", justify);
    }

    // Default.
    return Font::ALIGN_TOP_LEFT;
}

}
