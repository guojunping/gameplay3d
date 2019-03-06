#include "Base.h"
#include "UFont.h"
#include "Text.h"
#include "Game.h"
#include "FileSystem.h"
#include "Bundle.h"
#include "Material.h"
#include "Conversion.h"

#ifdef WIN32
#include <windows.h>
#else
#include <iconv.h>
#endif
#include <string>

// Default font shaders
#define FONT_VSH "res/shaders/font.vert"
#define FONT_FSH "res/shaders/font.frag"

#define GLYPH_PADDING   4

namespace gameplay
{
	UFont::UFont() :Font(), _fontData(NULL), _masterFont(NULL)
	{
	}

	UFont::~UFont()
	{
		FT_Done_Face(_face);
		FT_Done_FreeType(_library);

		SAFE_DELETE(_batch);
		
		if (_masterFont == NULL &&  _fontData != NULL) {
				SAFE_DELETE_ARRAY(_fontData);
		}
	}

	bool UFont::isCharacterSupported(int character) const
	{
		return FT_Get_Char_Index(_face, character) != 0;
	}

	Font* UFont::create(char* fontData, unsigned int fontDataLen, unsigned int size, Font::Format format, Font* masterFont)
	{
		GP_ASSERT(fontData);
		GP_ASSERT(fontDataLen);

		// Create the effect for the font's sprite batch.
		if (__fontEffect == NULL)
		{
			const char* defines = NULL;
			if (format == Font::DISTANCE_FIELD)
				defines = "DISTANCE_FIELD";
			__fontEffect = Effect::createFromFile(FONT_VSH, FONT_FSH, defines);
			if (__fontEffect == NULL)
			{
				GP_WARN("Failed to create effect for font.");
				return NULL;
			}
		}
		else
		{
			__fontEffect->addRef();
		}

		FT_Library library;
		FT_Error error = FT_Init_FreeType(&library);
		if (error)
		{
			GP_WARN("FT_Init_FreeType error: %d \n", error);
			return NULL;
		}

		// Initialize font face.
		FT_Face face;
		error = FT_New_Memory_Face(library, (const FT_Byte*)fontData, fontDataLen, 0, &face);
		if (error)
		{
			FT_Done_FreeType(library);
			GP_WARN("FT_New_Face error: %d \n", error);
			return NULL;
		}

		error = FT_Select_Charmap(face, FT_ENCODING_UNICODE);
		if (error)
		{
			FT_Done_Face(face);
			FT_Done_FreeType(library);
			GP_WARN("FT_Select_Charmap error: %d \n", error);
			return NULL;
		}

		error = FT_Set_Char_Size(face, 0, size * 64, 0, 0);
		if (error)
		{
			FT_Done_Face(face);
			FT_Done_FreeType(library);
			GP_WARN( "FT_Set_Char_Size error: %d \n", error);
			return NULL;
		}

		error = FT_Load_Char(face, 'a', FT_LOAD_DEFAULT);
		if (error)
		{
			GP_WARN("FT_Load_Char error : %d \n", error);
			return NULL;
		}

		Texture* texture = Texture::create(Texture::ALPHA, 1024, 128, NULL, true);

		// Create batch for the font.
		SpriteBatch* batch = SpriteBatch::create(texture, __fontEffect, 256);

		// Release __fontEffect since the SpriteBatch keeps a reference to it
		SAFE_RELEASE(__fontEffect);

		if (batch == NULL)
		{
			FT_Done_Face(face);
			FT_Done_FreeType(library);
			GP_WARN("Failed to create batch for font.");
			return NULL;
		}

		// Add linear filtering for better font quality.
		Texture::Sampler* sampler = batch->getSampler();
		sampler->setFilterMode(Texture::LINEAR_MIPMAP_LINEAR, Texture::LINEAR);
		sampler->setWrapMode(Texture::CLAMP, Texture::CLAMP);

		// Increase the ref count of the texture to retain it.
		texture->addRef();

		UFont* font = new UFont();
		font->_format = format;
		font->_family = face->family_name;
		font->_style = Font::PLAIN;
		font->_size = size;
		font->_texHeight = 128;
		font->_texWidth = 1024;
		font->_startX = 0;
		font->_startY = 0;
		font->_texture = texture;
		font->_batch = batch;
		font->_library = library;
		font->_face = face;
		font->_fontData = fontData;
		font->_fontDataLen = fontDataLen;
		font->_advance = face->glyph->metrics.horiAdvance >> 6;
		font->_masterFont = masterFont;
		return font;
	}

	UFont::Glyph* UFont::getGlyph(wchar_t ch)
	{
		std::unordered_map<wchar_t, Glyph*>::iterator it = _glyphs.find(ch);
		if (it != _glyphs.end()) {
			return it->second;
		}

		// Load glyph image into the slot (erase the previous one).
		FT_Error error = FT_Load_Glyph(_face, FT_Get_Char_Index(_face, ch), FT_LOAD_DEFAULT | FT_LOAD_NO_BITMAP);
		if (error)
		{
			GP_WARN("FT_Load_Char error : %d \n", error);
			return NULL;
		}

		FT_Glyph ft_glyph;
		error = FT_Get_Glyph(_face->glyph, &ft_glyph);
		if (error)
		{
			GP_WARN("FT_Get_Glyph error : %d \n", error);
			return NULL;
		}

		error =  FT_Render_Glyph(_face->glyph, FT_RENDER_MODE_LCD);
		if (error)
		{
			GP_WARN("FT_Render_Glyph error : %d \n", error);
			return NULL;
		}

		error = FT_Glyph_To_Bitmap(&ft_glyph, ft_render_mode_normal, 0, 1);
		if (error)
		{
			GP_WARN("FT_Glyph_To_Bitmap error : %d \n", error);
			return NULL;
		}
		FT_BitmapGlyph bitmap_glyph = (FT_BitmapGlyph)ft_glyph;
		FT_Bitmap& bitmap = bitmap_glyph->bitmap;
		unsigned char* glyphBuffer = bitmap.buffer;
		int glyphWidth = bitmap.width;
		int glyphHeight = bitmap.rows;

		int lineHeight = round((_face->bbox.yMax - _face->bbox.yMin) * _size / _face->units_per_EM);
		int advance = glyphWidth + GLYPH_PADDING;

		// If we reach the end of the image wrap aroud to the next row.
		if ((_startX + advance) > _texWidth)
		{
			_startX = 0;
			_startY = _startY + lineHeight;

			if ((_startY + lineHeight) > _texHeight)
			{
				if (_texHeight * 2 >= 2048) {
					GP_WARN("Image size exceeded!");
					return NULL;
				}
				Texture* texture = Texture::create(Texture::ALPHA, _texWidth, _texHeight * 2, NULL, true);
				texture->copyTexture(_texture, 0, 0, 0, 0, _texWidth, _texHeight);	

				this->_batch->setTexture(texture);
				_texHeight = _texHeight * 2;
				
				SAFE_RELEASE(_texture)
				_texture = texture;
				std::unordered_map<wchar_t, Glyph*>::iterator it = _glyphs.begin();
				for (; _glyphs.end() != it; ++it) {
					it->second->uvs[1] = it->second->uvs[1] / 2;
					it->second->uvs[3] = it->second->uvs[3] / 2;
				}
			}
		}
		UFont::Glyph *glyph = new UFont::Glyph();

		int penY = _startY + (lineHeight - glyphHeight) + (glyphHeight - _face->glyph->bitmap_top) - lineHeight/5;
		if (penY < 0)
			penY = 0;

		// Draw the glyph to the bitmap with a one pixel padding.
		_texture->setData(_startX, penY, glyphWidth, glyphHeight, glyphBuffer);

		glyph->code = ch;
		glyph->width = glyphWidth;
		glyph->bearingX = _face->glyph->metrics.horiBearingX >> 6;
		glyph->advance = _face->glyph->metrics.horiAdvance >> 6;

		// Generate UV coords.
		glyph->uvs[0] = (float)_startX / (float)_texture->getWidth();
		glyph->uvs[1] = (float)_startY / (float)this->_texture->getHeight();
		glyph->uvs[2] = (float)(_startX + glyphWidth) / (float)_texture->getWidth();
		glyph->uvs[3] = (float)(_startY + lineHeight) / (float)this->_texture->getHeight();

		// Set the pen position for the next glyph
		_startX += advance;

		_glyphs.insert(std::make_pair(ch, glyph));

		return glyph;
	}

