#ifndef UFONT_H_
#define UFONT_H_

#include <ft2build.h>
#include <freetype/ftglyph.h>
#include FT_FREETYPE_H
#include "Font.h"

#include "SpriteBatch.h"

namespace gameplay
{

	/**
	* Defines a font for text rendering.
	*/
	class UFont : public Font
	{
		friend class Bundle;
		friend class Text;
		friend class TextBox;

	public:

		/**
		* Determines if this font supports the specified character code.
		*
		* @param character The character code to check.
		* @return True if this Font supports (can draw) the specified character, false otherwise.
		*/
		bool isCharacterSupported(int character) const;

		/**
		* Draws the specified text in a solid color, with a scaling factor.
		*
		* @param text The text to draw.
		* @param x The viewport x position to draw text at.
		* @param y The viewport y position to draw text at.
		* @param color The color of text.
		* @param size The size to draw text (0 for default size).
		* @param rightToLeft Whether to draw text from right to left.
		*/
		void drawText(const char* text, int x, int y, const Vector4& color, unsigned int size = 0,
			bool rightToLeft = false);

		/**
		* Draws the specified text in a solid color, with a scaling factor.
		*
		* @param text The text to draw.
		* @param x The viewport x position to draw text at.
		* @param y The viewport y position to draw text at.
		* @param red The red channel of the text color.
		* @param green The green channel of the text color.
		* @param blue The blue channel of the text color.
		* @param alpha The alpha channel of the text color.
		* @param size The size to draw text (0 for default size).
		* @param rightToLeft Whether to draw text from right to left.
		*/
		void drawText(const char* text, int x, int y, float red, float green, float blue, float alpha, unsigned int size = 0,
			bool rightToLeft = false);

		/**
		* Draws the specified text within a rectangular area, with a specified alignment and scale.
		* Clips text outside the viewport. Optionally wraps text to fit within the width of the viewport.
		*
		* @param text The text to draw.
		* @param area The viewport area to draw within.  Text will be clipped outside this rectangle.
		* @param color The color of text.
		* @param size The size to draw text (0 for default size).
		* @param justify Justification of text within the viewport.
		* @param wrap Wraps text to fit within the width of the viewport if true.
		* @param rightToLeft Whether to draw text from right to left.
		* @param clip A region to clip text within after applying justification to the viewport area.
		*/
		void drawText(const char* text, const Rectangle& area, const Vector4& color, unsigned int size = 0,
			Font::Justify justify = Font::ALIGN_TOP_LEFT, bool wrap = true, bool rightToLeft = false,
			const Rectangle& clip = Rectangle(0, 0, 0, 0));

		/**
		* Measures a string's width and height without alignment, wrapping or clipping.
		*
		* @param text The text to measure.
		* @param size The font height to scale to.
		* @param widthOut Destination for the text's width.
		* @param heightOut Destination for the text's height.
		*/
		void measureText(const char* text, unsigned int size, unsigned int* widthOut, unsigned int* heightOut);

		/**
		* Measures a string's bounding box after alignment, wrapping and clipping within a viewport.
		*
		* @param text The text to measure.
		* @param clip The clip rectangle.
		* @param size The font height to scale to.
		* @param out Destination rectangle to store the bounds in.
		* @param justify Justification of text within the viewport.
		* @param wrap Whether to measure text with wrapping applied.
		* @param ignoreClip Whether to clip 'out' to the viewport.  Set false for the bounds of what would actually be drawn
		*                within the given viewport; true for bounds that are guaranteed to fit the entire string of text.
		*/
		void measureText(const char* text, const Rectangle& clip, unsigned int size, Rectangle* out,
			Font::Justify justify = Font::ALIGN_TOP_LEFT, bool wrap = true, bool ignoreClip = false);

		/**
		* Get an character index into a string corresponding to the character nearest the given location within the clip region.
		*/
		int getIndexAtLocation(const char* text, const Rectangle& clip, unsigned int size, const Vector2& inLocation,
			Vector2* outLocation, Font::Justify justify = Font::ALIGN_TOP_LEFT, bool wrap = true, bool rightToLeft = false);

		/**
		* Get the location of the character at the given index.
		*/
		void getLocationAtIndex(const char* text, const Rectangle& clip, unsigned int size, Vector2* outLocation,
			const unsigned int destIndex, Font::Justify justify = Font::ALIGN_TOP_LEFT, bool wrap = true,
			bool rightToLeft = false);

		int getIndexOrLocation(const char* text, const Rectangle& clip, unsigned int size, const Vector2& inLocation, Vector2* outLocation,
			const int destIndex = -1, Font::Justify justify = Font::ALIGN_TOP_LEFT, bool wrap = true, bool rightToLeft = false);

	private:

		/**
		* Constructor.
		*/
		UFont();

		/**
		* Constructor.
		*/
		UFont(const UFont& copy);

		/**
		* Destructor.
		*/
		~UFont();

		/**
		* Hidden copy assignment operator.
		*/
		UFont& operator=(const UFont&);

		/**
		* Creates a font with the given characteristics from the specified glyph array and texture map.
		*
		* This method will create a new Font object regardless of whether another Font is already
		* created with the same attributes.
		*
		* @param family The font family name.
		* @param style The font style.
		* @param size The font size.
		* @param glyphs An array of font glyphs, defining each character in the font within the texture map.
		* @param glyphCount The number of items in the glyph array.
		* @param texture A texture map containing rendered glyphs.
		* @param format The format of the font (bitmap or distance fields)
		*
		* @return The new Font or NULL if there was an error.
		*/
		static Font* create(char* fontData, unsigned int fontDataLen, unsigned int size, Font::Format format, Font* masterFont);
		Glyph* getGlyph(wchar_t ch);
		void getMeasurementInfo(const wchar_t* text, const Rectangle& area, unsigned int size, Font::Justify justify, bool wrap, bool rightToLeft,
			std::vector<int>* xPositions, int* yPosition, std::vector<unsigned int>* lineLengths);


		unsigned int getTokenWidth(const wchar_t* token, unsigned length, unsigned int size, float scale);

		unsigned int getReversedTokenLength(const wchar_t* token, const wchar_t* bufStart);

		int handleDelimiters(const wchar_t** token, const unsigned int size, const int iteration, const int areaX, int* xPos, int* yPos, unsigned int* lineLength,
			std::vector<int>::const_iterator* xPositionsIt, std::vector<int>::const_iterator xPositionsEnd, unsigned int* charIndex = NULL,
			const Vector2* stopAtPosition = NULL, const int currentIndex = -1, const int destIndex = -1);

		void addLineInfo(const Rectangle& area, int lineWidth, int lineLength, Font::Justify hAlign,
			std::vector<int>* xPositions, std::vector<unsigned int>* lineLengths, bool rightToLeft);
		
		std::unordered_map<wchar_t, Glyph*> _glyphs;
		
		int _texHeight;
		int _texWidth;
		int _startX = 0;
		int _startY = 0;
		int _advance;
		char *_fontData;
		int _fontDataLen;
		FT_Library _library;
		FT_Face _face;
		Font* _masterFont;
	};

}

#endif
