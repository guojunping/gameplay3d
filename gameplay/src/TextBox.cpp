#include "TextBox.h"
#include "Game.h"

namespace gameplay
{

TextBox::TextBox() : _caretLocation(0), _lastKeypress(0), _fontSize(0), _caretImage(NULL), _passwordChar('*'), _inputMode(TEXT), _ctrlPressed(false), _shiftPressed(false)
{
    _canFocus = true;
}

TextBox::~TextBox()
{
}

TextBox* TextBox::create(const char* id, Theme::Style* style)
{
    TextBox* textBox = new TextBox();
    textBox->_id = id ? id : "";
    textBox->initialize("TextBox", style, NULL);
    return textBox;
}

Control* TextBox::create(Theme::Style* style, Properties* properties)
{
    TextBox* textBox = new TextBox();
    textBox->initialize("TextBox", style, properties);
    return textBox;
}

void TextBox::initialize(const char* typeName, Theme::Style* style, Properties* properties)
{
    Label::initialize(typeName, style, properties);

	if (properties)
	{
		_inputMode = getInputMode(properties->getString("inputMode"));
	}
}

const char* TextBox::getTypeName() const
{
    return "TextBox";
}

void TextBox::addListener(Control::Listener* listener, int eventFlags)
{
    if ((eventFlags & Control::Listener::VALUE_CHANGED) == Control::Listener::VALUE_CHANGED)
    {
        GP_ERROR("VALUE_CHANGED event is not applicable to this control.");
    }

    Control::addListener(listener, eventFlags);
}

int TextBox::getLastKeypress()
{
    return _lastKeypress;
}

unsigned int TextBox::getCaretLocation() const
{
    return _caretLocation;
}

void TextBox::setCaretLocation(unsigned int index)
{
    _caretLocation = index;
    if (_caretLocation > _text.length())
        _caretLocation = (unsigned int)_text.length();
}

bool TextBox::touchEvent(Touch::TouchEvent evt, int x, int y, unsigned int contactIndex)
{
    if (getState() == ACTIVE) {
        switch (evt)
        {
        case Touch::TOUCH_PRESS:
            setCaretLocation(x, y);
            break;
        case Touch::TOUCH_MOVE:
            setCaretLocation(x, y);
            break;
        default:
            break;
        }
    }

    return Label::touchEvent(evt, x, y, contactIndex);
}

static bool isWhitespace(char c)
{
    switch (c)
    {
    case ' ':
    case '\t':
    case '\r':
    case '\n':
        return true;

    default:
        return false;
    }
}

static unsigned int findNextWord(const std::wstring& text, unsigned int from, bool backwards)
{
    int pos = (int)from;
    if (backwards)
    {
        if (pos > 0)
        {
            // Moving backwards: skip all consecutive whitespace characters
            while (pos > 0 && isWhitespace(text.at(pos-1)))
                --pos;
            // Now search back to the first whitespace character
            while (pos > 0 && !isWhitespace(text.at(pos-1)))
                --pos;
        }
    }
    else
    {
        const int len = (const int)text.length();
        if (pos < len)
        {
            // Moving forward: skip all consecutive non-whitespace characters
            ++pos;
            while (pos < len && !isWhitespace(text.at(pos)))
                ++pos;
            // Now search for the first non-whitespace character
            while (pos < len && isWhitespace(text.at(pos)))
                ++pos;
        }
    }

    return (unsigned int)pos;
}

static int Unicode2UTF8(unsigned wchar, char *utf8)
{
	int len = 0;
	if (wchar < 0xC0)
	{
		utf8[len++] = (char)wchar;
	}
	else if (wchar < 0x800)
	{
		utf8[len++] = 0xc0 | (wchar >> 6);
		utf8[len++] = 0x80 | (wchar & 0x3f);
	}
	else if (wchar < 0x10000)
	{
		utf8[len++] = 0xe0 | (wchar >> 12);
		utf8[len++] = 0x80 | ((wchar >> 6) & 0x3f);
		utf8[len++] = 0x80 | (wchar & 0x3f);
	}
	else if (wchar < 0x200000)
	{
		utf8[len++] = 0xf0 | ((int)wchar >> 18);
		utf8[len++] = 0x80 | ((wchar >> 12) & 0x3f);
		utf8[len++] = 0x80 | ((wchar >> 6) & 0x3f);
		utf8[len++] = 0x80 | (wchar & 0x3f);
	}
	else if (wchar < 0x4000000)
	{
		utf8[len++] = 0xf8 | ((int)wchar >> 24);
		utf8[len++] = 0x80 | ((wchar >> 18) & 0x3f);
		utf8[len++] = 0x80 | ((wchar >> 12) & 0x3f);
		utf8[len++] = 0x80 | ((wchar >> 6) & 0x3f);
		utf8[len++] = 0x80 | (wchar & 0x3f);
	}
	else if (wchar < 0x80000000)
	{
		utf8[len++] = 0xfc | ((int)wchar >> 30);
		utf8[len++] = 0x80 | ((wchar >> 24) & 0x3f);
		utf8[len++] = 0x80 | ((wchar >> 18) & 0x3f);
		utf8[len++] = 0x80 | ((wchar >> 12) & 0x3f);
		utf8[len++] = 0x80 | ((wchar >> 6) & 0x3f);
		utf8[len++] = 0x80 | (wchar & 0x3f);
	}

	return len;
}

bool TextBox::keyEvent(Keyboard::KeyEvent evt, int key)
{
    switch (evt)
    {
        case Keyboard::KEY_PRESS:
        {
            switch (key)
            {
            	case Keyboard::KEY_SHIFT:
            	{
                    _shiftPressed = true;
                    break;
            	}
                case Keyboard::KEY_CTRL:
                {
                    _ctrlPressed = true;
                    break;
                }
                case Keyboard::KEY_HOME:
                {
                    _caretLocation = 0;
                    break;
                }
                case Keyboard::KEY_END:
                {
                    _caretLocation = _text.length();
                    break;
                }
                case Keyboard::KEY_DELETE:
                {
                    if (_caretLocation < _text.length())
                    {
                        int newCaretLocation;
                        //if (_ctrlPressed)
                        //{
                        //    newCaretLocation = findNextWord(getDisplayedText(), _caretLocation, false);
                        //}
                        //else
                       // {
                            newCaretLocation = _caretLocation + 1;
                        //}
                        _text.erase(_caretLocation, newCaretLocation - _caretLocation);
                        notifyListeners(Control::Listener::TEXT_CHANGED);
                    }
                    break;
                }
                case Keyboard::KEY_TAB:
                {
                    // Allow tab to move the focus forward.
                    return false;
                }
                case Keyboard::KEY_LEFT_ARROW:
                {
                    if (_caretLocation > 0)
                    {
                        //if (_ctrlPressed)
                        //{
                        //    _caretLocation = findNextWord(getDisplayedText(), _caretLocation, true);
                        //}
                        //else
                        //{
                            --_caretLocation;
                        //}
                    }
                    break;
                }
                case Keyboard::KEY_RIGHT_ARROW:
                {
                    if (_caretLocation < _text.length())
                    {
                        //if (_ctrlPressed)
                        //{
                        //    _caretLocation = findNextWord(getDisplayedText(), _caretLocation, false);
                        //}
                        //else
                        //{
                            ++_caretLocation;
                        //}
                    }
                    break;
                }
                case Keyboard::KEY_UP_ARROW:
                {
                    // TODO: Support multiline
                    break;
                }
                case Keyboard::KEY_DOWN_ARROW:
                {
                    // TODO: Support multiline
                    break;
                }
                case Keyboard::KEY_BACKSPACE:
                {
                    if (_caretLocation > 0)
                    {
                        int newCaretLocation;
                        //if (_ctrlPressed)
                        //{
                        //    newCaretLocation = findNextWord(getDisplayedText(), _caretLocation, true);
                        //}
                        //else
                        //{
                            newCaretLocation = _caretLocation - 1;
                        //}
						if (this->_multiLines) {
							Control::State state = getState();
							Font* font = getFont(state);
							bool rear = _caretLocation == _text.length() ? true : false;
							if (_text.at(_caretLocation - 1) == L'\n') {
								newCaretLocation--;
							}
							_text.erase(newCaretLocation, _caretLocation - newCaretLocation);
							if (!rear) {
								unsigned int fontSize = getFontSize(state);
								unsigned int outWidth, outHeight;

								int i = newCaretLocation;
								int j = newCaretLocation;
								for (; j > 0; j--) {
									if (_text.at(j) == L'\n' || _text.at(j) == L'\r') {
										break;
									}
								}

								while (i < _text.length()) {
									if (_text.at(i) == L'\n') {
										_text.erase(i, 1);
										continue;
									}
									i++;
								}
								i = newCaretLocation + 1;
								while(true){
									if (i >= _text.length()) {
										font->measureText(_text.substr(j, i - j).c_str(), fontSize, &outWidth, &outHeight);
										if (_textBounds.width < outWidth) {
											font->measureText(_text.substr(j, i - j - 1).c_str(), fontSize, &outWidth, &outHeight);
											if (_textBounds.width > outWidth) {
												_text.insert(i - 1, 1, L'\n');
											}
											else {
												_text.insert(i - 2, 1, L'\n');
											}
										}
										break;
									}
									if (_text.at(i) == L'\r') {
										font->measureText(_text.substr(j, i - j).c_str(), fontSize, &outWidth, &outHeight);
										if (_textBounds.width < outWidth) {
											font->measureText(_text.substr(j, i - j - 1).c_str(), fontSize, &outWidth, &outHeight);
											if (_textBounds.width > outWidth) {
												_text.insert(i - 1, 1, L'\n');
												j = i - 1;
												i = j + 1;
											}
											else {
												_text.insert(i - 2, 1, L'\n');
												j = i - 2;
												i = j + 1;
											}
										}
										else {
											j = i + 1;
											i = j + 1;
										}
									}

									font->measureText(_text.substr(j, i - j).c_str(), fontSize, &outWidth, &outHeight);
									if (_textBounds.width < outWidth) {
										_text.insert(i - 1, 1, L'\n');
										j = i;
									}
									i++;
								}
							}
							_caretLocation = newCaretLocation;
						}
						else {
							_text.erase(newCaretLocation, _caretLocation - newCaretLocation);
							_caretLocation = newCaretLocation;
						}
                   
                        notifyListeners(Control::Listener::TEXT_CHANGED);
                    }
                    break;
                }
            }
            break;
        }

        case Keyboard::KEY_CHAR:
        {
            switch (key)
            {
                case Keyboard::KEY_RETURN:
				{
					// TODO: Support multi-line
					//notifyListeners(Control::Listener::ACTIVATED);
					if (this->_multiLines) {
						_text.insert(_caretLocation, 1, L'\r');
						_caretLocation++;
					}
					bool rear = _caretLocation == _text.length() ? true : false;
					if (!rear && this->_multiLines) {
						if (this->_multiLines) {
							Control::State state = getState();
							Font* font = getFont(state);

							unsigned int fontSize = getFontSize(state);
							unsigned int outWidth, outHeight;

							int i = _caretLocation;
							int j = _caretLocation;

							while (i < _text.length()) {
								if (_text.at(i) == L'\n') {
									_text.erase(i, 1);
									continue;
								}
								i++;
							}
							i = j + 1;
							while (true) {
								if (i >= _text.length()) {
									font->measureText(_text.substr(j, i - j).c_str(), fontSize, &outWidth, &outHeight);
									if (_textBounds.width < outWidth) {
										font->measureText(_text.substr(j, i - j - 1).c_str(), fontSize, &outWidth, &outHeight);
										if (_textBounds.width > outWidth) {
											_text.insert(i - 1, 1, L'\n');
										}
										else {
											_text.insert(i - 2, 1, L'\n');
										}
									}
									break;
								}
								if (_text.at(i) == L'\r') {
									j = i + 1;
									i = j + 1;
								}

								font->measureText(_text.substr(j, i - j).c_str(), fontSize, &outWidth, &outHeight);
								if (_textBounds.width < outWidth) {
									_text.insert(i - 1, 1, L'\n');
									j = i;
								}
								i++;
							}
						}
					}
					break;
				}
                case Keyboard::KEY_ESCAPE:
                    break;
                case Keyboard::KEY_BACKSPACE:
                    break;
                case Keyboard::KEY_TAB:
                    // Allow tab to move the focus forward.
                    return false;
                default:
                {
                    // Insert character into string, only if our font supports this character
                    if (_font && _font->isCharacterSupported(key))
                    {
						if (_shiftPressed && islower(key))
						{
							key = toupper(key);
						}
                        if (_caretLocation <= _text.length())
                        {
							Control::State state = getState();
							Font* font = getFont(state);
							bool rear = _caretLocation == _text.length() ? true : false;
							_text.insert(_caretLocation, 1, key);

							this->_multiLines = true;
							unsigned int fontSize = getFontSize(state);
							unsigned int outWidth, outHeight;
							if (rear) {
								font->measureText(_text.c_str(), fontSize, &outWidth, &outHeight);
								if (_textBounds.width < outWidth && _textBounds.height >= outHeight) {
									if (this->_multiLines) {
										_text.insert(_text.length() - 1, 1, L'\n');
										font->measureText(_text.c_str(), fontSize, &outWidth, &outHeight);
										if (_textBounds.width < outWidth || _textBounds.height < outHeight) {
											_text = _text.erase(_text.length() - 2);
											font->measureText(_text.c_str(), fontSize, &outWidth, &outHeight);
											if (_textBounds.width < outWidth || _textBounds.height < outHeight) {
												_text.erase(_text.length() - 1);
											}
											break;
										}
										if (rear)
											++_caretLocation;
									}
									else {
										_text.erase(_text.length() - 1);
										break;
									}
								}
							}
							else 
							{
								if (this->_multiLines) {
									int i = _caretLocation;
									int j = _caretLocation;
									for (; j > 0; j--) {
										if (_text.at(j) == L'\n' || _text.at(j) == L'\r') {
											break;
										}
									}
									while (true) {
										for (; i < _text.length(); i++) {
											if (_text.at(i) == L'\n' || _text.at(i) == L'\r') {
												break;
											}
										}
										if (i >= _text.length()) {
											font->measureText(_text.substr(j, i - j).c_str(), fontSize, &outWidth, &outHeight);
											if (_textBounds.width < outWidth) {
												font->measureText(_text.substr(j, i - j - 1).c_str(), fontSize, &outWidth, &outHeight);
												if (_textBounds.width > outWidth) {
													_text.insert(i - 1, 1, L'\n');
												}
												else {
													_text.insert(i - 2, 1, L'\n');
												}
											}
											break;
										}

										font->measureText(_text.substr(j, i - j).c_str(), fontSize, &outWidth, &outHeight);
										while (_textBounds.width < outWidth) {
											if (_text.at(i) == '\n') {
												char c = _text.at(i - 1);
												_text[i - 1] = '\n';
												_text[i] = c;
												i--;
												font->measureText(_text.substr(j, i - j).c_str(), fontSize, &outWidth, &outHeight);
											}
											else if (_text.at(i) == '\r') {
												_text.insert(i - 2, 1, '\n');
												i = i - 2;
												font->measureText(_text.substr(j, i - j).c_str(), fontSize, &outWidth, &outHeight);
											}
										}
										i++;
										j = i;
									}
								}
							}
							++_caretLocation;
                        }

                        notifyListeners(Control::Listener::TEXT_CHANGED);
                    }
                    break;
                }
            
                break;
            }
            break;
        }
        case Keyboard::KEY_RELEASE:
            switch (key)
            {
            	case Keyboard::KEY_SHIFT:
            	{
                    _shiftPressed = false;
                    break;
             	 }
                case Keyboard::KEY_CTRL:
                {
                    _ctrlPressed = false;
                    break;
                }
            }
    }

    _lastKeypress = key;

    return Label::keyEvent(evt, key);
}

void TextBox::controlEvent(Control::Listener::EventType evt)
{
    Label::controlEvent(evt);

    switch (evt)
    {
    case Control::Listener::FOCUS_GAINED:
        Game::getInstance()->displayKeyboard(true);
        break;

    case Control::Listener::FOCUS_LOST:
        Game::getInstance()->displayKeyboard(false);
        break;
    default:
        break;
    }
}

void TextBox::updateState(State state)
{
    Label::updateState(state);

    _fontSize = getFontSize(state);
    _caretImage = getImage("textCaret", state);
}

unsigned int TextBox::drawImages(Form* form, const Rectangle& clip)
{
    Control::State state = getState();

    if (_caretImage && (state == ACTIVE || hasFocus()))
    {
        // Draw the cursor at its current location.
        const Rectangle& region = _caretImage->getRegion();
        if (!region.isEmpty())
        {
            const Theme::UVs& uvs = _caretImage->getUVs();
            Vector4 color = _caretImage->getColor();
            color.w *= _opacity;

            float caretWidth = region.width * _fontSize / region.height;

            Font* font = getFont(state);
            unsigned int fontSize = getFontSize(state);
            Vector2 point;
            font->getLocationAtIndex(getDisplayedText().c_str(), _textBounds, fontSize, &point, _caretLocation, 
                 getTextAlignment(state), true, getTextRightToLeft(state));

            SpriteBatch* batch = _style->getTheme()->getSpriteBatch();
            startBatch(form, batch);
            batch->draw(point.x - caretWidth * 0.5f, point.y, caretWidth, fontSize, uvs.u1, uvs.v1, uvs.u2, uvs.v2, color, _viewportClipBounds);
            finishBatch(form, batch);

            return 1;
        }
    }

    return 0;
}

unsigned int TextBox::drawText(Form* form, const Rectangle& clip)
{
    if (_text.size() <= 0)
        return 0;

    // Draw the text.
    if (_font)
    {
        Control::State state = getState();
        const std::wstring displayedText = getDisplayedText();
        unsigned int fontSize = getFontSize(state);

        SpriteBatch* batch = _font->getSpriteBatch(fontSize);
        startBatch(form, batch);
        _font->drawText(displayedText.c_str(), _textBounds, _textColor, fontSize, getTextAlignment(state), true, getTextRightToLeft(state), _viewportClipBounds);
        finishBatch(form, batch);

        return 1;
    }

    return 0;
}

void TextBox::setText(char const *text)
{
    Label::setText(text);
    if (_caretLocation > _text.length())
    {
        _caretLocation = _text.length();
    }
    notifyListeners(Control::Listener::TEXT_CHANGED);
}

void TextBox::setCaretLocation(int x, int y)
{
    Control::State state = getState();

    Vector2 point(x + _absoluteBounds.x, y + _absoluteBounds.y);

    // Get index into string and cursor location from the latest touch location.
    Font* font = getFont(state);
    unsigned int fontSize = getFontSize(state);
    Font::Justify textAlignment = getTextAlignment(state);
    bool rightToLeft = getTextRightToLeft(state);
    const std::wstring displayedText = getDisplayedText();

    int index = font->getIndexAtLocation(displayedText.c_str(), _textBounds, fontSize, point, &point,
            textAlignment, true, rightToLeft);

    if (index == -1)
    {
        // Attempt to find the nearest valid caret location.
        Rectangle textBounds;
        font->measureText(displayedText.c_str(), _textBounds, fontSize, &textBounds, textAlignment, true, true);

        if (point.x > textBounds.x + textBounds.width &&
            point.y > textBounds.y + textBounds.height)
        {
            font->getLocationAtIndex(displayedText.c_str(), _textBounds, fontSize, &point, (unsigned int)_text.length(),
                textAlignment, true, rightToLeft);
            return;
        }

        if (point.x < textBounds.x)
        {
            point.x = textBounds.x;
        }
        else if (point.x > textBounds.x + textBounds.width)
        {
            point.x = textBounds.x + textBounds.width;
        }

        if (point.y < textBounds.y)
        {
            point.y = textBounds.y;
        }
        else if (point.y > textBounds.y + textBounds.height)
        {
            Font* font = getFont(state);
            GP_ASSERT(font);
            unsigned int fontSize = getFontSize(state);
            point.y = textBounds.y + textBounds.height - fontSize;
        }

        index = font->getIndexAtLocation(displayedText.c_str(), _textBounds, fontSize, point, &point,
            textAlignment, true, rightToLeft);
    }

    if (index != -1)
    {
        _caretLocation = index;
    }
    else
    {
        _caretLocation = _text.length();
    }
}

void TextBox::getCaretLocation(Vector2* p)
{
    GP_ASSERT(p);

    State state = getState();
    getFont(state)->getLocationAtIndex(getDisplayedText().c_str(), _textBounds, getFontSize(state), p, _caretLocation, getTextAlignment(state), true, getTextRightToLeft(state));
}

void TextBox::setPasswordChar(char character)
{
    _passwordChar = character;
}

char TextBox::getPasswordChar() const
{
    return _passwordChar;
}

void TextBox::setInputMode(InputMode inputMode)
{
    _inputMode = inputMode;
}

TextBox::InputMode TextBox::getInputMode() const
{
    return _inputMode;
}

TextBox::InputMode TextBox::getInputMode(const char* inputMode)
{
    if (!inputMode)
    {
        return TextBox::TEXT;
    }

    if (strcmp(inputMode, "TEXT") == 0)
    {
        return TextBox::TEXT;
    }
    else if (strcmp(inputMode, "PASSWORD") == 0)
    {
        return TextBox::PASSWORD;
    }
    else
    {
        GP_ERROR("Failed to get corresponding textbox inputmode for unsupported value '%s'.", inputMode);
    }

    // Default.
    return TextBox::TEXT;
}

std::wstring TextBox::getDisplayedText() const
{
    std::wstring displayedText;
    switch (_inputMode) {
        case PASSWORD:
            displayedText.insert((size_t)0, _text.length(), _passwordChar);
            break;

        case TEXT:
        default:
            displayedText = _text;
            break;
    }

    return displayedText;
}

}