	void UFont::drawText(const char* text, int x, int y, const Vector4& color, unsigned int size, bool rightToLeft)
	{
		GP_ASSERT(_size);
		GP_ASSERT(text);

		if (size == 0)
		{
			size = _size;
		}
		else
		{
			// Delegate to closest sized font
			Font* f = findClosestSize(size);
			if (f != this)
			{
				f->drawText(text, x, y, color, size, rightToLeft);
				return;
			}
		}

		lazyStart();

		float scale = (float)size / _size;
		int spacing = (int)(size * _spacing);

		int len = strlen(text);
		wchar_t *wbuf = new wchar_t[len+1];
		//int wlen = utf82wchar(text, len, wbuf, len);
        int wlen = UTF82Unicode(text, wbuf , len);

		wbuf[wlen] = 0x0;

		const wchar_t* cursor = NULL;

		if (rightToLeft)
		{
			cursor = wbuf;
		}

		int xPos = x, yPos = y;
		bool done = false;

		while (!done)
		{
			size_t length;
			size_t startIndex;
			int iteration;
			if (rightToLeft)
			{
				wchar_t delimiter = cursor[0];
				while (!done &&
					(delimiter == L' ' ||
						delimiter == L'\t' ||
						delimiter == L'\r' ||
						delimiter == L'\n' ||
						delimiter == 0))
				{
					switch (delimiter)
					{
					case L' ':
						xPos += _advance;
						break;
					case L'\r':
					case L'\n':
						yPos += size;
						xPos = x;
						break;
					case L'\t':
						xPos += _advance * 4;
						break;
					case 0:
						done = true;
						break;
					}

					if (!done)
					{
						++cursor;
						delimiter = cursor[0];
					}
				}

				length = wcscspn(cursor, L"\r\n");
				startIndex = length - 1;
				iteration = -1;
			}
			else
			{
				length = wlen;
				startIndex = 0;
				iteration = 1;
			}

			GP_ASSERT(_batch);
			for (size_t i = startIndex; i < length; i += (size_t)iteration)
			{
				wchar_t c = 0;
				if (rightToLeft)
				{
					c = cursor[i];
				}
				else
				{
					c = wbuf[i];
				}

				// Draw this character.
				switch (c)
				{
				case L' ':
					xPos += _advance;
					break;
				case L'\r':
				case L'\n':
					yPos += size;
					xPos = x;
					break;
				case L'\t':
					xPos += _advance * 4;
					break;
				default:
					Glyph* g = getGlyph(c);
					if (getFormat() == Font::DISTANCE_FIELD)
					{
						if (_cutoffParam == NULL)
							_cutoffParam = _batch->getMaterial()->getParameter("u_cutoff");
						// TODO: Fix me so that smaller font are much smoother
						_cutoffParam->setVector2(Vector2(1.0, 1.0));
					}
					_batch->draw(xPos + (int)(g->bearingX * scale), yPos, g->width * scale, size, g->uvs[0], g->uvs[1], g->uvs[2], g->uvs[3], color);
					xPos += floor(g->advance * scale + spacing);
					break;
				}
			}

			if (rightToLeft)
			{
				cursor += length;
			}
			else
			{
				done = true;
			}
		}
		delete[] wbuf;
	}

	void UFont::drawText(const wchar_t* text, int x, int y, const Vector4& color, unsigned int size, bool rightToLeft)
	{
		GP_ASSERT(_size);
		GP_ASSERT(text);

		if (size == 0)
		{
			size = _size;
		}
		else
		{
			// Delegate to closest sized font
			Font* f = findClosestSize(size);
			if (f != this)
			{
				f->drawText(text, x, y, color, size, rightToLeft);
				return;
			}
		}

		lazyStart();

		float scale = (float)size / _size;
		int spacing = (int)(size * _spacing);

		int len = wcslen(text);

		const wchar_t* cursor = NULL;

		if (rightToLeft)
		{
			cursor = text;
		}

		int xPos = x, yPos = y;
		bool done = false;

		while (!done)
		{
			size_t length;
			size_t startIndex;
			int iteration;
			if (rightToLeft)
			{
				wchar_t delimiter = cursor[0];
				while (!done &&
					(delimiter == L' ' ||
						delimiter == L'\t' ||
						delimiter == L'\r' ||
						delimiter == L'\n' ||
						delimiter == 0))
				{
					switch (delimiter)
					{
					case L' ':
						xPos += _advance;
						break;
					case L'\r':
					case L'\n':
						yPos += size;
						xPos = x;
						break;
					case L'\t':
						xPos += _advance * 4;
						break;
					case 0:
						done = true;
						break;
					}

					if (!done)
					{
						++cursor;
						delimiter = cursor[0];
					}
				}

				length = wcscspn(cursor, L"\r\n");
				startIndex = length - 1;
				iteration = -1;
			}
			else
			{
				length = len;
				startIndex = 0;
				iteration = 1;
			}

			GP_ASSERT(_batch);
			for (size_t i = startIndex; i < length; i += (size_t)iteration)
			{
				wchar_t c = 0;
				if (rightToLeft)
				{
					c = cursor[i];
				}
				else
				{
					c = text[i];
				}

				// Draw this character.
				switch (c)
				{
				case L' ':
					xPos += _advance;
					break;
				case L'\r':
				case L'\n':
					yPos += size;
					xPos = x;
					break;
				case L'\t':
					xPos += _advance * 4;
					break;
				default:
					Glyph* g = getGlyph(c);
					if (getFormat() == Font::DISTANCE_FIELD)
					{
						if (_cutoffParam == NULL)
							_cutoffParam = _batch->getMaterial()->getParameter("u_cutoff");
						// TODO: Fix me so that smaller font are much smoother
						_cutoffParam->setVector2(Vector2(1.0, 1.0));
					}
					_batch->draw(xPos + (int)(g->bearingX * scale), yPos, g->width * scale, size, g->uvs[0], g->uvs[1], g->uvs[2], g->uvs[3], color);
					xPos += floor(g->advance * scale + spacing);
					break;
				}
			}

			if (rightToLeft)
			{
				cursor += length;
			}
			else
			{
				done = true;
			}
		}
	}

	void UFont::drawText(const char* text, int x, int y, float red, float green, float blue, float alpha, unsigned int size, bool rightToLeft)
	{
		drawText(text, x, y, Vector4(red, green, blue, alpha), size, rightToLeft);
	}

	void UFont::drawText(const wchar_t* text, int x, int y, float red, float green, float blue, float alpha, unsigned int size, bool rightToLeft)
	{
		drawText(text, x, y, Vector4(red, green, blue, alpha), size, rightToLeft);
	}

