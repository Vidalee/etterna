#include "global.h"
/*
-----------------------------------------------------------------------------
 File: CombinedLifeMeterTug.h

 Desc: The song's CombinedLifeMeterTug displayed in SelectSong.

 Copyright (c) 2001-2002 by the person(s) listed below.  All rights reserved.
-----------------------------------------------------------------------------
*/

#include "CombinedLifeMeterTug.h"
#include "ThemeManager.h"
#include "GameState.h"
#include "PrefsManager.h"


const float METER_WIDTH = 550;

const float FACE_X[NUM_PLAYERS] = { -300, +300 };
const float FACE_Y[NUM_PLAYERS] = { 0, 0 };


CombinedLifeMeterTug::CombinedLifeMeterTug() 
{
	int p;
	for( p=0; p<NUM_PLAYERS; p++ )
	{
		m_Stream[p].Load( THEME->GetPathToG(ssprintf("CombinedLifeMeterTug stream p%d",p+1)), METER_WIDTH );
		this->AddChild( &m_Stream[p] );
	}
	m_Stream[PLAYER_2].SetZoomX( -1 );

	m_sprFrame.Load( THEME->GetPathToG(ssprintf("CombinedLifeMeterTug frame")) );
	this->AddChild( &m_sprFrame );
	
	
	for( p=0; p<NUM_PLAYERS; p++ )
	{
		Character* pCharacter = GAMESTATE->m_pCurCharacters[p];
		ASSERT( pCharacter );

		m_Head[p].LoadFromCharacter( pCharacter );
		m_Head[p].SetXY( FACE_X[p], FACE_Y[p] );
		this->AddChild( &m_Head[p] );
	}
}

void CombinedLifeMeterTug::Update( float fDelta )
{
	m_Stream[PLAYER_1].SetPercent( GAMESTATE->m_fTugLifePercentP1 );
	m_Stream[PLAYER_2].SetPercent( 1-GAMESTATE->m_fTugLifePercentP1 );

	ActorFrame::Update( fDelta );
}

void CombinedLifeMeterTug::ChangeLife( PlayerNumber pn, TapNoteScore score )
{
	float fPercentToMove = 0;
	switch( score )
	{
	case TNS_MARVELOUS:		fPercentToMove = PREFSMAN->m_fLifeDeltaMarvelousPercentChange;	break;
	case TNS_PERFECT:		fPercentToMove = PREFSMAN->m_fLifeDeltaPerfectPercentChange;	break;
	case TNS_GREAT:			fPercentToMove = PREFSMAN->m_fLifeDeltaGreatPercentChange;		break;
	case TNS_GOOD:			fPercentToMove = PREFSMAN->m_fLifeDeltaGoodPercentChange;		break;
	case TNS_BOO:			fPercentToMove = PREFSMAN->m_fLifeDeltaBooPercentChange;		break;
	case TNS_MISS:			fPercentToMove = PREFSMAN->m_fLifeDeltaMissPercentChange;		break;
	default:	ASSERT(0);	break;
	}

	switch( pn )
	{
	case PLAYER_1:	GAMESTATE->m_fTugLifePercentP1 += fPercentToMove;	break;
	case PLAYER_2:	GAMESTATE->m_fTugLifePercentP1 -= fPercentToMove;	break;
	default:	ASSERT(0);
	}

	CLAMP( GAMESTATE->m_fTugLifePercentP1, 0, 1 );
}

void CombinedLifeMeterTug::ChangeLife( PlayerNumber pn, HoldNoteScore score, TapNoteScore tscore )
{
	/* The initial tap note score (which we happen to have in have in
	 * tscore) has already been reported to the above function.  If the
	 * hold end result was an NG, count it as a miss; if the end result
	 * was an OK, count a perfect.  (Remember, this is just life meter
	 * computation, not scoring.) */
	float fPercentToMove = 0;
	switch( score )
	{
	case HNS_OK:			fPercentToMove = PREFSMAN->m_fLifeDeltaOKPercentChange;	break;
	case HNS_NG:			fPercentToMove = PREFSMAN->m_fLifeDeltaNGPercentChange;	break;
	default:	ASSERT(0);	break;
	}

	switch( pn )
	{
	case PLAYER_1:	GAMESTATE->m_fTugLifePercentP1 += fPercentToMove;	break;
	case PLAYER_2:	GAMESTATE->m_fTugLifePercentP1 -= fPercentToMove;	break;
	default:	ASSERT(0);
	}

	CLAMP( GAMESTATE->m_fTugLifePercentP1, 0, 1 );
}

void CombinedLifeMeterTug::ChangeLifeMine( PlayerNumber pn )
{
	float fPercentToMove = PREFSMAN->m_fLifeDeltaMinePercentChange;
	switch( pn )
	{
	case PLAYER_1:	GAMESTATE->m_fTugLifePercentP1 += fPercentToMove;	break;
	case PLAYER_2:	GAMESTATE->m_fTugLifePercentP1 -= fPercentToMove;	break;
	default:	ASSERT(0);
	}

	CLAMP( GAMESTATE->m_fTugLifePercentP1, 0, 1 );
	
}

