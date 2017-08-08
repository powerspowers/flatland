//******************************************************************************
// $Header$
// Copyright (C) 1998-2002 Flatland Online Inc.
// All Rights Reserved. 
//******************************************************************************

// Tokens.

enum tokens {

	// No token.

	TOKEN_NONE,

	// XML tokens.

	TOKEN_OPEN_TAG,	
	TOKEN_CLOSE_TAG,
	TOKEN_OPEN_END_TAG,
	TOKEN_CLOSE_SINGLE_TAG,
	TOKEN_OPEN_COMMENT,	
	TOKEN_CLOSE_COMMENT,
	TOKEN_EQUALS_SIGN,
	TOKEN_QUOTE,
	TOKEN_WHITESPACE,

	// Generic token types.

	TOKEN_CHARACTER_DATA,
	TOKEN_COMMENT,
	TOKEN_STRING,
	TOKEN_IDENTIFIER,

	// Tag and parameter name tokens.

	TOKEN_ACTION,
	TOKEN_ALIGN,
	TOKEN_AMBIENT_LIGHT,
	TOKEN_AMBIENT_SOUND,
	TOKEN_ANGLE,
	TOKEN_ANGLES,
	TOKEN_ANIMATE,
	TOKEN_AREA,
	TOKEN_BASE,
	TOKEN_BLOCK,
	TOKEN_BLOCKSET,
	TOKEN_BODY,
	TOKEN_BRIGHTNESS,
	TOKEN_BSP_TREE,
	TOKEN_CACHE,
	TOKEN_CAMERA,
	TOKEN_CATEGORY,
	TOKEN_CLASS,
	TOKEN_COLOUR,
	TOKEN_COMMAND,
	TOKEN_CONE,
	TOKEN_CONTENT,
	TOKEN_COORDS,
	TOKEN_CREATE,
	TOKEN_DAMP,
	TOKEN_DEBUG,
	TOKEN_DEFINE,
	TOKEN_DELAY,
	TOKEN_DENSITY,
	TOKEN_DIMENSIONS,
	TOKEN_DIRECTION,
	TOKEN_DIRECTORY,
	TOKEN_DISTANCE,
	TOKEN_DOUBLE,
	TOKEN_DROPRATE,
	TOKEN_END,
	TOKEN_ENTRANCE,
	TOKEN_EXIT,
	TOKEN_EXTENDS,
	TOKEN_FACES,
	TOKEN_FAST_MODE,
	TOKEN_FOG,
	TOKEN_FILE,
	TOKEN_FLOOD,
	TOKEN_FORCE,
	TOKEN_FRAME,
	TOKEN_FRAMES,
	TOKEN_FRONT,
	TOKEN_GO_FASTER,
	TOKEN_GO_SLOWER,
	TOKEN_GROUND,
	TOKEN_HEAD,
	TOKEN_HREF,
	TOKEN_ICON,
	TOKEN_ID,
	TOKEN_IMAGEMAP,
	TOKEN_IMPORT,
	TOKEN_IS_SPOT,
	TOKEN_JUMP,
	TOKEN_KEY,
	TOKEN_LEVEL,
	TOKEN_LOAD,
	TOKEN_LOCATION,
	TOKEN_LOOK_DOWN,
	TOKEN_LOOK_UP,
	TOKEN_LOOPS,
	TOKEN_LOOP,
	TOKEN_MAP,
	TOKEN_MAX_SPEED,
	TOKEN_META,
	TOKEN_MOVABLE,
	TOKEN_MOVE,
	TOKEN_MOVE_BACK,
	TOKEN_MOVE_FORWARD,
	TOKEN_MOVE_LEFT,
	TOKEN_MOVE_RIGHT,
	TOKEN_NAME,
	TOKEN_NUMBER,
	TOKEN_ORB,
	TOKEN_ORBIT,
	TOKEN_ORIENTATION,
	TOKEN_ORIGINS,
	TOKEN_PARAM,
	TOKEN_PART,
	TOKEN_PARTNAME,
	TOKEN_PARTS,
	TOKEN_PATTERN,
	TOKEN_PLACEHOLDER,
	TOKEN_PLACEMENT,
	TOKEN_PLAYBACK,
	TOKEN_PLAYER,
	TOKEN_POINT_LIGHT,
	TOKEN_POLYGON,
	TOKEN_POLYGONS,
	TOKEN_POPUP,
	TOKEN_PORT,
	TOKEN_POSITION,
	TOKEN_PROJECTION,
	TOKEN_RADIUS,
	TOKEN_REAR,

#ifdef STREAMING_MEDIA
	TOKEN_RECT,
#endif