	void UFont::drawText(const char* text, const Rectangle& area, const Vector4& color, unsigned int size, Font::Justify justify, bool wrap, bool rightToLeft, const Rectangle& clip)
	{
		GP_ASSERT(text);
		GP_ASSERT(_size);

		if (size == 0)
		{
			size = _size;
		}
		else
		{
			// Delegate to closest sized font
			Font* f = findClosestSize(size);
			if (f != this)
			{
				f->drawText(text, area, color, size, justify, wrap, rightToLeft, clip);
				return;
			}
		}

		lazyStart();

		float scale = (float)size / _size;
		int spacing = (int)(size * _spacing);
		int yPos = area.y;
		const float areaHeight = area.height - size;
		std::vector<int> xPositions;
		std::vector<unsigned int> lineLengths;

		int len = strlen(text);
		wchar_t *wbuf = new wchar_t[len + 1];
		//int wlen = utf82wchar(text, len, wbuf, len);
        int wlen = UTF82Unicode(text, wbuf , len);
		wbuf[wlen] = 0x0;

		getMeasurementInfo(wbuf, area, size, justify, wrap, rightToLeft, &xPositions, &yPos, &lineLengths);

		// Now we have the info we need in order to render.
		int xPos = area.x;
		std::vector<int>::const_iterator xPositionsIt = xPositions.begin();
		if (xPositionsIt != xPositions.end())
		{
			xPos = *xPositionsIt++;
		}

		const wchar_t* token = wbuf;
		int iteration = 1;
		unsigned int lineLength;
		unsigned int currentLineLength = 0;
		const wchar_t* lineStart;
		std::vector<unsigned int>::const_iterator lineLengthsIt;
		if (rightToLeft)
		{
			lineStart = token;
			lineLengthsIt = lineLengths.begin();
			lineLength = *lineLengthsIt++;
			token += lineLength - 1;
			iteration = -1;
		}

		while (token[0] != 0)
		{
			// Handle delimiters until next token.
			if (!handleDelimiters(&token, size, iteration, area.x, &xPos, &yPos, &currentLineLength, &xPositionsIt, xPositions.end()))
			{
				break;
			}

			bool truncated = false;
			unsigned int tokenLength;
			unsigned int tokenWidth;
			unsigned int startIndex;
			if (rightToLeft)
			{
				tokenLength = getReversedTokenLength(token, wbuf);
				currentLineLength += tokenLength;
				token -= (tokenLength - 1);
				tokenWidth = getTokenWidth(token, tokenLength, size, scale);
				iteration = -1;
				startIndex = tokenLength - 1;
			}
			else
			{
				tokenLength = (unsigned int)wcscspn(token, L"\r\n\t");
				tokenWidth = getTokenWidth(token, tokenLength, size, scale);
				iteration = 1;
				startIndex = 0;
			}

			// Wrap if necessary.
			if (wrap && (xPos + (int)tokenWidth > area.x + area.width || (rightToLeft && currentLineLength > lineLength)))
			{
				yPos += (int)size;
				currentLineLength = tokenLength;

				if (xPositionsIt != xPositions.end())
				{
					xPos = *xPositionsIt++;
				}
				else
				{
					xPos = area.x;
				}
			}

			bool draw = true;
			if (yPos < static_cast<int>(area.y - size))
			{
				// Skip drawing until line break or wrap.
				draw = false;
			}
			else if (yPos > area.y + areaHeight)
			{
				// Truncate below area's vertical limit.
				break;
			}

			GP_ASSERT(_batch);
			for (int i = startIndex; i < (int)tokenLength && i >= 0; i += iteration)
			{
				wchar_t c = token[i];
				if (c == L' ') {
					if (xPos + (int)(this->_advance * scale) > area.x + area.width)
					{
						// Truncate this line and go on to the next one.
						truncated = true;
						break;
					}
					xPos += this->_advance;
					continue;
				}
				Glyph* g = getGlyph(c);

				if (xPos + (int)(g->advance*scale) > area.x + area.width)
				{
					// Truncate this line and go on to the next one.
					truncated = true;
					break;
				}
				else if (xPos >= (int)area.x)
				{
					// Draw this character.
					if (draw)
					{
						if (getFormat() == Font::DISTANCE_FIELD)
						{
							if (_cutoffParam == NULL)
								_cutoffParam = _batch->getMaterial()->getParameter("u_cutoff");
							// TODO: Fix me so that smaller font are much smoother
							_cutoffParam->setVector2(Vector2(1.0, 1.0));
						}
						if (clip != Rectangle(0, 0, 0, 0))
						{
							_batch->draw(xPos + (int)(g->bearingX * scale), yPos, g->width * scale, size, g->uvs[0], g->uvs[1], g->uvs[2], g->uvs[3], color, clip);
						}
						else
						{
							_batch->draw(xPos + (int)(g->bearingX * scale), yPos, g->width * scale, size, g->uvs[0], g->uvs[1], g->uvs[2], g->uvs[3], color);
						}
					}
				}
				xPos += (int)(g->advance)*scale + spacing;

			}

			if (!truncated)
			{
				if (rightToLeft)
				{
					if (token == lineStart)
					{
						token += lineLength;

						// Now handle delimiters going forwards.
						if (!handleDelimiters(&token, size, 1, area.x, &xPos, &yPos, &currentLineLength, &xPositionsIt, xPositions.end()))
						{
							break;
						}

						if (lineLengthsIt != lineLengths.end())
						{
							lineLength = *lineLengthsIt++;
						}
						lineStart = token;
						token += lineLength - 1;
					}
					else
					{
						token--;
					}
				}
				else
				{
					token += tokenLength;
				}
			}
			else
			{
				if (rightToLeft)
				{
					token = lineStart + lineLength;

					if (!handleDelimiters(&token, size, 1, area.x, &xPos, &yPos, &currentLineLength, &xPositionsIt, xPositions.end()))
					{
						break;
					}

					if (lineLengthsIt != lineLengths.end())
					{
						lineLength = *lineLengthsIt++;
					}
					lineStart = token;
					token += lineLength - 1;
				}
				else
				{
					// Skip the rest of this line.
					size_t tokenLength = wcscspn(token, L"\r\n");

					if (tokenLength > 0)
					{
						// Get first token of next line.
						token += tokenLength;
					}
				}
			}
		}

		delete[] wbuf;
	}

	void UFont::drawText(const wchar_t* text, const Rectangle& area, const Vector4& color, unsigned int size, Font::Justify justify, bool wrap, bool rightToLeft, const Rectangle& clip)
	{
		GP_ASSERT(text);
		GP_ASSERT(_size);

		if (size == 0)
		{
			size = _size;
		}
		else
		{
			// Delegate to closest sized font
			Font* f = findClosestSize(size);
			if (f != this)
			{
				f->drawText(text, area, color, size, justify, wrap, rightToLeft, clip);
				return;
			}
		}

		lazyStart();

		float scale = (float)size / _size;
		int spacing = (int)(size * _spacing);
		int yPos = area.y;
		const float areaHeight = area.height - size;
		std::vector<int> xPositions;
		std::vector<unsigned int> lineLengths;

		int len = wcslen(text);

		getMeasurementInfo(text, area, size, justify, wrap, rightToLeft, &xPositions, &yPos, &lineLengths);

		// Now we have the info we need in order to render.
		int xPos = area.x;
		std::vector<int>::const_iterator xPositionsIt = xPositions.begin();
		if (xPositionsIt != xPositions.end())
		{
			xPos = *xPositionsIt++;
		}

		const wchar_t* token = text;
		int iteration = 1;
		unsigned int lineLength;
		unsigned int currentLineLength = 0;
		const wchar_t* lineStart;
		std::vector<unsigned int>::const_iterator lineLengthsIt;
		if (rightToLeft)
		{
			lineStart = token;
			lineLengthsIt = lineLengths.begin();
			lineLength = *lineLengthsIt++;
			token += lineLength - 1;
			iteration = -1;
		}

		while (token[0] != 0)
		{
			// Handle delimiters until next token.
			if (!handleDelimiters(&token, size, iteration, area.x, &xPos, &yPos, &currentLineLength, &xPositionsIt, xPositions.end()))
			{
				break;
			}

			bool truncated = false;
			unsigned int tokenLength;
			unsigned int tokenWidth;
			unsigned int startIndex;
			if (rightToLeft)
			{
				tokenLength = getReversedTokenLength(token, text);
				currentLineLength += tokenLength;
				token -= (tokenLength - 1);
				tokenWidth = getTokenWidth(token, tokenLength, size, scale);
				iteration = -1;
				startIndex = tokenLength - 1;
			}
			else
			{
				tokenLength = (unsigned int)wcscspn(token, L"\r\n\t");
				tokenWidth = getTokenWidth(token, tokenLength, size, scale);
				iteration = 1;
				startIndex = 0;
			}

			// Wrap if necessary.
			if (wrap && (xPos + (int)tokenWidth > area.x + area.width || (rightToLeft && currentLineLength > lineLength)))
			{
				yPos += (int)size;
				currentLineLength = tokenLength;

				if (xPositionsIt != xPositions.end())
				{
					xPos = *xPositionsIt++;
				}
				else
				{
					xPos = area.x;
				}
			}

			bool draw = true;
			if (yPos < static_cast<int>(area.y - size))
			{
				// Skip drawing until line break or wrap.
				draw = false;
			}
			else if (yPos > area.y + areaHeight)
			{
				// Truncate below area's vertical limit.
				break;
			}

			GP_ASSERT(_batch);
			for (int i = startIndex; i < (int)tokenLength && i >= 0; i += iteration)
			{
				wchar_t c = token[i];
				if (c == L' ') {
					if (xPos + (int)(this->_advance * scale) > area.x + area.width)
					{
						// Truncate this line and go on to the next one.
						truncated = true;
						break;
					}
					xPos += this->_advance;
					continue;
				}

				Glyph* g = getGlyph(c);

				if (xPos + (int)(g->advance*scale) > area.x + area.width)
				{
					// Truncate this line and go on to the next one.
					truncated = true;
					break;
				}
				else if (xPos >= (int)area.x)
				{
					// Draw this character.
					if (draw)
					{
						if (getFormat() == Font::DISTANCE_FIELD)
						{
							if (_cutoffParam == NULL)
								_cutoffParam = _batch->getMaterial()->getParameter("u_cutoff");
							// TODO: Fix me so that smaller font are much smoother
							_cutoffParam->setVector2(Vector2(1.0, 1.0));
						}
						if (clip != Rectangle(0, 0, 0, 0))
						{
							_batch->draw(xPos + (int)(g->bearingX * scale), yPos, g->width * scale, size, g->uvs[0], g->uvs[1], g->uvs[2], g->uvs[3], color, clip);
						}
						else
						{
							_batch->draw(xPos + (int)(g->bearingX * scale), yPos, g->width * scale, size, g->uvs[0], g->uvs[1], g->uvs[2], g->uvs[3], color);
						}
					}
				}
				xPos += (int)(g->advance)*scale + spacing;

			}

			if (!truncated)
			{
				if (rightToLeft)
				{
					if (token == lineStart)
					{
						token += lineLength;

						// Now handle delimiters going forwards.
						if (!handleDelimiters(&token, size, 1, area.x, &xPos, &yPos, &currentLineLength, &xPositionsIt, xPositions.end()))
						{
							break;
						}

						if (lineLengthsIt != lineLengths.end())
						{
							lineLength = *lineLengthsIt++;
						}
						lineStart = token;
						token += lineLength - 1;
					}
					else
					{
						token--;
					}
				}
				else
				{
					token += tokenLength;
				}
			}
			else
			{
				if (rightToLeft)
				{
					token = lineStart + lineLength;
					if (!handleDelimiters(&token, size, 1, area.x, &xPos, &yPos, &currentLineLength, &xPositionsIt, xPositions.end()))
					{
						break;
					}

					if (lineLengthsIt != lineLengths.end())
					{
						lineLength = *lineLengthsIt++;
					}
					lineStart = token;
					token += lineLength - 1;
				}
				else
				{
					// Skip the rest of this line.
					size_t tokenLength = wcscspn(token, L"\r\n");

					if (tokenLength > 0)
					{
						// Get first token of next line.
						token += tokenLength;
					}
				}
			}
		}
	}

