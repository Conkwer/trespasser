/**********************************************************************************************
 *
 * Copyright � DreamWorks Interactive, 1996
 *
 * Contents:
 *		Definition CTextOverlay
 *
 * Bugs:
 *
 * To do:
 *
 * Notes:
 *
 **********************************************************************************************
 *
 * $Log:: /JP2_PC/Source/Lib/EntityDBase/TextOverlay.hpp                                      $
 * 
 * 5     9/30/98 2:13a Rwyatt
 * New members
 * 
 * 4     9/29/98 12:59a Rwyatt
 * Added system message
 * 
 * 3     9/23/98 5:52p Rwyatt
 * Added a type field so we can distinguish between tutorial and subtitles
 * 
 * 2     9/18/98 2:04a Rwyatt
 * Added a extern definition for a function to lookup a resource
 * 
 * 1     9/17/98 2:50p Rwyatt
 * Initial Implementation
 * 
 *********************************************************************************************/

#ifndef HEADER_ENTITYDBASE_OVERLAYTEXT_HPP
#define HEADER_ENTITYDBASE_OVERLAYTEXT_HPP

#include <list.h>
#include "Lib/EntityDBase/MessageTypes/MsgStep.hpp"
#include "Lib/EntityDBase/Entity.hpp"
#include "Lib/EntityDBase/Subsystem.hpp"
#include "Lib/W95/DDFont.hpp"
#include "Lib/View/Colour.hpp"

extern uint32 u4LookupResourceString(int32 id,char* str_buf,uint32 u4_buf_len);

//**********************************************************************************************
#define TEXT_FORMAT_RAW		0x00000100
#define TEXT_FORMAT_SOLID	0x00000200
#define TEXT_FORMAT_IGNORE	0x00000400
#define TEXT_FORMAT_TIMED	0x00000800
#define TEXT_FORMAT_UTF8	0x00001000

//**********************************************************************************************
enum ETextType
// prefix: ett
{
	ettALL,
	ettUNKNOWN,
	ettSUBTITLE,
	ettTUTORIAL
};

//**********************************************************************************************
// prefix: tel
struct STextElement
{
	TSec	sRemove;
	TSec	sRemove2;							// removal time for TEXT_FORMAT_TIMED lines
	uint32		u4Row;							// stable stack row (0 = bottom line) for subtitles
	union
	{
		uint8*		pu1FormattedData;			// either a pointer to formatted data
		char*		strString;					// or a bare string
	};
	uint32			u4XPos;
	uint32			u4YPos;
	CColour			clrText;
	uint32			u4Flags;
	STextElement*	ptelNext;					// next piece of text in the sequence
	ETextType		ettType;					// type of this text line
};


//**********************************************************************************************
// prefix: ttl
typedef list<STextElement>		TTextList;


//*********************************************************************************************
//
class CTextOverlay : public CSubsystem
// prefix: tov
{
public:

	CDDFont*	pfntOverlayFont;
	TTextList	ttlTextItems;

	//*****************************************************************************************
	//
	// Constructors and destructors
	//
	CTextOverlay();
	virtual ~CTextOverlay();

	//*****************************************************************************************
	//
	// Overrides
	//
	void Process(const CMessageStep& msg_step);
	void Process(const CMessagePaint& msgpaint);
	void Process(const CMessageSystem& msg);

	
	//*****************************************************************************************
	uint32 u4DisplayFormattedString
	(
		char*		str_text,			// C style text string
		TSec		s_time,				// display time
		uint32		u4_flags,			// formatting flags
		CColour		clr,				// colour of the text
		uint32		u4_prev = 0,
		ETextType	ett = ettUNKNOWN
	);

	//*****************************************************************************************
	uint32 u4DisplayPositionedString
	(
		char*		str_text,			// C style text string
		TSec		s_time,				// display time
		uint32		u4_xpos,			// screen x position
		uint32		u4_ypos,			// screen y position
		CColour		clr,				// colour
		uint32		u4_prev = 0,
		ETextType	ett = ettUNKNOWN
	);

	//*****************************************************************************************
	// Display a UTF-8 encoded string (e.g. an SRT subtitle line) using the TTF font.
	// The line becomes visible at s_start seconds and is removed at s_end seconds.
	uint32 u4DisplayUTF8String
	(
		char*		str_text,			// UTF-8 C style text string
		TSec		s_start,			// time from now to display the line
		TSec		s_end,				// time from now to remove the line
		uint32		u4_flags,			// formatting flags
		CColour		clr,				// colour of the text
		ETextType	ett = ettSUBTITLE,
		uint32		u4_row = 0			// stable stack row (0 = bottom) for subtitles
	);

	//*****************************************************************************************
	uint32 u4FindSequenceEnd
	(
		ETextType	ett
	);

	//*****************************************************************************************
	// Lowest stack row not currently reserved by a subtitle element (visible or scheduled).
	uint32 u4NextFreeSubtitleRow();

	//*****************************************************************************************
	void RemoveText
	(
		ETextType	ett
	);

	//*****************************************************************************************
	void RemoveAll();

	//*****************************************************************************************
	void ResetScreen();

	//*****************************************************************************************
	// The global text system pointer.
	static CTextOverlay* ptovTextSystem;
};

extern CTextOverlay* ptovTextSystem;


#endif