	TOKEN_REF,
	TOKEN_REPLACE,
	TOKEN_RIPPLE,
	TOKEN_ROLLOFF,
	TOKEN_ROOT,
	TOKEN_ROTATE,

#ifdef STREAMING_MEDIA
	TOKEN_RP,
#endif

	TOKEN_SCALE,
	TOKEN_SETFRAME,
	TOKEN_SETLOOP,
	TOKEN_SCRIPT,
	TOKEN_SHAPE,
	TOKEN_SIDLE_MODE,
	TOKEN_SIZE,
	TOKEN_SKY,
	TOKEN_SOLID,
	TOKEN__SOURCE,
	TOKEN_SOUND,
	TOKEN_SPDESC,
	TOKEN_SPEED,
	TOKEN_SPIN,
	TOKEN_SPOT,
	TOKEN_SPOT_LIGHT,
	TOKEN_SPRITE,
	TOKEN_START,
	TOKEN_STOPAT,
	TOKEN_STOPSPIN,
	TOKEN_STOPMOVE,
	TOKEN_STOPRIPPLE,
	TOKEN_STOPORBIT,

#ifdef STREAMING_MEDIA
	TOKEN_STREAM,
#endif

	TOKEN_STYLE,
	TOKEN_SYMBOL,
	TOKEN_SYNOPSIS,
	TOKEN_TARGET,
	TOKEN_TEXCOORDS,
	TOKEN_TEXT,
	TOKEN_TEXTALIGN,
	TOKEN_TEXTCOLOUR,
	TOKEN_TEXTURE,
	TOKEN_TITLE,
	TOKEN_TRANSLUCENCY,
	TOKEN_TRIGGER,
	TOKEN__TYPE,
	TOKEN_UPDATED,
	TOKEN_VELOCITY,
	TOKEN_VERSION,
	TOKEN_VERTEX,
	TOKEN_VERTICES,
	TOKEN_VOLUME,
	TOKEN_WARNINGS,

#ifdef STREAMING_MEDIA
	TOKEN_WMP
#endif

};

// Value types.

enum values {
	VALUE_ACTION_TRIGGER,
	VALUE_ALIGNMENT,
	VALUE_ANGLES,
	VALUE_BLOCK_REF,
	VALUE_BLOCK_TYPE,
	VALUE_BOOLEAN,
	VALUE_DEGREES,
	VALUE_DELAY_RANGE,
	VALUE_DIRECTION,
	VALUE_DIRRANGE,
	VALUE_DOUBLE_SYMBOL,
	VALUE_EXIT_TRIGGER,
	VALUE_FOG_STYLE,
	VALUE_FORCE,
	VALUE_FLOAT,
	VALUE_HEADING,
	VALUE_IMAGEMAP_TRIGGER,
	VALUE_INTEGER,
	VALUE_INTEGER_RANGE,
	VALUE_KEY_CODE,
	VALUE_MAP_COORDS,
	VALUE_MAP_DIMENSIONS,
	VALUE_MAP_STYLE,
	VALUE_NAME,
	VALUE_NAME_LIST,
	VALUE_NAME_OR_WILDCARD,
	VALUE_ORIENTATION,
	VALUE_PCRANGE,
	VALUE_PERCENTAGE,
	VALUE_PLACEMENT,
	VALUE_PLAYBACK_MODE,
	VALUE_POINT_LIGHT_STYLE,
	VALUE_POPUP_TRIGGER,
	VALUE_PROJECTION,
	VALUE_RADIUS,

#ifdef STREAMING_MEDIA
	VALUE_RECT,
#endif

	VALUE_REL_COORDS,
	VALUE_REL_INTEGER,
	VALUE_REL_INTEGER_TRIPLET,
	VALUE_RGB,
	VALUE_RIPPLE_STYLE,
	VALUE_SCALE,
	VALUE_SHAPE,
	VALUE_SINGLE_SYMBOL,
	VALUE_SIZE,
	VALUE_SPOT_LIGHT_STYLE,
	VALUE_SPRITE_SIZE,
	VALUE_STRING,
	VALUE_SYMBOL,
	VALUE_TEXTURE_STYLE,
	VALUE_VALIGNMENT,
	VALUE_VERSION,
	VALUE_VERTEX_COORDS
};