	void UFont::measureText(const char* text, unsigned int size, unsigned int* width, unsigned int* height)
	{
		GP_ASSERT(_size);
		GP_ASSERT(text);
		GP_ASSERT(width);
		GP_ASSERT(height);

		if (size == 0)
		{
			size = _size;
		}
		else
		{
			// Delegate to closest sized font
			Font* f = findClosestSize(size);
			if (f != this)
			{
				f->measureText(text, size, width, height);
				return;
			}
		}

		const size_t length = strlen(text);
		if (length == 0)
		{
			*width = 0;
			*height = 0;
			return;
		}

		float scale = (float)size / _size;

		wchar_t *wbuf = new wchar_t[length + 1];
		//int wlen = utf82wchar(text, length, wbuf, length);
        int wlen = UTF82Unicode(text, wbuf , length);
		wbuf[wlen] = 0x0;

		const wchar_t* token = wbuf;

		*width = 0;
		*height = size;

		// Measure a line at a time.
		while (token[0] != 0)
		{
			while (token[0] == L'\n' || token[0] == L'\r')
			{
				*height += size;
				++token;
			}

			unsigned int tokenLength = (unsigned int)wcscspn(token, L"\n\r");
			unsigned int tokenWidth = getTokenWidth(token, tokenLength, size, scale);
			if (tokenWidth > *width)
			{
				*width = tokenWidth;
			}

			token += tokenLength;
		}
		delete[] wbuf;
	}

	void UFont::measureText(const char* text, const Rectangle& clip, unsigned int size, Rectangle* out, Font::Justify justify, bool wrap, bool ignoreClip)
	{
		GP_ASSERT(_size);
		GP_ASSERT(text);
		GP_ASSERT(out);

		if (size == 0)
		{
			size = _size;
		}
		else
		{
			// Delegate to closest sized font
			Font* f = findClosestSize(size);
			if (f != this)
			{
				f->measureText(text, clip, size, out, justify, wrap, ignoreClip);
				return;
			}
		}

		const size_t length = strlen(text);
		if (length == 0)
		{
			out->set(0, 0, 0, 0);
			return;
		}

		wchar_t *wbuf = new wchar_t[length+1];
		//int wlen = utf82wchar(text, length, wbuf, length);
        int wlen = UTF82Unicode(text, wbuf , length);
		wbuf[wlen] = 0x0;

		const wchar_t* token = wbuf;

		float scale = (float)size / _size;
		Font::Justify vAlign = static_cast<Font::Justify>(justify & 0xF0);
		if (vAlign == 0)
		{
			vAlign = Font::ALIGN_TOP;
		}

		Font::Justify hAlign = static_cast<Font::Justify>(justify & 0x0F);
		if (hAlign == 0)
		{
			hAlign = Font::ALIGN_LEFT;
		}

		std::vector<bool> emptyLines;
		std::vector<Vector2> lines;

		unsigned int lineWidth = 0;
		int yPos = clip.y + size;
		const float viewportHeight = clip.height;

		if (wrap)
		{
			unsigned int delimWidth = 0;
			bool reachedEOF = false;
			while (token[0] != 0)
			{
				// Handle delimiters until next token.
				wchar_t delimiter = token[0];
				while (delimiter == L' ' ||
					delimiter == L'\t' ||
					delimiter == L'\r' ||
					delimiter == L'\n' ||
					delimiter == 0)
				{
					switch (delimiter)
					{
					case L' ':
						delimWidth += _advance;
						break;
					case L'\r':
					case L'\n':
						// Add line-height to vertical cursor.
						yPos += size;

						if (lineWidth > 0)
						{
							// Determine horizontal position and width.
							int hWhitespace = clip.width - lineWidth;
							int xPos = clip.x;
							if (hAlign == Font::ALIGN_HCENTER)
							{
								xPos += hWhitespace / 2;
							}
							else if (hAlign == Font::ALIGN_RIGHT)
							{
								xPos += hWhitespace;
							}

							// Record this line's size.
							emptyLines.push_back(false);
							lines.push_back(Vector2(xPos, lineWidth));
						}
						else
						{
							// Record the existence of an empty line.
							emptyLines.push_back(true);
							lines.push_back(Vector2(FLT_MAX, 0));
						}

						lineWidth = 0;
						delimWidth = 0;
						break;
					case L'\t':
						delimWidth += _advance * 4;
						break;
					case 0:
						reachedEOF = true;
						break;
					}

					if (reachedEOF)
					{
						break;
					}

					token++;
					delimiter = token[0];
				}

				if (reachedEOF)
				{
					break;
				}

				// Measure the next token.
				unsigned int tokenLength = (unsigned int)wcscspn(token, L" \r\n\t");
				unsigned int tokenWidth = getTokenWidth(token, tokenLength, size, scale);

				// Wrap if necessary.
				if (lineWidth + tokenWidth + delimWidth > clip.width)
				{
					// Add line-height to vertical cursor.
					yPos += size;

					// Determine horizontal position and width.
					int hWhitespace = clip.width - lineWidth;
					int xPos = clip.x;
					if (hAlign == Font::ALIGN_HCENTER)
					{
						xPos += hWhitespace / 2;
					}
					else if (hAlign == Font::ALIGN_RIGHT)
					{
						xPos += hWhitespace;
					}

					// Record this line's size.
					emptyLines.push_back(false);
					lines.push_back(Vector2(xPos, lineWidth));
					lineWidth = 0;
				}
				else
				{
					lineWidth += delimWidth;
				}

				delimWidth = 0;
				lineWidth += tokenWidth;
				token += tokenLength;
			}
		}
		else
		{
			// Measure a whole line at a time.
			int emptyLinesCount = 0;
			while (token[0] != 0)
			{
				// Handle any number of consecutive newlines.
				bool nextLine = true;
				while (token[0] == L'\n' || token[0] == L'\r')
				{
					if (nextLine)
					{
						// Add line-height to vertical cursor.
						yPos += size * (emptyLinesCount + 1);
						nextLine = false;
						emptyLinesCount = 0;
						emptyLines.push_back(false);
					}
					else
					{
						// Record the existence of an empty line.
						++emptyLinesCount;
						emptyLines.push_back(true);
						lines.push_back(Vector2(FLT_MAX, 0));
					}

					token++;
				}

				// Measure the next line.
				unsigned int tokenLength = (unsigned int)wcscspn(token, L"\n");
				lineWidth = getTokenWidth(token, tokenLength, size, scale);

				// Determine horizontal position and width.
				int xPos = clip.x;
				int hWhitespace = clip.width - lineWidth;
				if (hAlign == Font::ALIGN_HCENTER)
				{
					xPos += hWhitespace / 2;
				}
				else if (hAlign == Font::ALIGN_RIGHT)
				{
					xPos += hWhitespace;
				}

				// Record this line's size.
				lines.push_back(Vector2(xPos, lineWidth));

				token += tokenLength;
			}

			yPos += size;
		}

		if (wrap)
		{
			// Record the size of the last line.
			int hWhitespace = clip.width - lineWidth;
			int xPos = clip.x;
			if (hAlign == Font::ALIGN_HCENTER)
			{
				xPos += hWhitespace / 2;
			}
			else if (hAlign == Font::ALIGN_RIGHT)
			{
				xPos += hWhitespace;
			}

			lines.push_back(Vector2(xPos, lineWidth));
		}

		int x = INT_MAX;
		int y = clip.y;
		unsigned int width = 0;
		int height = yPos - clip.y;

		// Calculate top of text without clipping.
		int vWhitespace = viewportHeight - height;
		if (vAlign == Font::ALIGN_VCENTER)
		{
			y += vWhitespace / 2;
		}
		else if (vAlign == Font::ALIGN_BOTTOM)
		{
			y += vWhitespace;
		}

		int clippedTop = 0;
		int clippedBottom = 0;
		if (!ignoreClip)
		{
			// Trim rect to fit text that would actually be drawn within the given clip.
			if (y >= clip.y)
			{
				// Text goes off the bottom of the clip.
				clippedBottom = (height - viewportHeight) / size + 1;
				if (clippedBottom > 0)
				{
					// Also need to crop empty lines above non-empty lines that have been clipped.
					size_t emptyIndex = emptyLines.size() - clippedBottom;
					while (emptyIndex < emptyLines.size() && emptyLines[emptyIndex] == true)
					{
						height -= size;
						emptyIndex++;
					}

					height -= size * clippedBottom;
				}
				else
				{
					clippedBottom = 0;
				}
			}
			else
			{
				// Text goes above the top of the clip.
				clippedTop = (clip.y - y) / size + 1;
				if (clippedTop < 0)
				{
					clippedTop = 0;
				}

				// Also need to crop empty lines below non-empty lines that have been clipped.
				size_t emptyIndex = clippedTop;
				while (emptyIndex < emptyLines.size() && emptyLines[emptyIndex] == true)
				{
					y += size;
					height -= size;
					emptyIndex++;
				}

				if (vAlign == Font::ALIGN_VCENTER)
				{
					// In this case lines may be clipped off the bottom as well.
					clippedBottom = (height - viewportHeight + vWhitespace / 2 + 0.01) / size + 1;
					if (clippedBottom > 0)
					{
						emptyIndex = emptyLines.size() - clippedBottom;
						while (emptyIndex < emptyLines.size() && emptyLines[emptyIndex] == true)
						{
							height -= size;
							emptyIndex++;
						}

						height -= size * clippedBottom;
					}
					else
					{
						clippedBottom = 0;
					}
				}

				y = y + size * clippedTop;
				height = height - size * clippedTop;
			}
		}

		// Determine left-most x coordinate and largest width out of lines that have not been clipped.
		for (int i = clippedTop; i < (int)lines.size() - clippedBottom; ++i)
		{
			if (lines[i].x < x)
			{
				x = lines[i].x;
			}
			if (lines[i].y > width)
			{
				width = lines[i].y;
			}
		}

		if (!ignoreClip)
		{
			// Guarantee that the output rect will fit within the clip.
			out->x = (x >= clip.x) ? x : clip.x;
			out->y = (y >= clip.y) ? y : clip.y;
			out->width = (width <= clip.width) ? width : clip.width;
			out->height = (height <= viewportHeight) ? height : viewportHeight;
		}
		else
		{
			out->x = x;
			out->y = y;
			out->width = width;
			out->height = height;
		}
		delete[] wbuf;
	}

