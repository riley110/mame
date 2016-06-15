// license:BSD-3-Clause
// copyright-holders:Nicola Salmoria, Aaron Giles, Nathan Woods
/*********************************************************************

	text.cpp

	Text functionality for MAME's crude user interface

*********************************************************************/

#include "text.h"
#include "rendfont.h"
#include "render.h"

namespace ui {

/***************************************************************************
INLINE FUNCTIONS
***************************************************************************/

//-------------------------------------------------
//  is_space_character
//-------------------------------------------------

inline bool is_space_character(unicode_char ch)
{
	return ch == ' ';
}


//-------------------------------------------------
//  is_breakable_char - is a given unicode
//  character a possible line break?
//-------------------------------------------------

inline bool is_breakable_char(unicode_char ch)
{
	// regular spaces and hyphens are breakable
	if (is_space_character(ch) || ch == '-')
		return true;

	// In the following character sets, any character is breakable:
	//  Hiragana (3040-309F)
	//  Katakana (30A0-30FF)
	//  Bopomofo (3100-312F)
	//  Hangul Compatibility Jamo (3130-318F)
	//  Kanbun (3190-319F)
	//  Bopomofo Extended (31A0-31BF)
	//  CJK Strokes (31C0-31EF)
	//  Katakana Phonetic Extensions (31F0-31FF)
	//  Enclosed CJK Letters and Months (3200-32FF)
	//  CJK Compatibility (3300-33FF)
	//  CJK Unified Ideographs Extension A (3400-4DBF)
	//  Yijing Hexagram Symbols (4DC0-4DFF)
	//  CJK Unified Ideographs (4E00-9FFF)
	if (ch >= 0x3040 && ch <= 0x9fff)
		return true;

	// Hangul Syllables (AC00-D7AF) are breakable
	if (ch >= 0xac00 && ch <= 0xd7af)
		return true;

	// CJK Compatibility Ideographs (F900-FAFF) are breakable
	if (ch >= 0xf900 && ch <= 0xfaff)
		return true;

	return false;
}



/***************************************************************************
CORE IMPLEMENTATION
***************************************************************************/

//-------------------------------------------------
//  ctor
//-------------------------------------------------

text_layout::text_layout(render_font &font, float xscale, float yscale, float width, text_layout::text_justify justify, text_layout::word_wrapping wrap)
	: m_font(font), m_xscale(xscale), m_yscale(yscale), m_width(width), m_justify(justify), m_wrap(wrap), m_current_line(nullptr), m_last_break(0), m_text_position(0)

{
}


//-------------------------------------------------
//  ctor (move)
//-------------------------------------------------

text_layout::text_layout(text_layout &&that)
	: m_font(that.m_font), m_xscale(that.m_xscale), m_yscale(that.m_yscale), m_width(that.m_width), m_justify(that.m_justify), m_wrap(that.m_wrap), m_lines(std::move(that.m_lines)),
	  m_current_line(that.m_current_line), m_last_break(that.m_last_break), m_text_position(that.m_text_position)
{
}


//-------------------------------------------------
//  dtor
//-------------------------------------------------

text_layout::~text_layout()
{
}


//-------------------------------------------------
//  add_text
//-------------------------------------------------

void text_layout::add_text(const char *text, const char_style &style)
{
	int position = 0;
	int text_length = strlen(text);

	while(position < text_length)
	{
		// do we need to create a new line?
		if (m_current_line == nullptr)
		{
			// get the current character
			unicode_char schar;
			int scharcount;
			scharcount = uchar_from_utf8(&schar, &text[position], text_length - position);
			if (scharcount == -1)
				break;

			// if the line starts with a tab character, center it regardless
			text_justify line_justify = justify();
			if (schar == '\t')
			{
				position += scharcount;
				line_justify = text_layout::CENTER;
			}

			// start a new line
			start_new_line(line_justify, style.size);
		}

		// get the current character
		int scharcount;
		unicode_char ch;
		scharcount = uchar_from_utf8(&ch, &text[position], text_length - position);
		if (scharcount < 0)
			break;
		position += scharcount;

		// set up source information
		source_info source = { 0, };
		source.start = m_text_position;
		source.span = scharcount;
		m_text_position += scharcount;

		// is this an endline?
		if (ch == '\n')
		{
			// first, start a line if we have not already
			if (m_current_line == nullptr)
				start_new_line(LEFT, style.size);

			// and then close up the current line
			update_maximum_line_width();
			m_current_line = nullptr;
		}
		else
		{
			// if we hit a space, remember the location and width *without* the space
			if (is_space_character(ch))
				m_last_break = m_current_line->character_count();

			// append the character
			m_current_line->add_character(ch, style, source);

			// do we have to wrap?
			if (wrap() != NEVER && m_current_line->width() > m_width)
			{
				switch (wrap())
				{
					case TRUNCATE:
						truncate_wrap();
						break;

					case WORD:
						word_wrap();
						break;

					default:
						fatalerror("invalid word wrapping value");
						break;
				}
			}
			else
			{
				// we didn't wrap - if we hit any non-space breakable character, remember the location and width
				// *with* the breakable character
				if (ch != ' ' && is_breakable_char(ch))
					m_last_break = m_current_line->character_count();
			}
		}
	}
}


//-------------------------------------------------
//  update_maximum_line_width
//-------------------------------------------------

void text_layout::update_maximum_line_width()
{
	m_maximum_line_width = actual_width();
}


//-------------------------------------------------
//  actual_width
//-------------------------------------------------

float text_layout::actual_width() const
{
	float current_line_width = m_current_line ? m_current_line->width() : 0;
	return MAX(m_maximum_line_width, current_line_width);
}


//-------------------------------------------------
//  actual_height
//-------------------------------------------------

float text_layout::actual_height() const
{
	line *last_line = (m_lines.size() > 0)
		? m_lines[m_lines.size() - 1].get()
		: nullptr;
	return last_line
		? last_line->yoffset() + last_line->height()
		: 0;
}


//-------------------------------------------------
//  start_new_line
//-------------------------------------------------

void text_layout::start_new_line(text_layout::text_justify justify, float height)
{
	// create a new line
	std::unique_ptr<line> new_line(global_alloc_clear<line>(*this, justify, actual_height(), height * yscale()));

	// update the current line
	update_maximum_line_width();
	m_current_line = new_line.get();
	m_last_break = 0;

	// append it
	m_lines.push_back(std::move(new_line));
}


//-------------------------------------------------
//  get_char_width
//-------------------------------------------------

float text_layout::get_char_width(unicode_char ch, float size)
{
	return font().char_width(size * yscale(), xscale() / yscale(), ch);
}


//-------------------------------------------------
//  truncate_wrap
//-------------------------------------------------

void text_layout::truncate_wrap()
{
	// for now, lets assume that we're only truncating the last character
	size_t truncate_position = m_current_line->character_count() - 1;
	const auto& truncate_char = m_current_line->character(truncate_position);

	// copy style information
	char_style style = truncate_char.style;

	// copy source information
	source_info source = { 0, };
	source.start = truncate_char.source.start + truncate_char.source.span;
	source.span = 0;

	// figure out how wide an elipsis is
	float elipsis_width = 3 * get_char_width('.', style.size);

	// where should we really truncate from?
	while (truncate_position > 0 && m_current_line->character(truncate_position).xoffset + elipsis_width < width())
		truncate_position--;

	// truncate!!!
	m_current_line->truncate(truncate_position);

	// and append the elipsis
	m_current_line->add_character('.', style, source);

	// finally start a new line
	start_new_line(m_current_line->justify(), style.size);
}


//-------------------------------------------------
//  word_wrap
//-------------------------------------------------

void text_layout::word_wrap()
{
	// keep track of the last line and break
	line *last_line = m_current_line;
	size_t last_break = m_last_break;

	// start a new line with the same justification
	start_new_line(last_line->justify(), last_line->character(last_line->character_count() - 1).style.size);

	// find the begining of the word to wrap
	size_t position = last_break;
	while (position + 1 < last_line->character_count() && is_space_character(last_line->character(position).character))
		position++;

	// transcribe the characters
	for (size_t i = position; i < last_line->character_count(); i++)
	{
		auto &ch = last_line->character(i);
		m_current_line->add_character(ch.character, ch.style, ch.source);
	}

	// and finally, truncate the last line
	last_line->truncate(last_break);
}


//-------------------------------------------------
//  hit_test
//-------------------------------------------------

bool text_layout::hit_test(float x, float y, size_t &start, size_t &span) const
{
	for (const auto &line : m_lines)
	{
		if (y >= line->yoffset() && y < line->yoffset() + line->height())
		{
			float line_xoffset = line->xoffset();
			if (x >= line_xoffset && x < line_xoffset + line->width())
			{
				for (size_t i = 0; i < line->character_count(); i++)
				{
					const auto &ch = line->character(i);
					if (x >= ch.xoffset && x < ch.xoffset + ch.xwidth)
					{
						start = ch.source.start;
						span = ch.source.span;
						return true;
					}
				}
			}
		}
	}
	start = 0;
	span = 0;
	return false;
}


//-------------------------------------------------
//  restyle
//-------------------------------------------------

void text_layout::restyle(size_t start, size_t span, rgb_t *fgcolor, rgb_t *bgcolor)
{
	for (const auto &line : m_lines)
	{
		for (size_t i = 0; i < line->character_count(); i++)
		{
			auto &ch = line->character(i);
			if (ch.source.start >= start && ch.source.start + ch.source.span <= start + span)
			{
				if (fgcolor != nullptr)
					ch.style.fgcolor = *fgcolor;
				if (bgcolor != nullptr)
					ch.style.bgcolor = *bgcolor;
			}
		}
	}
}


//-------------------------------------------------
//  get_wrap_info
//-------------------------------------------------

int text_layout::get_wrap_info(std::vector<int> &xstart, std::vector<int> &xend) const
{
	// this is a hacky method (tailored to the need to implement
	// mame_ui_manager::wrap_text) but so be it
	int line_count = 0;
	for (const auto &line : m_lines)
	{
		int start_pos = 0;
		int end_pos = 0;

		auto line_character_count = line->character_count();
		if (line_character_count > 0)
		{
			start_pos = line->character(0).source.start;
			end_pos = line->character(line_character_count - 1).source.start
				+ line->character(line_character_count - 1).source.span;
		}

		line_count++;
		xstart.push_back(start_pos);
		xend.push_back(end_pos);
	}
	return line_count;
}


//-------------------------------------------------
//  emit
//-------------------------------------------------

void text_layout::emit(render_container *container, float x, float y)
{
	for (const auto &line : m_lines)
	{
		float line_xoffset = line->xoffset();

		// emit every single character
		for (auto i = 0; i < line->character_count(); i++)
		{
			auto &ch = line->character(i);
			
			// position this specific character correctly (TODO - this doesn't
			// handle differently sized text (yet)
			float char_x = x + line_xoffset + ch.xoffset;
			float char_y = y + line->yoffset();
			float char_width = ch.xwidth;
			float char_height = line->height();

			// render the background of the character (if present)
			if (ch.style.bgcolor.a() != 0)
				container->add_rect(char_x, char_y, char_x + char_width, char_y + char_height, ch.style.bgcolor, PRIMFLAG_BLENDMODE(BLENDMODE_ALPHA));

			// render the foreground
			container->add_char(
				char_x,
				char_y,
				char_height,
				xscale() / yscale(),
				ch.style.fgcolor,
				font(),
				ch.character);
		}
	}
}


//-------------------------------------------------
//  line::ctor
//-------------------------------------------------

text_layout::line::line(text_layout &layout, text_justify justify, float yoffset, float height)
	: m_layout(layout), m_justify(justify), m_yoffset(yoffset), m_width(0.0), m_height(height)
{
}


//-------------------------------------------------
//  line::add_character
//-------------------------------------------------

void text_layout::line::add_character(unicode_char ch, const char_style &style, const source_info &source)
{
	// get the width of this character
	float chwidth = m_layout.get_char_width(ch, style.size);

	// create the positioned character
	positioned_char positioned_char = { 0, };
	positioned_char.character = ch;
	positioned_char.xoffset = m_width;
	positioned_char.xwidth = chwidth;
	positioned_char.style = style;
	positioned_char.source = source;

	// append the character
	m_characters.push_back(positioned_char);
	m_width += chwidth;

	// we might be bigger
	m_height = MAX(m_height, style.size * m_layout.yscale());
}


//-------------------------------------------------
//  line::xoffset
//-------------------------------------------------

float text_layout::line::xoffset() const
{
	float result;
	switch (justify())
	{
		case LEFT:
		default:
			result = 0;
			break;
		case CENTER:
			result = (m_layout.width() - width()) / 2;
			break;
		case RIGHT:
			result = m_layout.width() - width();
			break;
	}
	return result;
}


//-------------------------------------------------
//  line::truncate
//-------------------------------------------------

void text_layout::line::truncate(size_t position)
{
	assert(position <= m_characters.size());

	// are we actually truncating?
	if (position < m_characters.size())
	{
		// set the width as appropriate
		m_width = m_characters[position].xoffset;

		// and resize the array
		m_characters.resize(position);
	}
}

} // namespace ui
