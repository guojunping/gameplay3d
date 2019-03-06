#include "Base.h"
#include "AFont.h"
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

	AFont::AFont() :
		Font(), _glyphs(NULL), _glyphCount(0)
	{
	}

	AFont::~AFont()
	{
		SAFE_DELETE(_batch);
		SAFE_DELETE_ARRAY(_glyphs);
		SAFE_RELEASE(_texture);
	}


	Font* AFont::create(const char* family, Style style, unsigned int size, Glyph* glyphs, int glyphCount, Texture* texture, Font::Format format)
	{
		GP_ASSERT(family);
		GP_ASSERT(glyphs);
		GP_ASSERT(texture);

		// Create the effect for the font's sprite batch.
		if (__fontEffect == NULL)
		{
			const char* defines = NULL;
			if (format == DISTANCE_FIELD)
				defines = "DISTANCE_FIELD";
			__fontEffect = Effect::createFromFile(FONT_VSH, FONT_FSH, defines);
			if (__fontEffect == NULL)
			{
				GP_WARN("Failed to create effect for font.");
				SAFE_RELEASE(texture);
				return NULL;
			}
		}
		else
		{
			__fontEffect->addRef();
		}

		// Create batch for the font.
		SpriteBatch* batch = SpriteBatch::create(texture, __fontEffect, 128);

		// Release __fontEffect since the SpriteBatch keeps a reference to it
		SAFE_RELEASE(__fontEffect);

		if (batch == NULL)
		{
			GP_WARN("Failed to create batch for font.");
			return NULL;
		}

		// Add linear filtering for better font quality.
		Texture::Sampler* sampler = batch->getSampler();
		sampler->setFilterMode(Texture::LINEAR_MIPMAP_LINEAR, Texture::LINEAR);
		sampler->setWrapMode(Texture::CLAMP, Texture::CLAMP);

		// Increase the ref count of the texture to retain it.
		texture->addRef();

		AFont* font = new AFont();
		font->_format = format;
		font->_family = family;
		font->_style = style;
		font->_size = size;
		font->_texture = texture;
		font->_batch = batch;

		// Copy the glyphs array.
		font->_glyphs = new Glyph[glyphCount];
		memcpy(font->_glyphs, glyphs, sizeof(Glyph) * glyphCount);
		font->_glyphCount = glyphCount;

		return font;
	}

	bool AFont::isCharacterSupported(int character) const
	{
		// TODO: Update this once we support unicode fonts
		int glyphIndex = character - 32; // HACK for ASCII
		return (glyphIndex >= 0 && glyphIndex < (int)_glyphCount);
	}

	void AFont::drawText(const char* text, int x, int y, const Vector4& color, unsigned int size, bool rightToLeft)
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
		const char* cursor = NULL;

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
				char delimiter = cursor[0];
				while (!done &&
					(delimiter == ' ' ||
						delimiter == '\t' ||
						delimiter == '\r' ||
						delimiter == '\n' ||
						delimiter == 0))
				{
					switch (delimiter)
					{
					case ' ':
						xPos += _glyphs[0].advance;
						break;
					case '\r':
					case '\n':
						yPos += size;
						xPos = x;
						break;
					case '\t':
						xPos += _glyphs[0].advance * 4;
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

				length = strcspn(cursor, "\r\n");
				startIndex = length - 1;
				iteration = -1;
			}
			else
			{
				length = strlen(text);
				startIndex = 0;
				iteration = 1;
			}

			GP_ASSERT(_glyphs);
			GP_ASSERT(_batch);
			for (size_t i = startIndex; i < length; i += (size_t)iteration)
			{
				char c = 0;
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
				case ' ':
					xPos += _glyphs[0].advance;
					break;
				case '\r':
				case '\n':
					yPos += size;
					xPos = x;
					break;
				case '\t':
					xPos += _glyphs[0].advance * 4;
					break;
				default:
					int index = c - 32; // HACK for ASCII
					if (index >= 0 && index < (int)_glyphCount)
					{
						Glyph& g = _glyphs[index];

						if (getFormat() == DISTANCE_FIELD)
						{
							if (_cutoffParam == NULL)
								_cutoffParam = _batch->getMaterial()->getParameter("u_cutoff");
							// TODO: Fix me so that smaller font are much smoother
							_cutoffParam->setVector2(Vector2(1.0, 1.0));
						}
						_batch->draw(xPos + (int)(g.bearingX * scale), yPos, g.width * scale, size, g.uvs[0], g.uvs[1], g.uvs[2], g.uvs[3], color);
						xPos += floor(g.advance * scale + spacing);
						break;
					}
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

	void AFont::drawText(const wchar_t* text, int x, int y, const Vector4& color, unsigned int size, bool rightToLeft)
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
				char delimiter = (char)cursor[0];
				while (!done &&
					(delimiter == ' ' ||
						delimiter == '\t' ||
						delimiter == '\r' ||
						delimiter == '\n' ||
						delimiter == 0))
				{
					switch (delimiter)
					{
					case ' ':
						xPos += _glyphs[0].advance;
						break;
					case '\r':
					case '\n':
						yPos += size;
						xPos = x;
						break;
					case '\t':
						xPos += _glyphs[0].advance * 4;
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
				length = wcslen(text);
				startIndex = 0;
				iteration = 1;
			}

			GP_ASSERT(_glyphs);
			GP_ASSERT(_batch);
			for (size_t i = startIndex; i < length; i += (size_t)iteration)
			{
				char c = 0;
				if (rightToLeft)
				{
					c = (char)cursor[i];
				}
				else
				{
					c = (char)text[i];
				}

				// Draw this character.
				switch (c)
				{
				case ' ':
					xPos += _glyphs[0].advance;
					break;
				case '\r':
				case '\n':
					yPos += size;
					xPos = x;
					break;
				case '\t':
					xPos += _glyphs[0].advance * 4;
					break;
				default:
					int index = c - 32; // HACK for ASCII
					if (index >= 0 && index < (int)_glyphCount)
					{
						Glyph& g = _glyphs[index];

						if (getFormat() == DISTANCE_FIELD)
						{
							if (_cutoffParam == NULL)
								_cutoffParam = _batch->getMaterial()->getParameter("u_cutoff");
							// TODO: Fix me so that smaller font are much smoother
							_cutoffParam->setVector2(Vector2(1.0, 1.0));
						}
						_batch->draw(xPos + (int)(g.bearingX * scale), yPos, g.width * scale, size, g.uvs[0], g.uvs[1], g.uvs[2], g.uvs[3], color);
						xPos += floor(g.advance * scale + spacing);
						break;
					}
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

	void AFont::drawText(const char* text, int x, int y, float red, float green, float blue, float alpha, unsigned int size, bool rightToLeft)
	{
		drawText(text, x, y, Vector4(red, green, blue, alpha), size, rightToLeft);
	}

	void AFont::drawText(const wchar_t* text, int x, int y, float red, float green, float blue, float alpha, unsigned int size, bool rightToLeft)
	{
		drawText(text, x, y, Vector4(red, green, blue, alpha), size, rightToLeft);
	}

	void AFont::drawText(const char* text, const Rectangle& area, const Vector4& color, unsigned int size, Justify justify, bool wrap, bool rightToLeft, const Rectangle& clip)
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

		getMeasurementInfo(text, area, size, justify, wrap, rightToLeft, &xPositions, &yPos, &lineLengths);

		// Now we have the info we need in order to render.
		int xPos = area.x;
		std::vector<int>::const_iterator xPositionsIt = xPositions.begin();
		if (xPositionsIt != xPositions.end())
		{
			xPos = *xPositionsIt++;
		}

		const char* token = text;
		int iteration = 1;
		unsigned int lineLength;
		unsigned int currentLineLength = 0;
		const char* lineStart;
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
				tokenLength = (unsigned int)strcspn(token, " \r\n\t");
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

			GP_ASSERT(_glyphs);
			GP_ASSERT(_batch);
			for (int i = startIndex; i < (int)tokenLength && i >= 0; i += iteration)
			{
				char c = token[i];
				int glyphIndex = c - 32; // HACK for ASCII

				if (glyphIndex >= 0 && glyphIndex < (int)_glyphCount)
				{
					Glyph& g = _glyphs[glyphIndex];

					if (xPos + (int)(g.advance*scale) > area.x + area.width)
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
							if (getFormat() == DISTANCE_FIELD)
							{
								if (_cutoffParam == NULL)
									_cutoffParam = _batch->getMaterial()->getParameter("u_cutoff");
								// TODO: Fix me so that smaller font are much smoother
								_cutoffParam->setVector2(Vector2(1.0, 1.0));
							}
							if (clip != Rectangle(0, 0, 0, 0))
							{
								_batch->draw(xPos + (int)(g.bearingX * scale), yPos, g.width * scale, size, g.uvs[0], g.uvs[1], g.uvs[2], g.uvs[3], color, clip);
							}
							else
							{
								_batch->draw(xPos + (int)(g.bearingX * scale), yPos, g.width * scale, size, g.uvs[0], g.uvs[1], g.uvs[2], g.uvs[3], color);
							}
						}
					}
					xPos += (int)(g.advance)*scale + spacing;
				}
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
					size_t tokenLength = strcspn(token, "\r\n");

					if (tokenLength > 0)
					{
						// Get first token of next line.
						token += tokenLength;
					}
				}
			}
		}
	}

	void AFont::drawText(const wchar_t* text, const Rectangle& area, const Vector4& color, unsigned int size, Justify justify, bool wrap, bool rightToLeft, const Rectangle& clip)
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
				tokenLength = (unsigned int)wcscspn(token, L" \r\n\t");
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

			GP_ASSERT(_glyphs);
			GP_ASSERT(_batch);
			for (int i = startIndex; i < (int)tokenLength && i >= 0; i += iteration)
			{
				char c = token[i];
				int glyphIndex = c - 32; // HACK for ASCII

				if (glyphIndex >= 0 && glyphIndex < (int)_glyphCount)
				{
					Glyph& g = _glyphs[glyphIndex];

					if (xPos + (int)(g.advance*scale) > area.x + area.width)
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
							if (getFormat() == DISTANCE_FIELD)
							{
								if (_cutoffParam == NULL)
									_cutoffParam = _batch->getMaterial()->getParameter("u_cutoff");
								// TODO: Fix me so that smaller font are much smoother
								_cutoffParam->setVector2(Vector2(1.0, 1.0));
							}
							if (clip != Rectangle(0, 0, 0, 0))
							{
								_batch->draw(xPos + (int)(g.bearingX * scale), yPos, g.width * scale, size, g.uvs[0], g.uvs[1], g.uvs[2], g.uvs[3], color, clip);
							}
							else
							{
								_batch->draw(xPos + (int)(g.bearingX * scale), yPos, g.width * scale, size, g.uvs[0], g.uvs[1], g.uvs[2], g.uvs[3], color);
							}
						}
					}
					xPos += (int)(g.advance)*scale + spacing;
				}
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

	void AFont::measureText(const char* text, unsigned int size, unsigned int* width, unsigned int* height)
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
		const char* token = text;

		*width = 0;
		*height = size;

		// Measure a line at a time.
		while (token[0] != 0)
		{
			while (token[0] == '\n' || token[0] == '\r')
			{
				*height += size;
				++token;
			}

			unsigned int tokenLength = (unsigned int)strcspn(token, "\n\r");
			unsigned int tokenWidth = getTokenWidth(token, tokenLength, size, scale);
			if (tokenWidth > *width)
			{
				*width = tokenWidth;
			}

			token += tokenLength;
		}
	}

	void AFont::measureText(const wchar_t* text, unsigned int size, unsigned int* width, unsigned int* height)
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

	void AFont::measureText(const char* text, const Rectangle& clip, unsigned int size, Rectangle* out, Justify justify, bool wrap, bool ignoreClip)
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

		if (strlen(text) == 0)
		{
			out->set(0, 0, 0, 0);
			return;
		}

		float scale = (float)size / _size;
		Justify vAlign = static_cast<Justify>(justify & 0xF0);
		if (vAlign == 0)
		{
			vAlign = ALIGN_TOP;
		}

		Justify hAlign = static_cast<Justify>(justify & 0x0F);
		if (hAlign == 0)
		{
			hAlign = ALIGN_LEFT;
		}

		const char* token = text;
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
				char delimiter = token[0];
				while (delimiter == ' ' ||
					delimiter == '\t' ||
					delimiter == '\r' ||
					delimiter == '\n' ||
					delimiter == 0)
				{
					switch (delimiter)
					{
					case ' ':
						delimWidth += _glyphs[0].advance;
						break;
					case '\r':
					case '\n':
						// Add line-height to vertical cursor.
						yPos += size;

						if (lineWidth > 0)
						{
							// Determine horizontal position and width.
							int hWhitespace = clip.width - lineWidth;
							int xPos = clip.x;
							if (hAlign == ALIGN_HCENTER)
							{
								xPos += hWhitespace / 2;
							}
							else if (hAlign == ALIGN_RIGHT)
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
					case '\t':
						delimWidth += _glyphs[0].advance * 4;
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
				unsigned int tokenLength = (unsigned int)strcspn(token, " \r\n\t");
				unsigned int tokenWidth = getTokenWidth(token, tokenLength, size, scale);

				// Wrap if necessary.
				if (lineWidth + tokenWidth + delimWidth > clip.width)
				{
					// Add line-height to vertical cursor.
					yPos += size;

					// Determine horizontal position and width.
					int hWhitespace = clip.width - lineWidth;
					int xPos = clip.x;
					if (hAlign == ALIGN_HCENTER)
					{
						xPos += hWhitespace / 2;
					}
					else if (hAlign == ALIGN_RIGHT)
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
				while (token[0] == '\n' || token[0] == '\r')
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
				unsigned int tokenLength = (unsigned int)strcspn(token, "\n");
				lineWidth = getTokenWidth(token, tokenLength, size, scale);

				// Determine horizontal position and width.
				int xPos = clip.x;
				int hWhitespace = clip.width - lineWidth;
				if (hAlign == ALIGN_HCENTER)
				{
					xPos += hWhitespace / 2;
				}
				else if (hAlign == ALIGN_RIGHT)
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
			if (hAlign == ALIGN_HCENTER)
			{
				xPos += hWhitespace / 2;
			}
			else if (hAlign == ALIGN_RIGHT)
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
		if (vAlign == ALIGN_VCENTER)
		{
			y += vWhitespace / 2;
		}
		else if (vAlign == ALIGN_BOTTOM)
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

				if (vAlign == ALIGN_VCENTER)
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

	void AFont::measureText(const wchar_t* text, const Rectangle& clip, unsigned int size, Rectangle* out, Justify justify, bool wrap, bool ignoreClip)
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

		if (wcslen(text) == 0)
		{
			out->set(0, 0, 0, 0);
			return;
		}

		float scale = (float)size / _size;
		Justify vAlign = static_cast<Justify>(justify & 0xF0);
		if (vAlign == 0)
		{
			vAlign = ALIGN_TOP;
		}

		Justify hAlign = static_cast<Justify>(justify & 0x0F);
		if (hAlign == 0)
		{
			hAlign = ALIGN_LEFT;
		}

		const wchar_t* token = text;
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
				char delimiter = (char)token[0];
				while (delimiter == ' ' ||
					delimiter == '\t' ||
					delimiter == '\r' ||
					delimiter == '\n' ||
					delimiter == 0)
				{
					switch (delimiter)
					{
					case ' ':
						delimWidth += _glyphs[0].advance;
						break;
					case '\r':
					case '\n':
						// Add line-height to vertical cursor.
						yPos += size;

						if (lineWidth > 0)
						{
							// Determine horizontal position and width.
							int hWhitespace = clip.width - lineWidth;
							int xPos = clip.x;
							if (hAlign == ALIGN_HCENTER)
							{
								xPos += hWhitespace / 2;
							}
							else if (hAlign == ALIGN_RIGHT)
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
					case '\t':
						delimWidth += _glyphs[0].advance * 4;
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
					if (hAlign == ALIGN_HCENTER)
					{
						xPos += hWhitespace / 2;
					}
					else if (hAlign == ALIGN_RIGHT)
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
				unsigned int tokenLength = (unsigned int)wcscspn(token, L"\n\r");
				lineWidth = getTokenWidth(token, tokenLength, size, scale);

				// Determine horizontal position and width.
				int xPos = clip.x;
				int hWhitespace = clip.width - lineWidth;
				if (hAlign == ALIGN_HCENTER)
				{
					xPos += hWhitespace / 2;
				}
				else if (hAlign == ALIGN_RIGHT)
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
			if (hAlign == ALIGN_HCENTER)
			{
				xPos += hWhitespace / 2;
			}
			else if (hAlign == ALIGN_RIGHT)
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
		if (vAlign == ALIGN_VCENTER)
		{
			y += vWhitespace / 2;
		}
		else if (vAlign == ALIGN_BOTTOM)
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

				if (vAlign == ALIGN_VCENTER)
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

	void AFont::getMeasurementInfo(const char* text, const Rectangle& area, unsigned int size, Justify justify, bool wrap, bool rightToLeft,
		std::vector<int>* xPositions, int* yPosition, std::vector<unsigned int>* lineLengths)
	{
		GP_ASSERT(_size);
		GP_ASSERT(text);
		GP_ASSERT(yPosition);

		if (size == 0)
			size = _size;

		float scale = (float)size / _size;

		Justify vAlign = static_cast<Justify>(justify & 0xF0);
		if (vAlign == 0)
		{
			vAlign = ALIGN_TOP;
		}

		Justify hAlign = static_cast<Justify>(justify & 0x0F);
		if (hAlign == 0)
		{
			hAlign = ALIGN_LEFT;
		}

		const char* token = text;
		const float areaHeight = area.height - size;

		// For alignments other than top-left, need to calculate the y position to begin drawing from
		// and the starting x position of each line.  For right-to-left text, need to determine
		// the number of characters on each line.
		if (vAlign != ALIGN_TOP || hAlign != ALIGN_LEFT || rightToLeft)
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
					char delimiter = token[0];
					while (delimiter == ' ' ||
						delimiter == '\t' ||
						delimiter == '\r' ||
						delimiter == '\n' ||
						delimiter == 0)
					{
						switch (delimiter)
						{
						case ' ':
							delimWidth += _glyphs[0].advance;
							lineLength++;
							break;
						case '\r':
						case '\n':
							*yPosition += size;

							if (lineWidth > 0)
							{
								addLineInfo(area, lineWidth, lineLength, hAlign, xPositions, lineLengths, rightToLeft);
							}

							lineWidth = 0;
							lineLength = 0;
							delimWidth = 0;
							break;
						case '\t':
							delimWidth += _glyphs[0].advance * 4;
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

					unsigned int tokenLength = (unsigned int)strcspn(token, " \r\n\t");
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
				if (vAlign == ALIGN_VCENTER)
				{
					*yPosition = area.y + vWhiteSpace / 2;
				}
				else if (vAlign == ALIGN_BOTTOM)
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
					char delimiter = token[0];
					while (delimiter == '\n' || delimiter == '\r')
					{
						*yPosition += size;
						++token;
						delimiter = token[0];
					}

					unsigned int tokenLength = (unsigned int)strcspn(token, "\n\r");
					if (tokenLength == 0)
					{
						tokenLength = (unsigned int)strlen(token);
					}

					int lineWidth = getTokenWidth(token, tokenLength, size, scale);
					addLineInfo(area, lineWidth, tokenLength, hAlign, xPositions, lineLengths, rightToLeft);

					token += tokenLength;
				}

				int textHeight = *yPosition - area.y;
				int vWhiteSpace = areaHeight - textHeight;
				if (vAlign == ALIGN_VCENTER)
				{
					*yPosition = area.y + vWhiteSpace / 2;
				}
				else if (vAlign == ALIGN_BOTTOM)
				{
					*yPosition = area.y + vWhiteSpace;
				}
			}

			if (vAlign == ALIGN_TOP)
			{
				*yPosition = area.y;
			}
		}
	}

	void AFont::getMeasurementInfo(const wchar_t* text, const Rectangle& area, unsigned int size, Justify justify, bool wrap, bool rightToLeft,
		std::vector<int>* xPositions, int* yPosition, std::vector<unsigned int>* lineLengths)
	{
		GP_ASSERT(_size);
		GP_ASSERT(text);
		GP_ASSERT(yPosition);

		if (size == 0)
			size = _size;

		float scale = (float)size / _size;

		Justify vAlign = static_cast<Justify>(justify & 0xF0);
		if (vAlign == 0)
		{
			vAlign = ALIGN_TOP;
		}

		Justify hAlign = static_cast<Justify>(justify & 0x0F);
		if (hAlign == 0)
		{
			hAlign = ALIGN_LEFT;
		}

		const wchar_t* token = text;
		const float areaHeight = area.height - size;

		// For alignments other than top-left, need to calculate the y position to begin drawing from
		// and the starting x position of each line.  For right-to-left text, need to determine
		// the number of characters on each line.
		if (vAlign != ALIGN_TOP || hAlign != ALIGN_LEFT || rightToLeft)
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
					char delimiter = (char)token[0];
					while (delimiter == ' ' ||
						delimiter == '\t' ||
						delimiter == '\r' ||
						delimiter == '\n' ||
						delimiter == 0)
					{
						switch (delimiter)
						{
						case ' ':
							delimWidth += _glyphs[0].advance;
							lineLength++;
							break;
						case '\r':
						case '\n':
							*yPosition += size;

							if (lineWidth > 0)
							{
								addLineInfo(area, lineWidth, lineLength, hAlign, xPositions, lineLengths, rightToLeft);
							}

							lineWidth = 0;
							lineLength = 0;
							delimWidth = 0;
							break;
						case '\t':
							delimWidth += _glyphs[0].advance * 4;
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
				if (vAlign == ALIGN_VCENTER)
				{
					*yPosition = area.y + vWhiteSpace / 2;
				}
				else if (vAlign == ALIGN_BOTTOM)
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
					char delimiter = (char)token[0];
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
				if (vAlign == ALIGN_VCENTER)
				{
					*yPosition = area.y + vWhiteSpace / 2;
				}
				else if (vAlign == ALIGN_BOTTOM)
				{
					*yPosition = area.y + vWhiteSpace;
				}
			}

			if (vAlign == ALIGN_TOP)
			{
				*yPosition = area.y;
			}
		}
	}

	int AFont::getIndexAtLocation(const char* text, const Rectangle& area, unsigned int size, const Vector2& inLocation, Vector2* outLocation,
		Justify justify, bool wrap, bool rightToLeft)
	{
		return getIndexOrLocation(text, area, size, inLocation, outLocation, -1, justify, wrap, rightToLeft);
	}

	int AFont::getIndexAtLocation(const wchar_t* text, const Rectangle& area, unsigned int size, const Vector2& inLocation, Vector2* outLocation,
		Justify justify, bool wrap, bool rightToLeft)
	{
		return getIndexOrLocation(text, area, size, inLocation, outLocation, -1, justify, wrap, rightToLeft);
	}

	void AFont::getLocationAtIndex(const char* text, const Rectangle& clip, unsigned int size, Vector2* outLocation, const unsigned int destIndex,
		Justify justify, bool wrap, bool rightToLeft)
	{
		getIndexOrLocation(text, clip, size, *outLocation, outLocation, (const int)destIndex, justify, wrap, rightToLeft);
	}

	void AFont::getLocationAtIndex(const wchar_t* text, const Rectangle& clip, unsigned int size, Vector2* outLocation, const unsigned int destIndex,
		Justify justify, bool wrap, bool rightToLeft)
	{
		getIndexOrLocation(text, clip, size, *outLocation, outLocation, (const int)destIndex, justify, wrap, rightToLeft);
	}

	int AFont::getIndexOrLocation(const char* text, const Rectangle& area, unsigned int size, const Vector2& inLocation, Vector2* outLocation,
		const int destIndex, Justify justify, bool wrap, bool rightToLeft)
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

		getMeasurementInfo(text, area, size, justify, wrap, rightToLeft, &xPositions, &yPos, &lineLengths);

		int xPos = area.x;
		std::vector<int>::const_iterator xPositionsIt = xPositions.begin();
		if (xPositionsIt != xPositions.end())
		{
			xPos = *xPositionsIt++;
		}

		const char* token = text;

		int iteration = 1;
		unsigned int lineLength;
		unsigned int currentLineLength = 0;
		const char* lineStart;
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
				tokenLength = (unsigned int)strcspn(token, " \r\n\t");
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

			GP_ASSERT(_glyphs);
			for (int i = startIndex; i < (int)tokenLength && i >= 0; i += iteration)
			{
				char c = token[i];
				int glyphIndex = c - 32; // HACK for ASCII

				if (glyphIndex >= 0 && glyphIndex < (int)_glyphCount)
				{
					Glyph& g = _glyphs[glyphIndex];

					if (xPos + (int)(g.advance*scale) > area.x + area.width)
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
							inLocation.x >= xPos && inLocation.x < floor(xPos + g.advance*scale + spacing) &&
							inLocation.y >= yPos && inLocation.y < yPos + size))
					{
						outLocation->x = xPos;
						outLocation->y = yPos;
						return charIndex;
					}

					xPos += floor(g.advance*scale + spacing);
					charIndex++;
				}
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
					unsigned int tokenLength = (unsigned int)strcspn(token, "\n");

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

	int AFont::getIndexOrLocation(const wchar_t* text, const Rectangle& area, unsigned int size, const Vector2& inLocation, Vector2* outLocation,
		const int destIndex, Justify justify, bool wrap, bool rightToLeft)
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

			GP_ASSERT(_glyphs);
			for (int i = startIndex; i < (int)tokenLength && i >= 0; i += iteration)
			{
				char c = (char)token[i];
				int glyphIndex = c - 32; // HACK for ASCII

				if (glyphIndex >= 0 && glyphIndex < (int)_glyphCount)
				{
					Glyph& g = _glyphs[glyphIndex];

					if (xPos + (int)(g.advance*scale) > area.x + area.width)
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
							inLocation.x >= xPos && inLocation.x < floor(xPos + g.advance*scale + spacing) &&
							inLocation.y >= yPos && inLocation.y < yPos + size))
					{
						outLocation->x = xPos;
						outLocation->y = yPos;
						return charIndex;
					}

					xPos += floor(g.advance*scale + spacing);
					charIndex++;
				}
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

	unsigned int AFont::getTokenWidth(const char* token, unsigned int length, unsigned int size, float scale)
	{
		GP_ASSERT(token);
		GP_ASSERT(_glyphs);

		if (size == 0)
			size = _size;

		int spacing = (int)(size * _spacing);

		// Calculate width of word or line.
		unsigned int tokenWidth = 0;
		for (unsigned int i = 0; i < length; ++i)
		{
			char c = token[i];
			switch (c)
			{
			case ' ':
				tokenWidth += _glyphs[0].advance;
				break;
			case '\t':
				tokenWidth += _glyphs[0].advance * 4;
				break;
			default:
				int glyphIndex = c - 32;
				if (glyphIndex >= 0 && glyphIndex < (int)_glyphCount)
				{
					Glyph& g = _glyphs[glyphIndex];
					tokenWidth += floor(g.advance * scale + spacing);
				}
				break;
			}
		}

		return tokenWidth;
	}

	unsigned int AFont::getTokenWidth(const wchar_t* token, unsigned int length, unsigned int size, float scale)
	{
		GP_ASSERT(token);
		GP_ASSERT(_glyphs);

		if (size == 0)
			size = _size;

		int spacing = (int)(size * _spacing);

		// Calculate width of word or line.
		unsigned int tokenWidth = 0;
		for (unsigned int i = 0; i < length; ++i)
		{
			char c = (char)token[i];
			switch (c)
			{
			case ' ':
				tokenWidth += _glyphs[0].advance;
				break;
			case '\t':
				tokenWidth += _glyphs[0].advance * 4;
				break;
			default:
				int glyphIndex = c - 32;
				if (glyphIndex >= 0 && glyphIndex < (int)_glyphCount)
				{
					Glyph& g = _glyphs[glyphIndex];
					tokenWidth += floor(g.advance * scale + spacing);
				}
				break;
			}
		}

		return tokenWidth;
	}

	unsigned int AFont::getReversedTokenLength(const char* token, const char* bufStart)
	{
		GP_ASSERT(token);
		GP_ASSERT(bufStart);

		const char* cursor = token;
		char c = cursor[0];
		unsigned int length = 0;

		while (cursor != bufStart && c != ' ' && c != '\r' && c != '\n' && c != '\t')
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

	unsigned int AFont::getReversedTokenLength(const wchar_t* token, const wchar_t* bufStart)
	{
		GP_ASSERT(token);
		GP_ASSERT(bufStart);

		const wchar_t* cursor = token;
		char c = (char)cursor[0];
		unsigned int length = 0;

		while (cursor != bufStart && c != ' ' && c != '\r' && c != '\n' && c != '\t')
		{
			length++;
			cursor--;
			c = (char)cursor[0];
		}

		if (cursor == bufStart)
		{
			length++;
		}

		return length;
	}

	int AFont::handleDelimiters(const char** token, const unsigned int size, const int iteration, const int areaX, int* xPos, int* yPos, unsigned int* lineLength,
		std::vector<int>::const_iterator* xPositionsIt, std::vector<int>::const_iterator xPositionsEnd, unsigned int* charIndex,
		const Vector2* stopAtPosition, const int currentIndex, const int destIndex)
	{
		GP_ASSERT(token);
		GP_ASSERT(*token);
		GP_ASSERT(xPos);
		GP_ASSERT(yPos);
		GP_ASSERT(lineLength);
		GP_ASSERT(xPositionsIt);

		char delimiter = *token[0];
		bool nextLine = true;
		while (delimiter == ' ' ||
			delimiter == '\t' ||
			delimiter == '\r' ||
			delimiter == '\n' ||
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
			case ' ':
				*xPos += _glyphs[0].advance;
				(*lineLength)++;
				if (charIndex)
				{
					(*charIndex)++;
				}
				break;
			case '\r':
			case '\n':
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
			case '\t':
				*xPos += _glyphs[0].advance * 4;
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

	int AFont::handleDelimiters(const wchar_t** token, const unsigned int size, const int iteration, const int areaX, int* xPos, int* yPos, unsigned int* lineLength,
		std::vector<int>::const_iterator* xPositionsIt, std::vector<int>::const_iterator xPositionsEnd, unsigned int* charIndex,
		const Vector2* stopAtPosition, const int currentIndex, const int destIndex)
	{
		GP_ASSERT(token);
		GP_ASSERT(*token);
		GP_ASSERT(xPos);
		GP_ASSERT(yPos);
		GP_ASSERT(lineLength);
		GP_ASSERT(xPositionsIt);

		char delimiter = (char)*token[0];
		bool nextLine = true;
		while (delimiter == ' ' ||
			delimiter == '\t' ||
			delimiter == '\r' ||
			delimiter == '\n' ||
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
			case ' ':
				*xPos += _glyphs[0].advance;
				(*lineLength)++;
				if (charIndex)
				{
					(*charIndex)++;
				}
				break;
			case '\r':
			case '\n':
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
			case '\t':
				*xPos += _glyphs[0].advance * 4;
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
			delimiter = (char)*token[0];
		}

		// Success.
		return 1;
	}

	void AFont::addLineInfo(const Rectangle& area, int lineWidth, int lineLength, Justify hAlign,
		std::vector<int>* xPositions, std::vector<unsigned int>* lineLengths, bool rightToLeft)
	{
		int hWhitespace = area.width - lineWidth;
		if (hAlign == ALIGN_HCENTER)
		{
			GP_ASSERT(xPositions);
			(*xPositions).push_back(area.x + hWhitespace / 2);
		}
		else if (hAlign == ALIGN_RIGHT)
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