	void UFont::measureText(const wchar_t* text, unsigned int size, unsigned int* width, unsigned int* height)
	{
		GP_ASSERT(_size);
		GP_ASSERT(text);
		GP_ASSERT(width);
		GP_ASSERT(height);

		if (size == 0)
		{
			size = _size;
		}
		else
		{
			// Delegate to closest sized font
			Font* f = findClosestSize(size);
			if (f != this)
			{
				f->measureText(text, size, width, height);
				return;
			}
		}

		const size_t length = wcslen(text);
		if (length == 0)
		{
			*width = 0;
			*height = 0;
			return;
		}

		float scale = (float)size / _size;

		const wchar_t* token = text;

		*width = 0;
		*height = size;

		// Measure a line at a time.
		while (token[0] != 0)
		{
			while (token[0] == L'\n' || token[0] == L'\r')
			{
				*height += size;
				++token;
			}

			unsigned int tokenLength = (unsigned int)wcscspn(token, L"\n\r");
			unsigned int tokenWidth = getTokenWidth(token, tokenLength, size, scale);
			if (tokenWidth > *width)
			{
				*width = tokenWidth;
			}

			token += tokenLength;
		}
	}

	void UFont::measureText(const wchar_t* text, const Rectangle& clip, unsigned int size, Rectangle* out, Font::Justify justify, bool wrap, bool ignoreClip)
	{
		GP_ASSERT(_size);
		GP_ASSERT(text);
		GP_ASSERT(out);

		if (size == 0)
		{
			size = _size;
		}
		else
		{
			// Delegate to closest sized font
			Font* f = findClosestSize(size);
			if (f != this)
			{
				f->measureText(text, clip, size, out, justify, wrap, ignoreClip);
				return;
			}
		}

		const size_t length = wcslen(text);
		if (length == 0)
		{
			out->set(0, 0, 0, 0);
			return;
		}

		const wchar_t* token = text;

		float scale = (float)size / _size;
		Font::Justify vAlign = static_cast<Font::Justify>(justify & 0xF0);
		if (vAlign == 0)
		{
			vAlign = Font::ALIGN_TOP;
		}

		Font::Justify hAlign = static_cast<Font::Justify>(justify & 0x0F);
		if (hAlign == 0)
		{
			hAlign = Font::ALIGN_LEFT;
		}

		std::vector<bool> emptyLines;
		std::vector<Vector2> lines;

		unsigned int lineWidth = 0;
		int yPos = clip.y + size;
		const float viewportHeight = clip.height;

		if (wrap)
		{
			unsigned int delimWidth = 0;
			bool reachedEOF = false;
			while (token[0] != 0)
			{
				// Handle delimiters until next token.
				wchar_t delimiter = token[0];
				while (delimiter == L' ' ||
					delimiter == L'\t' ||
					delimiter == L'\r' ||
					delimiter == L'\n' ||
					delimiter == 0)
				{
					switch (delimiter)
					{
					case L' ':
						delimWidth += _advance;
						break;
					case L'\r':
					case L'\n':
						// Add line-height to vertical cursor.
						yPos += size;

						if (lineWidth > 0)
						{
							// Determine horizontal position and width.
							int hWhitespace = clip.width - lineWidth;
							int xPos = clip.x;
							if (hAlign == Font::ALIGN_HCENTER)
							{
								xPos += hWhitespace / 2;
							}
							else if (hAlign == Font::ALIGN_RIGHT)
							{
								xPos += hWhitespace;
							}

							// Record this line's size.
							emptyLines.push_back(false);
							lines.push_back(Vector2(xPos, lineWidth));
						}
						else
						{
							// Record the existence of an empty line.
							emptyLines.push_back(true);
							lines.push_back(Vector2(FLT_MAX, 0));
						}

						lineWidth = 0;
						delimWidth = 0;
						break;
					case L'\t':
						delimWidth += _advance * 4;
						break;
					case 0:
						reachedEOF = true;
						break;
					}

					if (reachedEOF)
					{
						break;
					}

					token++;
					delimiter = token[0];
				}

				if (reachedEOF)
				{
					break;
				}

				// Measure the next token.
				unsigned int tokenLength = (unsigned int)wcscspn(token, L" \r\n\t");
				unsigned int tokenWidth = getTokenWidth(token, tokenLength, size, scale);

				// Wrap if necessary.
				if (lineWidth + tokenWidth + delimWidth > clip.width)
				{
					// Add line-height to vertical cursor.
					yPos += size;

					// Determine horizontal position and width.
					int hWhitespace = clip.width - lineWidth;
					int xPos = clip.x;
					if (hAlign == Font::ALIGN_HCENTER)
					{
						xPos += hWhitespace / 2;
					}
					else if (hAlign == Font::ALIGN_RIGHT)
					{
						xPos += hWhitespace;
					}

					// Record this line's size.
					emptyLines.push_back(false);
					lines.push_back(Vector2(xPos, lineWidth));
					lineWidth = 0;
				}
				else
				{
					lineWidth += delimWidth;
				}

				delimWidth = 0;
				lineWidth += tokenWidth;
				token += tokenLength;
			}
		}
		else
		{
			// Measure a whole line at a time.
			int emptyLinesCount = 0;
			while (token[0] != 0)
			{
				// Handle any number of consecutive newlines.
				bool nextLine = true;
				while (token[0] == L'\n' || token[0] == L'\r')
				{
					if (nextLine)
					{
						// Add line-height to vertical cursor.
						yPos += size * (emptyLinesCount + 1);
						nextLine = false;
						emptyLinesCount = 0;
						emptyLines.push_back(false);
					}
					else
					{
						// Record the existence of an empty line.
						++emptyLinesCount;
						emptyLines.push_back(true);
						lines.push_back(Vector2(FLT_MAX, 0));
					}

					token++;
				}

				// Measure the next line.
				unsigned int tokenLength = (unsigned int)wcscspn(token, L"\n");
				lineWidth = getTokenWidth(token, tokenLength, size, scale);

				// Determine horizontal position and width.
				int xPos = clip.x;
				int hWhitespace = clip.width - lineWidth;
				if (hAlign == Font::ALIGN_HCENTER)
				{
					xPos += hWhitespace / 2;
				}
				else if (hAlign == Font::ALIGN_RIGHT)
				{
					xPos += hWhitespace;
				}

				// Record this line's size.
				lines.push_back(Vector2(xPos, lineWidth));

				token += tokenLength;
			}

			yPos += size;
		}

		if (wrap)
		{
			// Record the size of the last line.
			int hWhitespace = clip.width - lineWidth;
			int xPos = clip.x;
			if (hAlign == Font::ALIGN_HCENTER)
			{
				xPos += hWhitespace / 2;
			}
			else if (hAlign == Font::ALIGN_RIGHT)
			{
				xPos += hWhitespace;
			}

			lines.push_back(Vector2(xPos, lineWidth));
		}

		int x = INT_MAX;
		int y = clip.y;
		unsigned int width = 0;
		int height = yPos - clip.y;

		// Calculate top of text without clipping.
		int vWhitespace = viewportHeight - height;
		if (vAlign == Font::ALIGN_VCENTER)
		{
			y += vWhitespace / 2;
		}
		else if (vAlign == Font::ALIGN_BOTTOM)
		{
			y += vWhitespace;
		}

		int clippedTop = 0;
		int clippedBottom = 0;
		if (!ignoreClip)
		{
			// Trim rect to fit text that would actually be drawn within the given clip.
			if (y >= clip.y)
			{
				// Text goes off the bottom of the clip.
				clippedBottom = (height - viewportHeight) / size + 1;
				if (clippedBottom > 0)
				{
					// Also need to crop empty lines above non-empty lines that have been clipped.
					size_t emptyIndex = emptyLines.size() - clippedBottom;
					while (emptyIndex < emptyLines.size() && emptyLines[emptyIndex] == true)
					{
						height -= size;
						emptyIndex++;
					}

					height -= size * clippedBottom;
				}
				else
				{
					clippedBottom = 0;
				}
			}
			else
			{
				// Text goes above the top of the clip.
				clippedTop = (clip.y - y) / size + 1;
				if (clippedTop < 0)
				{
					clippedTop = 0;
				}

				// Also need to crop empty lines below non-empty lines that have been clipped.
				size_t emptyIndex = clippedTop;
				while (emptyIndex < emptyLines.size() && emptyLines[emptyIndex] == true)
				{
					y += size;
					height -= size;
					emptyIndex++;
				}

				if (vAlign == Font::ALIGN_VCENTER)
				{
					// In this case lines may be clipped off the bottom as well.
					clippedBottom = (height - viewportHeight + vWhitespace / 2 + 0.01) / size + 1;
					if (clippedBottom > 0)
					{
						emptyIndex = emptyLines.size() - clippedBottom;
						while (emptyIndex < emptyLines.size() && emptyLines[emptyIndex] == true)
						{
							height -= size;
							emptyIndex++;
						}

						height -= size * clippedBottom;
					}
					else
					{
						clippedBottom = 0;
					}
				}

				y = y + size * clippedTop;
				height = height - size * clippedTop;
			}
		}

		// Determine left-most x coordinate and largest width out of lines that have not been clipped.
		for (int i = clippedTop; i < (int)lines.size() - clippedBottom; ++i)
		{
			if (lines[i].x < x)
			{
				x = lines[i].x;
			}
			if (lines[i].y > width)
			{
				width = lines[i].y;
			}
		}

		if (!ignoreClip)
		{
			// Guarantee that the output rect will fit within the clip.
			out->x = (x >= clip.x) ? x : clip.x;
			out->y = (y >= clip.y) ? y : clip.y;
			out->width = (width <= clip.width) ? width : clip.width;
			out->height = (height <= viewportHeight) ? height : viewportHeight;
		}
		else
		{
			out->x = x;
			out->y = y;
			out->width = width;
			out->height = height;
		}
	}

