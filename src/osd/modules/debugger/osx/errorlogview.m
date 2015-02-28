// license:BSD-3-Clause
// copyright-holders:Vas Crabb
//============================================================
//
//  errorlogview.m - MacOS X Cocoa debug window handling
//
//  Copyright (c) 1996-2015, Nicola Salmoria and the MAME Team.
//  Visit http://mamedev.org for licensing and usage restrictions.
//
//============================================================

#import "errorlogview.h"

#include "debug/debugvw.h"


@implementation MAMEErrorLogView

- (id)initWithFrame:(NSRect)f machine:(running_machine &)m {
	if (!(self = [super initWithFrame:f type:DVT_LOG machine:m wholeLineScroll:NO]))
		return nil;
	return self;
}


- (void)dealloc {
	[super dealloc];
}

@end
