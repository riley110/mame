// license:BSD-3-Clause
// copyright-holders:Vas Crabb
//============================================================
//
//  debugview.h - MacOS X Cocoa debug window handling
//
//  Copyright (c) 1996-2015, Nicola Salmoria and the MAME Team.
//  Visit http://mamedev.org for licensing and usage restrictions.
//
//============================================================

#import "debugosx.h"

#include "emu.h"
#include "debug/debugvw.h"

#import <Cocoa/Cocoa.h>


@interface MAMEDebugView : NSView
{
	int				type;
	running_machine	*machine;
	debug_view		*view;
	BOOL			wholeLineScroll;

	INT32			totalWidth, totalHeight, originTop;

	NSFont			*font;
	CGFloat			fontWidth, fontHeight, fontAscent;

	NSTextStorage	*text;
	NSTextContainer	*textContainer;
	NSLayoutManager	*layoutManager;
}

+ (NSFont *)defaultFont;

- (id)initWithFrame:(NSRect)f type:(debug_view_type)t machine:(running_machine &)m wholeLineScroll:(BOOL)w;

- (void)update;

- (NSSize)maximumFrameSize;

- (NSFont *)font;
- (void)setFont:(NSFont *)f;

- (BOOL)cursorSupported;
- (BOOL)cursorVisible;
- (debug_view_xy)cursorPosition;

- (IBAction)copyVisible:(id)sender;
- (IBAction)paste:(id)sender;

- (void)viewBoundsDidChange:(NSNotification *)notification;
- (void)viewFrameDidChange:(NSNotification *)notification;

- (void)windowDidBecomeKey:(NSNotification *)notification;
- (void)windowDidResignKey:(NSNotification *)notification;

- (void)addContextMenuItemsToMenu:(NSMenu *)menu;

@end


@protocol MAMEDebugViewSubviewSupport <NSObject>

- (NSString *)selectedSubviewName;
- (int)selectedSubviewIndex;
- (void)selectSubviewAtIndex:(int)index;
- (BOOL)selectSubviewForDevice:(device_t *)device;

@end


@protocol MAMEDebugViewExpressionSupport <NSObject>

- (NSString *)expression;
- (void)setExpression:(NSString *)exp;

@end