	void UFont::getMeasurementInfo(const wchar_t* text, const Rectangle& area, unsigned int size, Font::Justify justify, bool wrap, bool rightToLeft,
		std::vector<int>* xPositions, int* yPosition, std::vector<unsigned int>* lineLengths)
	{
		GP_ASSERT(_size);
		GP_ASSERT(text);
		GP_ASSERT(yPosition);

		if (size == 0)
			size = _size;

		float scale = (float)size / _size;

		Font::Justify vAlign = static_cast<Font::Justify>(justify & 0xF0);
		if (vAlign == 0)
		{
			vAlign = Font::ALIGN_TOP;
		}

		Font::Justify hAlign = static_cast<Font::Justify>(justify & 0x0F);
		if (hAlign == 0)
		{
			hAlign = Font::ALIGN_LEFT;
		}

		const wchar_t* token = text;
		const float areaHeight = area.height - size;

		// For alignments other than top-left, need to calculate the y position to begin drawing from
		// and the starting x position of each line.  For right-to-left text, need to determine
		// the number of characters on each line.
		if (vAlign != Font::ALIGN_TOP || hAlign != Font::ALIGN_LEFT || rightToLeft)
		{
			int lineWidth = 0;
			int delimWidth = 0;

			if (wrap)
			{
				// Go a word at a time.
				bool reachedEOF = false;
				unsigned int lineLength = 0;
				while (token[0] != 0)
				{
					unsigned int tokenWidth = 0;

					// Handle delimiters until next token.
					wchar_t delimiter = token[0];
					while (delimiter == L' ' ||
						delimiter == L'\t' ||
						delimiter == L'\r' ||
						delimiter == L'\n' ||
						delimiter == 0)
					{
						switch (delimiter)
						{
						case L' ':
							delimWidth += _advance;
							lineLength++;
							break;
						case L'\r':
						case L'\n':
							*yPosition += size;

							if (lineWidth > 0)
							{
								addLineInfo(area, lineWidth, lineLength, hAlign, xPositions, lineLengths, rightToLeft);
							}

							lineWidth = 0;
							lineLength = 0;
							delimWidth = 0;
							break;
						case L'\t':
							delimWidth += _advance * 4;
							lineLength++;
							break;
						case 0:
							reachedEOF = true;
							break;
						}

						if (reachedEOF)
						{
							break;
						}

						token++;
						delimiter = token[0];
					}

					if (reachedEOF || token == NULL)
					{
						break;
					}

					unsigned int tokenLength = (unsigned int)wcscspn(token, L" \r\n\t");

					tokenWidth += getTokenWidth(token, tokenLength, size, scale);

					// Wrap if necessary.
					if (lineWidth + tokenWidth + delimWidth > area.width)
					{
						*yPosition += size;

						// Push position of current line.
						if (lineLength)
						{
							addLineInfo(area, lineWidth, lineLength - 1, hAlign, xPositions, lineLengths, rightToLeft);
						}
						else
						{
							addLineInfo(area, lineWidth, tokenLength, hAlign, xPositions, lineLengths, rightToLeft);
						}

						// Move token to the next line.
						lineWidth = 0;
						lineLength = 0;
						delimWidth = 0;
					}
					else
					{
						lineWidth += delimWidth;
						delimWidth = 0;
					}

					lineWidth += tokenWidth;
					lineLength += tokenLength;
					token += tokenLength;
				}

				// Final calculation of vertical position.
				int textHeight = *yPosition - area.y;
				int vWhiteSpace = areaHeight - textHeight;
				if (vAlign == Font::ALIGN_VCENTER)
				{
					*yPosition = area.y + vWhiteSpace / 2;
				}
				else if (vAlign == Font::ALIGN_BOTTOM)
				{
					*yPosition = area.y + vWhiteSpace;
				}

				// Calculation of final horizontal position.
				addLineInfo(area, lineWidth, lineLength, hAlign, xPositions, lineLengths, rightToLeft);
			}
			else
			{
				// Go a line at a time.
				while (token[0] != 0)
				{
					wchar_t delimiter = token[0];
					while (delimiter == '\n' || delimiter == '\r')
					{
						*yPosition += size;
						++token;
						delimiter = token[0];
					}

					unsigned int tokenLength = (unsigned int)wcscspn(token, L"\n\r");
					if (tokenLength == 0)
					{
						tokenLength = (unsigned int)wcslen(token);
					}

					int lineWidth = getTokenWidth(token, tokenLength, size, scale);
					addLineInfo(area, lineWidth, tokenLength, hAlign, xPositions, lineLengths, rightToLeft);

					token += tokenLength;
				}

				int textHeight = *yPosition - area.y;
				int vWhiteSpace = areaHeight - textHeight;
				if (vAlign == Font::ALIGN_VCENTER)
				{
					*yPosition = area.y + vWhiteSpace / 2;
				}
				else if (vAlign == Font::ALIGN_BOTTOM)
				{
					*yPosition = area.y + vWhiteSpace;
				}
			}

			if (vAlign == Font::ALIGN_TOP)
			{
				*yPosition = area.y;
			}
		}
	}

	int UFont::getIndexAtLocation(const char* text, const Rectangle& area, unsigned int size, const Vector2& inLocation, Vector2* outLocation,
		Font::Justify justify, bool wrap, bool rightToLeft)
	{
		return getIndexOrLocation(text, area, size, inLocation, outLocation, -1, justify, wrap, rightToLeft);
	}

	int UFont::getIndexAtLocation(const wchar_t* text, const Rectangle& area, unsigned int size, const Vector2& inLocation, Vector2* outLocation,
		Font::Justify justify, bool wrap, bool rightToLeft)
	{
		return getIndexOrLocation(text, area, size, inLocation, outLocation, -1, justify, wrap, rightToLeft);
	}

	void UFont::getLocationAtIndex(const char* text, const Rectangle& clip, unsigned int size, Vector2* outLocation, const unsigned int destIndex,
		Font::Justify justify, bool wrap, bool rightToLeft)
	{
		getIndexOrLocation(text, clip, size, *outLocation, outLocation, (const int)destIndex, justify, wrap, rightToLeft);
	}

	void UFont::getLocationAtIndex(const wchar_t* text, const Rectangle& clip, unsigned int size, Vector2* outLocation, const unsigned int destIndex,
		Font::Justify justify, bool wrap, bool rightToLeft)
	{
		getIndexOrLocation(text, clip, size, *outLocation, outLocation, (const int)destIndex, justify, wrap, rightToLeft);
	}

	int UFont::getIndexOrLocation(const char* text, const Rectangle& area, unsigned int size, const Vector2& inLocation, Vector2* outLocation,
		const int destIndex, Font::Justify justify, bool wrap, bool rightToLeft)
	{
		GP_ASSERT(_size);
		GP_ASSERT(text);
		GP_ASSERT(outLocation);

		if (size == 0)
		{
			size = _size;
		}
		else
		{
			// Delegate to closest sized font
			Font* f = findClosestSize(size);
			if (f != this)
			{
				return f->getIndexOrLocation(text, area, size, inLocation, outLocation, destIndex, justify, wrap, rightToLeft);
			}
		}

		unsigned int charIndex = 0;

		// Essentially need to measure text until we reach inLocation.
		float scale = (float)size / _size;
		int spacing = (int)(size * _spacing);
		int yPos = area.y;
		const float areaHeight = area.height - size;
		std::vector<int> xPositions;
		std::vector<unsigned int> lineLengths;

		const size_t length = strlen(text);
		wchar_t *wbuf = new wchar_t[length];
		//int wlen = utf82wchar(text, length, wbuf, length);
        int wlen = UTF82Unicode(text, wbuf , length);
		getMeasurementInfo(wbuf, area, size, justify, wrap, rightToLeft, &xPositions, &yPos, &lineLengths);

		int xPos = area.x;
		std::vector<int>::const_iterator xPositionsIt = xPositions.begin();
		if (xPositionsIt != xPositions.end())
		{
			xPos = *xPositionsIt++;
		}

		const wchar_t* token = wbuf;

		int iteration = 1;
		unsigned int lineLength;
		unsigned int currentLineLength = 0;
		const wchar_t* lineStart;
		std::vector<unsigned int>::const_iterator lineLengthsIt;
		if (rightToLeft)
		{
			lineStart = token;
			lineLengthsIt = lineLengths.begin();
			lineLength = *lineLengthsIt++;
			token += lineLength - 1;
			iteration = -1;
		}

		while (token[0] != 0)
		{
			// Handle delimiters until next token.
			unsigned int delimLength = 0;
			int result;
			if (destIndex == -1)
			{
				result = handleDelimiters(&token, size, iteration, area.x, &xPos, &yPos, &delimLength, &xPositionsIt, xPositions.end(), &charIndex, &inLocation);
			}
			else
			{
				result = handleDelimiters(&token, size, iteration, area.x, &xPos, &yPos, &delimLength, &xPositionsIt, xPositions.end(), &charIndex, NULL, charIndex, destIndex);
			}

			currentLineLength += delimLength;
			if (result == 0 || result == 2)
			{
				outLocation->x = xPos;
				outLocation->y = yPos;
				delete[] wbuf;
				return charIndex;
			}

			if (destIndex == (int)charIndex ||
				(destIndex == -1 &&
					inLocation.x >= xPos && inLocation.x < xPos + spacing &&
					inLocation.y >= yPos && inLocation.y < yPos + size))
			{
				outLocation->x = xPos;
				outLocation->y = yPos;
				delete[] wbuf;
				return charIndex;
			}

			bool truncated = false;
			unsigned int tokenLength;
			unsigned int tokenWidth;
			unsigned int startIndex;
			if (rightToLeft)
			{
				tokenLength = getReversedTokenLength(token, wbuf);
				currentLineLength += tokenLength;
				charIndex += tokenLength;
				token -= (tokenLength - 1);
				tokenWidth = getTokenWidth(token, tokenLength, size, scale);
				iteration = -1;
				startIndex = tokenLength - 1;
			}
			else
			{
				tokenLength = (unsigned int)wcscspn(token, L" \r\n\t");
				tokenWidth = getTokenWidth(token, tokenLength, size, scale);
				iteration = 1;
				startIndex = 0;
			}

			// Wrap if necessary.
			if (wrap && (xPos + (int)tokenWidth > area.x + area.width || (rightToLeft && currentLineLength > lineLength)))
			{
				yPos += size;
				currentLineLength = tokenLength;

				if (xPositionsIt != xPositions.end())
				{
					xPos = *xPositionsIt++;
				}
				else
				{
					xPos = area.x;
				}
			}

			if (yPos > area.y + areaHeight)
			{
				// Truncate below area's vertical limit.
				break;
			}


			for (int i = startIndex; i < (int)tokenLength && i >= 0; i += iteration)
			{
				wchar_t c = token[i];

				Glyph* g = getGlyph(c);

				if (xPos + (int)(g->advance*scale) > area.x + area.width)
				{
					// Truncate this line and go on to the next one.
					truncated = true;
					break;
				}

				// Check against inLocation.
				//  Note: g.width is smaller than g.advance, so if I only check against g.width, I will 
				//  miss locations towards the right of the character.
				if (destIndex == (int)charIndex ||
					(destIndex == -1 &&
						inLocation.x >= xPos && inLocation.x < floor(xPos + g->advance*scale + spacing) &&
						inLocation.y >= yPos && inLocation.y < yPos + size))
				{
					outLocation->x = xPos;
					outLocation->y = yPos;
					delete[] wbuf;
					return charIndex;
				}

				xPos += floor(g->advance*scale + spacing);
				charIndex++;
			}

			if (!truncated)
			{
				if (rightToLeft)
				{
					if (token == lineStart)
					{
						token += lineLength;

						// Now handle delimiters going forwards.
						if (!handleDelimiters(&token, size, 1, area.x, &xPos, &yPos, &currentLineLength, &xPositionsIt, xPositions.end()))
						{
							break;
						}
						charIndex += currentLineLength;

						if (lineLengthsIt != lineLengths.end())
						{
							lineLength = *lineLengthsIt++;
						}
						lineStart = token;
						token += lineLength - 1;
						charIndex += tokenLength;
					}
					else
					{
						token--;
					}
				}
				else
				{
					token += tokenLength;
				}
			}
			else
			{
				if (rightToLeft)
				{
					token = lineStart + lineLength;

					if (!handleDelimiters(&token, size, 1, area.x, &xPos, &yPos, &currentLineLength, &xPositionsIt, xPositions.end()))
					{
						break;
					}

					if (lineLengthsIt != lineLengths.end())
					{
						lineLength = *lineLengthsIt++;
					}
					lineStart = token;
					token += lineLength - 1;
				}
				else
				{
					// Skip the rest of this line.
					unsigned int tokenLength = (unsigned int)wcscspn(token, L"\n");

					if (tokenLength > 0)
					{
						// Get first token of next line.
						token += tokenLength;
						charIndex += tokenLength;
					}
				}
			}
		}
		delete[] wbuf;

		if (destIndex == (int)charIndex ||
			(destIndex == -1 &&
				inLocation.x >= xPos && inLocation.x < xPos + spacing &&
				inLocation.y >= yPos && inLocation.y < yPos + size))
		{
			outLocation->x = xPos;
			outLocation->y = yPos;
			return charIndex;
		}
		
		return -1;
	}

	int UFont::getIndexOrLocation(const wchar_t* text, const Rectangle& area, unsigned int size, const Vector2& inLocation, Vector2* outLocation,
		const int destIndex, Font::Justify justify, bool wrap, bool rightToLeft)
	{
		GP_ASSERT(_size);
		GP_ASSERT(text);
		GP_ASSERT(outLocation);

		if (size == 0)
		{
			size = _size;
		}
		else
		{
			// Delegate to closest sized font
			Font* f = findClosestSize(size);
			if (f != this)
			{
				return f->getIndexOrLocation(text, area, size, inLocation, outLocation, destIndex, justify, wrap, rightToLeft);
			}
		}

		unsigned int charIndex = 0;

		// Essentially need to measure text until we reach inLocation.
		float scale = (float)size / _size;
		int spacing = (int)(size * _spacing);
		int yPos = area.y;
		const float areaHeight = area.height - size;
		std::vector<int> xPositions;
		std::vector<unsigned int> lineLengths;

		const size_t length = wcslen(text);

		getMeasurementInfo(text, area, size, justify, wrap, rightToLeft, &xPositions, &yPos, &lineLengths);

		int xPos = area.x;
		std::vector<int>::const_iterator xPositionsIt = xPositions.begin();
		if (xPositionsIt != xPositions.end())
		{
			xPos = *xPositionsIt++;
		}

		const wchar_t* token = text;

		int iteration = 1;
		unsigned int lineLength;
		unsigned int currentLineLength = 0;
		const wchar_t* lineStart;
		std::vector<unsigned int>::const_iterator lineLengthsIt;
		if (rightToLeft)
		{
			lineStart = token;
			lineLengthsIt = lineLengths.begin();
			lineLength = *lineLengthsIt++;
			token += lineLength - 1;
			iteration = -1;
		}

		while (token[0] != 0)
		{
			// Handle delimiters until next token.
			unsigned int delimLength = 0;
			int result;
			if (destIndex == -1)
			{
				result = handleDelimiters(&token, size, iteration, area.x, &xPos, &yPos, &delimLength, &xPositionsIt, xPositions.end(), &charIndex, &inLocation);
			}
			else
			{
				result = handleDelimiters(&token, size, iteration, area.x, &xPos, &yPos, &delimLength, &xPositionsIt, xPositions.end(), &charIndex, NULL, charIndex, destIndex);
			}

			currentLineLength += delimLength;
			if (result == 0 || result == 2)
			{
				outLocation->x = xPos;
				outLocation->y = yPos;
				return charIndex;
			}

			if (destIndex == (int)charIndex ||
				(destIndex == -1 &&
					inLocation.x >= xPos && inLocation.x < xPos + spacing &&
					inLocation.y >= yPos && inLocation.y < yPos + size))
			{
				outLocation->x = xPos;
				outLocation->y = yPos;
				return charIndex;
			}

			bool truncated = false;
			unsigned int tokenLength;
			unsigned int tokenWidth;
			unsigned int startIndex;
			if (rightToLeft)
			{
				tokenLength = getReversedTokenLength(token, text);
				currentLineLength += tokenLength;
				charIndex += tokenLength;
				token -= (tokenLength - 1);
				tokenWidth = getTokenWidth(token, tokenLength, size, scale);
				iteration = -1;
				startIndex = tokenLength - 1;
			}
			else
			{
				tokenLength = (unsigned int)wcscspn(token, L" \r\n\t");
				tokenWidth = getTokenWidth(token, tokenLength, size, scale);
				iteration = 1;
				startIndex = 0;
			}

			// Wrap if necessary.
			if (wrap && (xPos + (int)tokenWidth > area.x + area.width || (rightToLeft && currentLineLength > lineLength)))
			{
				yPos += size;
				currentLineLength = tokenLength;

				if (xPositionsIt != xPositions.end())
				{
					xPos = *xPositionsIt++;
				}
				else
				{
					xPos = area.x;
				}
			}

			if (yPos > area.y + areaHeight)
			{
				// Truncate below area's vertical limit.
				break;
			}


			for (int i = startIndex; i < (int)tokenLength && i >= 0; i += iteration)
			{
				wchar_t c = token[i];

				Glyph* g = getGlyph(c);

				if (xPos + (int)(g->advance*scale) > area.x + area.width)
				{
					// Truncate this line and go on to the next one.
					truncated = true;
					break;
				}

				// Check against inLocation.
				//  Note: g.width is smaller than g.advance, so if I only check against g.width, I will 
				//  miss locations towards the right of the character.
				if (destIndex == (int)charIndex ||
					(destIndex == -1 &&
						inLocation.x >= xPos && inLocation.x < floor(xPos + g->advance*scale + spacing) &&
						inLocation.y >= yPos && inLocation.y < yPos + size))
				{
					outLocation->x = xPos;
					outLocation->y = yPos;
					return charIndex;
				}

				xPos += floor(g->advance*scale + spacing);
				charIndex++;
			}

			if (!truncated)
			{
				if (rightToLeft)
				{
					if (token == lineStart)
					{
						token += lineLength;

						// Now handle delimiters going forwards.
						if (!handleDelimiters(&token, size, 1, area.x, &xPos, &yPos, &currentLineLength, &xPositionsIt, xPositions.end()))
						{
							break;
						}
						charIndex += currentLineLength;

						if (lineLengthsIt != lineLengths.end())
						{
							lineLength = *lineLengthsIt++;
						}
						lineStart = token;
						token += lineLength - 1;
						charIndex += tokenLength;
					}
					else
					{
						token--;
					}
				}
				else
				{
					token += tokenLength;
				}
			}
			else
			{
				if (rightToLeft)
				{
					token = lineStart + lineLength;

					if (!handleDelimiters(&token, size, 1, area.x, &xPos, &yPos, &currentLineLength, &xPositionsIt, xPositions.end()))
					{
						break;
					}

					if (lineLengthsIt != lineLengths.end())
					{
						lineLength = *lineLengthsIt++;
					}
					lineStart = token;
					token += lineLength - 1;
				}
				else
				{
					// Skip the rest of this line.
					unsigned int tokenLength = (unsigned int)wcscspn(token, L"\n");

					if (tokenLength > 0)
					{
						// Get first token of next line.
						token += tokenLength;
						charIndex += tokenLength;
					}
				}
			}
		}

		if (destIndex == (int)charIndex ||
			(destIndex == -1 &&
				inLocation.x >= xPos && inLocation.x < xPos + spacing &&
				inLocation.y >= yPos && inLocation.y < yPos + size))
		{
			outLocation->x = xPos;
			outLocation->y = yPos;
			return charIndex;
		}

		return -1;
	}

	unsigned int UFont::getTokenWidth(const wchar_t* token, unsigned int length, unsigned int size, float scale)
	{
		GP_ASSERT(token);

		if (size == 0)
			size = _size;

		int spacing = (int)(size * _spacing);

		// Calculate width of word or line.
		unsigned int tokenWidth = 0;
		for (unsigned int i = 0; i < length; ++i)
		{
			wchar_t c = token[i];
			switch (c)
			{
			case L' ':
				tokenWidth += _advance;
				break;
			case L'\t':
				tokenWidth += _advance * 4;
				break;
			default:
				Glyph* g =  this->getGlyph(c);
				tokenWidth += floor(g->advance * scale + spacing);
				break;
			}
		}

		return tokenWidth;
	}

	unsigned int UFont::getReversedTokenLength(const wchar_t* token, const wchar_t* bufStart)
	{
		GP_ASSERT(token);
		GP_ASSERT(bufStart);

		const wchar_t* cursor = token;
		wchar_t c = cursor[0];
		unsigned int length = 0;

		while (cursor != bufStart && c != L' ' && c != L'\r' && c != L'\n' && c != L'\t')
		{
			length++;
			cursor--;
			c = cursor[0];
		}

		if (cursor == bufStart)
		{
			length++;
		}

		return length;
	}

	int UFont::handleDelimiters(const wchar_t** token, const unsigned int size, const int iteration, const int areaX, int* xPos, int* yPos, unsigned int* lineLength,
		std::vector<int>::const_iterator* xPositionsIt, std::vector<int>::const_iterator xPositionsEnd, unsigned int* charIndex,
		const Vector2* stopAtPosition, const int currentIndex, const int destIndex)
	{
		GP_ASSERT(token);
		GP_ASSERT(*token);
		GP_ASSERT(xPos);
		GP_ASSERT(yPos);
		GP_ASSERT(lineLength);
		GP_ASSERT(xPositionsIt);

		wchar_t delimiter = *token[0];
		bool nextLine = true;
		while (delimiter == L' ' ||
			delimiter == L'\t' ||
			delimiter == L'\r' ||
			delimiter == L'\n' ||
			delimiter == 0)
		{
			if ((stopAtPosition &&
				stopAtPosition->x >= *xPos && stopAtPosition->x < *xPos + ((int)size >> 1) &&
				stopAtPosition->y >= *yPos && stopAtPosition->y < *yPos + (int)size) ||
				(currentIndex >= 0 && destIndex >= 0 && currentIndex + (int)*lineLength == destIndex))
			{
				// Success + stopAtPosition was reached.
				return 2;
			}

			switch (delimiter)
			{
			case L' ':
				*xPos += _advance;
				(*lineLength)++;
				if (charIndex)
				{
					(*charIndex)++;
				}
				break;
			case L'\r':
			case L'\n':
				*yPos += size;

				// Only use next xPos for first newline character (in case of multiple consecutive newlines).
				if (nextLine)
				{
					if (*xPositionsIt != xPositionsEnd)
					{
						*xPos = **xPositionsIt;
						(*xPositionsIt)++;
					}
					else
					{
						*xPos = areaX;
					}
					nextLine = false;
					*lineLength = 0;
					if (charIndex)
					{
						(*charIndex)++;
					}
				}
				break;
			case L'\t':
				*xPos += _advance * 4;
				(*lineLength)++;
				if (charIndex)
				{
					(*charIndex)++;
				}
				break;
			case 0:
				// EOF reached.
				return 0;
			}

			*token += iteration;
			delimiter = *token[0];
		}

		// Success.
		return 1;
	}

	void UFont::addLineInfo(const Rectangle& area, int lineWidth, int lineLength, Font::Justify hAlign,
		std::vector<int>* xPositions, std::vector<unsigned int>* lineLengths, bool rightToLeft)
	{
		int hWhitespace = area.width - lineWidth;
		if (hAlign == Font::ALIGN_HCENTER)
		{
			GP_ASSERT(xPositions);
			(*xPositions).push_back(area.x + hWhitespace / 2);
		}
		else if (hAlign == Font::ALIGN_RIGHT)
		{
			GP_ASSERT(xPositions);
			(*xPositions).push_back(area.x + hWhitespace);
		}

		if (rightToLeft)
		{
			GP_ASSERT(lineLengths);
			(*lineLengths).push_back(lineLength);
		}
	}
}
