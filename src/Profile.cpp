#include "global.h"
#include "Profile.h"
#include "RageUtil.h"
#include "PrefsManager.h"
#include "XmlFile.h"
#include "IniFile.h"
#include "GameManager.h"
#include "GameState.h"
#include "RageLog.h"
#include "Song.h"
#include "SongManager.h"
#include "Steps.h"
#include "Course.h"
#include "ThemeManager.h"
#include "CryptManager.h"
#include "ProfileManager.h"
#include "RageFile.h"
#include "RageFileDriverDeflate.h"
#include "RageFileManager.h"
#include "LuaManager.h"
#include "UnlockManager.h"
#include "XmlFile.h"
#include "XmlFileUtil.h"
#include "Foreach.h"
#include "Bookkeeper.h"
#include "Game.h"
#include "CharacterManager.h"
#include "Character.h"
#include "MinaCalc.h"
#include "NoteData.h"
#include <algorithm>

const RString STATS_XML            = "Stats.xml";
const RString STATS_XML_GZ         = "Stats.xml.gz";
const RString ETT_XML = "Etterna.xml";
const RString ETT_XML_GZ = "Etterna.xml.gz";
/** @brief The filename for where one can edit their personal profile information. */
const RString EDITABLE_INI         = "Editable.ini";
/** @brief A tiny file containing the type and list priority. */
const RString TYPE_INI             = "Type.ini";
/** @brief The filename containing the signature for STATS_XML's signature. */
const RString DONT_SHARE_SIG       = "DontShare.sig";
const RString PUBLIC_KEY_FILE      = "public.key";
const RString SCREENSHOTS_SUBDIR   = "Screenshots/";
const RString EDIT_STEPS_SUBDIR    = "Edits/";
const RString EDIT_COURSES_SUBDIR  = "EditCourses/";
//const RString UPLOAD_SUBDIR         = "Upload/";
const RString RIVAL_SUBDIR         = "Rivals/";
const RString REPLAY_SUBDIR	       = "ReplayData/";

ThemeMetric<bool> SHOW_COIN_DATA( "Profile", "ShowCoinData" );
static Preference<bool> g_bProfileDataCompress( "ProfileDataCompress", false );
static ThemeMetric<RString> UNLOCK_AUTH_STRING( "Profile", "UnlockAuthString" );
#define GUID_SIZE_BYTES 8

#define MAX_EDITABLE_INI_SIZE_BYTES			2*1024		// 2KB
#define MAX_PLAYER_STATS_XML_SIZE_BYTES	\
	400 /* Songs */						\
	* 5 /* Steps per Song */			\
	* 5 /* HighScores per Steps */		\
	* 1024 /* size in bytes of a HighScores XNode */

const int DEFAULT_WEIGHT_POUNDS	= 120;
const float DEFAULT_BIRTH_YEAR= 1995;

#if defined(_MSC_VER)
#pragma warning (disable : 4706) // assignment within conditional expression
#endif

static const char* ProfileTypeNames[] = {
	"Guest",
	"Normal",
	"Test",
};
XToString(ProfileType);
StringToX(ProfileType);
LuaXType(ProfileType);


int Profile::HighScoresForASong::GetNumTimesPlayed() const
{
	int iCount = 0;
	FOREACHM_CONST( StepsID, HighScoresForASteps, m_StepsHighScores, i )
	{
		iCount += i->second.hsl.GetNumTimesPlayed();
	}
	return iCount;
}

int Profile::HighScoresForACourse::GetNumTimesPlayed() const
{
	int iCount = 0;
	FOREACHM_CONST( TrailID, HighScoresForATrail, m_TrailHighScores, i )
	{
		iCount += i->second.hsl.GetNumTimesPlayed();
	}
	return iCount;
}


void Profile::InitEditableData()
{
	m_sDisplayName = "";
	m_sCharacterID = "";
	m_sLastUsedHighScoreName = "";
	m_iWeightPounds = 0;
	m_Voomax= 0;
	m_BirthYear= 0;
	m_IgnoreStepCountCalories= false;
	m_IsMale= true;
}

void Profile::ClearStats()
{
	// don't reset the Guid
	RString sGuid = m_sGuid;
	InitAll();
	m_sGuid = sGuid;
}

RString Profile::MakeGuid()
{
	RString s;
	s.reserve( GUID_SIZE_BYTES*2 );
	unsigned char buf[GUID_SIZE_BYTES];
	CryptManager::GetRandomBytes( buf, GUID_SIZE_BYTES );
	for( unsigned i=0; i<GUID_SIZE_BYTES; i++ )
		s += ssprintf( "%02x", buf[i] );
	return s;
}

void Profile::InitGeneralData()
{
	m_sGuid = MakeGuid();

	m_SortOrder = SortOrder_Invalid;
	m_LastDifficulty = Difficulty_Invalid;
	m_LastCourseDifficulty = Difficulty_Invalid;
	m_LastStepsType = StepsType_Invalid;
	m_lastSong.Unset();
	m_lastCourse.Unset();
	m_iCurrentCombo = 0;
	m_iTotalSessions = 0;
	m_iTotalSessionSeconds = 0;
	m_iTotalGameplaySeconds = 0;
	m_fTotalCaloriesBurned = 0;
	m_GoalType = (GoalType)0;
	m_iGoalCalories = 0;
	m_iGoalSeconds = 0;
	m_iTotalDancePoints = 0;
	m_iNumExtraStagesPassed = 0;
	m_iNumExtraStagesFailed = 0;
	m_iNumToasties = 0;
	m_UnlockedEntryIDs.clear();
	m_sLastPlayedMachineGuid = "";
	m_LastPlayedDate.Init();
	m_iTotalTapsAndHolds = 0;
	m_iTotalJumps = 0;
	m_iTotalHolds = 0;
	m_iTotalRolls = 0;
	m_iTotalMines = 0;
	m_iTotalHands = 0;
	m_iTotalLifts = 0;
	m_fPlayerRating = 0.f;

	FOREACH_ENUM( PlayMode, i )
		m_iNumSongsPlayedByPlayMode[i] = 0;
	m_iNumSongsPlayedByStyle.clear();
	FOREACH_ENUM( Difficulty, i )
		m_iNumSongsPlayedByDifficulty[i] = 0;
	for( int i=0; i<MAX_METER+1; i++ )
		m_iNumSongsPlayedByMeter[i] = 0;
	m_iNumTotalSongsPlayed = 0;
	ZERO( m_iNumStagesPassedByPlayMode );
	ZERO( m_iNumStagesPassedByGrade );
	ZERO( m_fPlayerSkillsets );

	m_UserTable.Unset();
}

void Profile::InitSongScores()
{
	m_SongHighScores.clear();
}

void Profile::InitCourseScores()
{
	m_CourseHighScores.clear();
}

void Profile::InitCategoryScores()
{
	FOREACH_ENUM( StepsType,st )
		FOREACH_ENUM( RankingCategory,rc )
			m_CategoryHighScores[st][rc].Init();
}

void Profile::InitScreenshotData()
{
	m_vScreenshots.clear();
}

void Profile::InitCalorieData()
{
	m_mapDayToCaloriesBurned.clear();
}

RString Profile::GetDisplayNameOrHighScoreName() const
{
	if( !m_sDisplayName.empty() )
		return m_sDisplayName;
	else if( !m_sLastUsedHighScoreName.empty() )
		return m_sLastUsedHighScoreName;
	else
		return RString();
}

Character *Profile::GetCharacter() const
{
	vector<Character*> vpCharacters;
	CHARMAN->GetCharacters( vpCharacters );
	FOREACH_CONST( Character*, vpCharacters, c )
	{
		if( (*c)->m_sCharacterID.CompareNoCase(m_sCharacterID)==0 )
			return *c;
	}
	return CHARMAN->GetDefaultCharacter();
}

void Profile::SetCharacter(const RString &sCharacterID)
{
	if(CHARMAN->GetCharacterFromID(sCharacterID))
		m_sCharacterID = sCharacterID;
}

static RString FormatCalories( float fCals )
{
	return Commify((int)fCals) + " Cal";
}

int Profile::GetCalculatedWeightPounds() const
{
	if( m_iWeightPounds == 0 )	// weight not entered
		return DEFAULT_WEIGHT_POUNDS;
	else 
		return m_iWeightPounds;
}

int Profile::GetAge() const
{
	if(m_BirthYear == 0)
	{
		return (GetLocalTime().tm_year+1900) - static_cast<int>(DEFAULT_BIRTH_YEAR);
	}
	return (GetLocalTime().tm_year+1900) - m_BirthYear;
}

RString Profile::GetDisplayTotalCaloriesBurned() const
{
	return FormatCalories( m_fTotalCaloriesBurned );
}

RString Profile::GetDisplayTotalCaloriesBurnedToday() const
{
	float fCals = GetCaloriesBurnedToday();
	return FormatCalories( fCals );
}

float Profile::GetCaloriesBurnedToday() const
{
	DateTime now = DateTime::GetNowDate();
	return GetCaloriesBurnedForDay(now);
}

int Profile::GetTotalNumSongsPassed() const
{
	int iTotal = 0;
	FOREACH_ENUM( PlayMode, i )
		iTotal += m_iNumStagesPassedByPlayMode[i];
	return iTotal;
}

int Profile::GetTotalStepsWithTopGrade( StepsType st, Difficulty d, Grade g ) const
{
	int iCount = 0;

	FOREACH_CONST( Song*, SONGMAN->GetAllSongs(), pSong )
	{
		if( !(*pSong)->NormallyDisplayed() )
			continue;	// skip

		FOREACH_CONST( Steps*, (*pSong)->GetAllSteps(), pSteps )
		{
			if( (*pSteps)->m_StepsType != st )
				continue;	// skip

			if( (*pSteps)->GetDifficulty() != d )
				continue;	// skip

			const HighScoreList &hsl = GetStepsHighScoreList( *pSong, *pSteps );
			if( hsl.vHighScores.empty() )
				continue;	// skip

			if( hsl.vHighScores[0].GetGrade() == g )
				iCount++;
		}
	}

	return iCount;
}

int Profile::GetTotalTrailsWithTopGrade( StepsType st, CourseDifficulty d, Grade g ) const
{
	int iCount = 0;

	// add course high scores
	vector<Course*> vCourses;
	SONGMAN->GetAllCourses( vCourses, false );
	FOREACH_CONST( Course*, vCourses, pCourse )
	{
		// Don't count any course that has any entries that change over time.
		if( !(*pCourse)->AllSongsAreFixed() )
			continue;

		vector<Trail*> vTrails;
		Trail* pTrail = (*pCourse)->GetTrail( st, d );
		if( pTrail == NULL )
			continue;

		const HighScoreList &hsl = GetCourseHighScoreList( *pCourse, pTrail );
		if( hsl.vHighScores.empty() )
			continue;	// skip

		if( hsl.vHighScores[0].GetGrade() == g )
			iCount++;
	}

	return iCount;
}

float Profile::GetSongsPossible( StepsType st, Difficulty dc ) const
{
	int iTotalSteps = 0;

	// add steps high scores
	const vector<Song*> &vSongs = SONGMAN->GetAllSongs();
	for( unsigned i=0; i<vSongs.size(); i++ )
	{
		Song* pSong = vSongs[i];
		
		if( !pSong->NormallyDisplayed() )
			continue;	// skip

		vector<Steps*> vSteps = pSong->GetAllSteps();
		for( unsigned j=0; j<vSteps.size(); j++ )
		{
			Steps* pSteps = vSteps[j];
			
			if( pSteps->m_StepsType != st )
				continue;	// skip

			if( pSteps->GetDifficulty() != dc )
				continue;	// skip

			iTotalSteps++;
		}
	}

	return (float) iTotalSteps;
}

float Profile::GetSongsActual( StepsType st, Difficulty dc ) const
{
	CHECKPOINT_M( ssprintf("Profile::GetSongsActual(%d,%d)",st,dc) );

	float fTotalPercents = 0;

	// add steps high scores
	FOREACHM_CONST( SongID, HighScoresForASong, m_SongHighScores, i )
	{
		const SongID &id = i->first;
		Song* pSong = id.ToSong();

		CHECKPOINT_M( ssprintf("Profile::GetSongsActual: %p", pSong) );

		// If the Song isn't loaded on the current machine, then we can't 
		// get radar values to compute dance points.
		if( pSong == NULL )
			continue;

		if( !pSong->NormallyDisplayed() )
			continue;	// skip

		CHECKPOINT_M( ssprintf("Profile::GetSongsActual: song %s", pSong->GetSongDir().c_str()) );
		const HighScoresForASong &hsfas = i->second;

		FOREACHM_CONST( StepsID, HighScoresForASteps, hsfas.m_StepsHighScores, j )
		{
			const StepsID &sid = j->first;
			Steps* pSteps = sid.ToSteps( pSong, true );
			CHECKPOINT_M( ssprintf("Profile::GetSongsActual: song %p, steps %p", pSong, pSteps) );

			// If the Steps isn't loaded on the current machine, then we can't 
			// get radar values to compute dance points.
			if( pSteps == NULL )
				continue;

			if( pSteps->m_StepsType != st )
				continue;

			CHECKPOINT_M( ssprintf("Profile::GetSongsActual: n %s = %p", sid.ToString().c_str(), pSteps) );
			if( pSteps->GetDifficulty() != dc )
			{
				continue;	// skip
			}
			
			CHECKPOINT_M( ssprintf("Profile::GetSongsActual: difficulty %s is correct", DifficultyToString(dc).c_str()));

			const HighScoresForASteps& h = j->second;
			const HighScoreList& hsl = h.hsl;

			fTotalPercents += hsl.GetTopScore().GetPercentDP();
		}
	}

	return fTotalPercents;
}

float Profile::GetSongsPercentComplete( StepsType st, Difficulty dc ) const
{
	return GetSongsActual(st,dc) / GetSongsPossible(st,dc);
}

static void GetHighScoreCourses( vector<Course*> &vpCoursesOut )
{
	vpCoursesOut.clear();

	vector<Course*> vpCourses;
	SONGMAN->GetAllCourses( vpCourses, false );
	FOREACH_CONST( Course*, vpCourses, c )
	{
		// Don't count any course that has any entries that change over time.
		if( !(*c)->AllSongsAreFixed() )
			continue;

		vpCoursesOut.push_back( *c );
	}
}

float Profile::GetCoursesPossible( StepsType st, CourseDifficulty cd ) const
{
	int iTotalTrails = 0;

	vector<Course*> vpCourses;
	GetHighScoreCourses( vpCourses );
	FOREACH_CONST( Course*, vpCourses, c )
	{
		Trail* pTrail = (*c)->GetTrail(st,cd);
		if( pTrail == NULL )
			continue;

		iTotalTrails++;
	}
	
	return (float) iTotalTrails;
}

float Profile::GetCoursesActual( StepsType st, CourseDifficulty cd ) const
{
	float fTotalPercents = 0;

	vector<Course*> vpCourses;
	GetHighScoreCourses( vpCourses );
	FOREACH_CONST( Course*, vpCourses, c )
	{
		Trail *pTrail = (*c)->GetTrail( st, cd );
		if( pTrail == NULL )
			continue;

		const HighScoreList& hsl = GetCourseHighScoreList( *c, pTrail );
		fTotalPercents += hsl.GetTopScore().GetPercentDP();
	}

	return fTotalPercents;
}

float Profile::GetCoursesPercentComplete( StepsType st, CourseDifficulty cd ) const
{
	return GetCoursesActual(st,cd) / GetCoursesPossible(st,cd);
}

float Profile::GetSongsAndCoursesPercentCompleteAllDifficulties( StepsType st ) const
{
	float fActual = 0;
	float fPossible = 0;
	FOREACH_ENUM( Difficulty, d )
	{
		fActual += GetSongsActual(st,d);
		fPossible += GetSongsPossible(st,d);
	}
	FOREACH_ENUM( CourseDifficulty, d )
	{
		fActual += GetCoursesActual(st,d);
		fPossible += GetCoursesPossible(st,d);
	}
	return fActual / fPossible;
}

int Profile::GetSongNumTimesPlayed( const Song* pSong ) const
{
	SongID songID;
	songID.FromSong( pSong );
	return GetSongNumTimesPlayed( songID );
}

int Profile::GetSongNumTimesPlayed( const SongID& songID ) const
{
	const HighScoresForASong *hsSong = GetHighScoresForASong( songID );
	if( hsSong == NULL )
		return 0;

	int iTotalNumTimesPlayed = 0;
	FOREACHM_CONST( StepsID, HighScoresForASteps, hsSong->m_StepsHighScores, j )
	{
		const HighScoresForASteps &hsSteps = j->second;

		iTotalNumTimesPlayed += hsSteps.hsl.GetNumTimesPlayed();
	}
	return iTotalNumTimesPlayed;
}

/*
 * Get the profile default modifiers.  Return true if set, in which case sModifiersOut
 * will be set.  Return false if no modifier string is set, in which case the theme
 * defaults should be used.  Note that the null string means "no modifiers are active",
 * which is distinct from no modifier string being set at all.
 *
 * In practice, we get the default modifiers from the theme the first time a game
 * is played, and from the profile every time thereafter.
 */
bool Profile::GetDefaultModifiers( const Game* pGameType, RString &sModifiersOut ) const
{
	map<RString,RString>::const_iterator it;
	it = m_sDefaultModifiers.find( pGameType->m_szName );
	if( it == m_sDefaultModifiers.end() )
		return false;
	sModifiersOut = it->second;
	return true;
}

void Profile::SetDefaultModifiers( const Game* pGameType, const RString &sModifiers )
{
	if( sModifiers == "" )
		m_sDefaultModifiers.erase( pGameType->m_szName );
	else
		m_sDefaultModifiers[pGameType->m_szName] = sModifiers;
}

bool Profile::IsCodeUnlocked( const RString &sUnlockEntryID ) const
{
	return m_UnlockedEntryIDs.find( sUnlockEntryID ) != m_UnlockedEntryIDs.end();
}


Song *Profile::GetMostPopularSong() const
{
	int iMaxNumTimesPlayed = 0;
	SongID id;
	FOREACHM_CONST( SongID, HighScoresForASong, m_SongHighScores, i )
	{
		int iNumTimesPlayed = i->second.GetNumTimesPlayed();
		if(i->first.ToSong() != NULL && iNumTimesPlayed > iMaxNumTimesPlayed)
		{
			id = i->first;
			iMaxNumTimesPlayed = iNumTimesPlayed;
		}
	}

	return id.ToSong();
}

Course *Profile::GetMostPopularCourse() const
{
	int iMaxNumTimesPlayed = 0;
	CourseID id;
	FOREACHM_CONST( CourseID, HighScoresForACourse, m_CourseHighScores, i )
	{
		int iNumTimesPlayed = i->second.GetNumTimesPlayed();
		if(i->first.ToCourse() != NULL && iNumTimesPlayed > iMaxNumTimesPlayed)
		{
			id = i->first;
			iMaxNumTimesPlayed = iNumTimesPlayed;
		}
	}

	return id.ToCourse();
}

// Steps high scores
void Profile::AddStepsHighScore( const Song* pSong, const Steps* pSteps, HighScore hs, int &iIndexOut )
{
	GetStepsHighScoreList(pSong,pSteps).AddHighScore( hs, iIndexOut, IsMachine() );
}

const HighScoreList& Profile::GetStepsHighScoreList( const Song* pSong, const Steps* pSteps ) const
{
	return ((Profile*)this)->GetStepsHighScoreList(pSong,pSteps);
}

HighScoreList& Profile::GetStepsHighScoreList( const Song* pSong, const Steps* pSteps )
{
	SongID songID;
	songID.FromSong( pSong );

	StepsID stepsID;
	stepsID.FromSteps( pSteps );

	HighScoresForASong &hsSong = m_SongHighScores[songID];	// operator[] inserts into map
	HighScoresForASteps &hsSteps = hsSong.m_StepsHighScores[stepsID];	// operator[] inserts into map

	return hsSteps.hsl;
}

int Profile::GetStepsNumTimesPlayed( const Song* pSong, const Steps* pSteps ) const
{
	return GetStepsHighScoreList(pSong,pSteps).GetNumTimesPlayed();
}

DateTime Profile::GetSongLastPlayedDateTime( const Song* pSong ) const
{
	SongID id;
	id.FromSong( pSong );
	std::map<SongID,HighScoresForASong>::const_iterator iter = m_SongHighScores.find( id );

	// don't call this unless has been played once
	ASSERT( iter != m_SongHighScores.end() );
	ASSERT( !iter->second.m_StepsHighScores.empty() );

	DateTime dtLatest;	// starts out zeroed
	FOREACHM_CONST( StepsID, HighScoresForASteps, iter->second.m_StepsHighScores, i )
	{
		const HighScoreList &hsl = i->second.hsl;
		if( hsl.GetNumTimesPlayed() == 0 )
			continue;
		if( dtLatest < hsl.GetLastPlayed() )
			dtLatest = hsl.GetLastPlayed();
	}
	return dtLatest;
}

bool Profile::HasPassedSteps( const Song* pSong, const Steps* pSteps ) const
{
	const HighScoreList &hsl = GetStepsHighScoreList( pSong, pSteps );
	Grade grade = hsl.GetTopScore().GetGrade();
	switch( grade )
	{
	case Grade_Failed:
	case Grade_NoData:
		return false;
	default:
		return true;
	}
}

bool Profile::HasPassedAnyStepsInSong( const Song* pSong ) const
{
	FOREACH_CONST( Steps*, pSong->GetAllSteps(), steps )
	{
		if( HasPassedSteps( pSong, *steps ) )
			return true;
	}
	return false;
}

void Profile::IncrementStepsPlayCount( const Song* pSong, const Steps* pSteps )
{
	DateTime now = DateTime::GetNowDate();
	GetStepsHighScoreList(pSong,pSteps).IncrementPlayCount( now );
}

void Profile::GetGrades( const Song* pSong, StepsType st, int iCounts[NUM_Grade] ) const
{
	SongID songID;
	songID.FromSong( pSong );

	memset( iCounts, 0, sizeof(int)*NUM_Grade );
	const HighScoresForASong *hsSong = GetHighScoresForASong( songID );
	if( hsSong == NULL )
		return;

	FOREACH_ENUM( Grade,g)
	{
		FOREACHM_CONST( StepsID, HighScoresForASteps, hsSong->m_StepsHighScores, it )
		{
			const StepsID &id = it->first;
			if( !id.MatchesStepsType(st) )
				continue;

			const HighScoresForASteps &hsSteps = it->second;
			if( hsSteps.hsl.GetTopScore().GetGrade() == g )
				iCounts[g]++;
		}
	}
}

// Course high scores
void Profile::AddCourseHighScore( const Course* pCourse, const Trail* pTrail, HighScore hs, int &iIndexOut )
{
	GetCourseHighScoreList(pCourse,pTrail).AddHighScore( hs, iIndexOut, IsMachine() );
}

const HighScoreList& Profile::GetCourseHighScoreList( const Course* pCourse, const Trail* pTrail ) const
{
	return ((Profile *)this)->GetCourseHighScoreList( pCourse, pTrail );
}

HighScoreList& Profile::GetCourseHighScoreList( const Course* pCourse, const Trail* pTrail )
{
	CourseID courseID;
	courseID.FromCourse( pCourse );

	TrailID trailID;
	trailID.FromTrail( pTrail );

	HighScoresForACourse &hsCourse = m_CourseHighScores[courseID];	// operator[] inserts into map
	HighScoresForATrail &hsTrail = hsCourse.m_TrailHighScores[trailID];	// operator[] inserts into map

	return hsTrail.hsl;
}

int Profile::GetCourseNumTimesPlayed( const Course* pCourse ) const
{
	CourseID courseID;
	courseID.FromCourse( pCourse );

	return GetCourseNumTimesPlayed( courseID );
}

int Profile::GetCourseNumTimesPlayed( const CourseID &courseID ) const
{
	const HighScoresForACourse *hsCourse = GetHighScoresForACourse( courseID );
	if( hsCourse == NULL )
		return 0;

	int iTotalNumTimesPlayed = 0;
	FOREACHM_CONST( TrailID, HighScoresForATrail, hsCourse->m_TrailHighScores, j )
	{
		const HighScoresForATrail &hsTrail = j->second;

		iTotalNumTimesPlayed += hsTrail.hsl.GetNumTimesPlayed();
	}
	return iTotalNumTimesPlayed;
}

DateTime Profile::GetCourseLastPlayedDateTime( const Course* pCourse ) const
{
	CourseID id;
	id.FromCourse( pCourse );
	std::map<CourseID,HighScoresForACourse>::const_iterator iter = m_CourseHighScores.find( id );

	// don't call this unless has been played once
	ASSERT( iter != m_CourseHighScores.end() );
	ASSERT( !iter->second.m_TrailHighScores.empty() );

	DateTime dtLatest;	// starts out zeroed
	FOREACHM_CONST( TrailID, HighScoresForATrail, iter->second.m_TrailHighScores, i )
	{
		const HighScoreList &hsl = i->second.hsl;
		if( hsl.GetNumTimesPlayed() == 0 )
			continue;
		if( dtLatest < hsl.GetLastPlayed() )
			dtLatest = hsl.GetLastPlayed();
	}
	return dtLatest;
}

void Profile::IncrementCoursePlayCount( const Course* pCourse, const Trail* pTrail )
{
	DateTime now = DateTime::GetNowDate();
	GetCourseHighScoreList(pCourse,pTrail).IncrementPlayCount( now );
}

void Profile::GetAllUsedHighScoreNames(std::set<RString>& names)
{
#define GET_NAMES_FROM_MAP(main_member, main_key_type, main_value_type, sub_member, sub_key_type, sub_value_type) \
	for(std::map<main_key_type, main_value_type>::iterator main_entry= \
				main_member.begin(); main_entry != main_member.end(); ++main_entry) \
	{ \
		for(std::map<sub_key_type, sub_value_type>::iterator sub_entry= \
					main_entry->second.sub_member.begin(); \
				sub_entry != main_entry->second.sub_member.end(); ++sub_entry) \
		{ \
			for(vector<HighScore>::iterator high_score= \
						sub_entry->second.hsl.vHighScores.begin(); \
					high_score != sub_entry->second.hsl.vHighScores.end(); \
					++high_score) \
			{ \
				if(high_score->GetName().size() > 0) \
				{ \
					names.insert(high_score->GetName()); \
				} \
			} \
		} \
	}
	GET_NAMES_FROM_MAP(m_SongHighScores, SongID, HighScoresForASong, m_StepsHighScores, StepsID, HighScoresForASteps);
	GET_NAMES_FROM_MAP(m_CourseHighScores, CourseID, HighScoresForACourse, m_TrailHighScores, TrailID, HighScoresForATrail);
#undef GET_NAMES_FROM_MAP
}

// MergeScoresFromOtherProfile has three intended use cases:
// 1.  Restoring scores to the machine profile that were deleted because the
//   songs were not loaded.
// 2.  Migrating a profile from an older version of Stepmania, and adding its
//   scores to the machine profile.
// 3.  Merging two profiles that were separate together.
// In case 1, the various total numbers are still correct, so they should be
//   skipped.  This is why the skip_totals arg exists.
// -Kyz
void Profile::MergeScoresFromOtherProfile(Profile* other, bool skip_totals,
	RString const& from_dir, RString const& to_dir)
{
	if(!skip_totals)
	{
#define MERGE_FIELD(field_name) field_name+= other->field_name;
		MERGE_FIELD(m_iTotalSessions);
		MERGE_FIELD(m_iTotalSessionSeconds);
		MERGE_FIELD(m_iTotalGameplaySeconds);
		MERGE_FIELD(m_fTotalCaloriesBurned);
		MERGE_FIELD(m_iTotalDancePoints);
		MERGE_FIELD(m_iNumExtraStagesPassed);
		MERGE_FIELD(m_iNumExtraStagesFailed);
		MERGE_FIELD(m_iNumToasties);
		MERGE_FIELD(m_iTotalTapsAndHolds);
		MERGE_FIELD(m_iTotalJumps);
		MERGE_FIELD(m_iTotalHolds);
		MERGE_FIELD(m_iTotalRolls);
		MERGE_FIELD(m_iTotalMines);
		MERGE_FIELD(m_iTotalHands);
		MERGE_FIELD(m_iTotalLifts);
		FOREACH_ENUM(PlayMode, i)
		{
			MERGE_FIELD(m_iNumSongsPlayedByPlayMode[i]);
			MERGE_FIELD(m_iNumStagesPassedByPlayMode[i]);
		}
		FOREACH_ENUM(Difficulty, i)
		{
			MERGE_FIELD(m_iNumSongsPlayedByDifficulty[i]);
		}
		for(int i= 0; i < MAX_METER; ++i)
		{
			MERGE_FIELD(m_iNumSongsPlayedByMeter[i]);
		}
		MERGE_FIELD(m_iNumTotalSongsPlayed);
		FOREACH_ENUM(Grade, i)
		{
			MERGE_FIELD(m_iNumStagesPassedByGrade[i]);
		}
#undef MERGE_FIELD
		for(map<DateTime, Calories>::iterator other_cal=
					other->m_mapDayToCaloriesBurned.begin();
				other_cal != other->m_mapDayToCaloriesBurned.end(); ++other_cal)
		{
			map<DateTime, Calories>::iterator this_cal=
				m_mapDayToCaloriesBurned.find(other_cal->first);
			if(this_cal == m_mapDayToCaloriesBurned.end())
			{
				m_mapDayToCaloriesBurned[other_cal->first]= other_cal->second;
			}
			else
			{
				this_cal->second.fCals+= other_cal->second.fCals;
			}
		}
	}
#define MERGE_SCORES_IN_MEMBER(main_member, main_key_type, main_value_type, sub_member, sub_key_type, sub_value_type) \
	for(std::map<main_key_type, main_value_type>::iterator main_entry= \
				other->main_member.begin(); main_entry != other->main_member.end(); \
			++main_entry) \
	{ \
		std::map<main_key_type, main_value_type>::iterator this_entry= \
			main_member.find(main_entry->first); \
		if(this_entry == main_member.end()) \
		{ \
			main_member[main_entry->first]= main_entry->second; \
		} \
		else \
		{ \
			for(std::map<sub_key_type, sub_value_type>::iterator sub_entry= \
						main_entry->second.sub_member.begin(); \
					sub_entry != main_entry->second.sub_member.end(); ++sub_entry) \
			{ \
				std::map<sub_key_type, sub_value_type>::iterator this_sub= \
					this_entry->second.sub_member.find(sub_entry->first); \
				if(this_sub == this_entry->second.sub_member.end()) \
				{ \
					this_entry->second.sub_member[sub_entry->first]= sub_entry->second; \
				} \
				else \
				{ \
					this_sub->second.hsl.MergeFromOtherHSL(sub_entry->second.hsl, IsMachine()); \
				} \
			} \
		} \
	}
	MERGE_SCORES_IN_MEMBER(m_SongHighScores, SongID, HighScoresForASong, m_StepsHighScores, StepsID, HighScoresForASteps);
	MERGE_SCORES_IN_MEMBER(m_CourseHighScores, CourseID, HighScoresForACourse, m_TrailHighScores, TrailID, HighScoresForATrail);
#undef MERGE_SCORES_IN_MEMBER
	// I think the machine profile should not have screenshots merged into it
	// because the intended use case is someone whose profile scores were
	// deleted off the machine by mishap, or a profile being migrated from an
	// older version of Stepmania.  Either way, the screenshots should stay
	// with the profile they came from.
	// In the case where two local profiles are being merged together, the user
	// is probably planning to delete the old profile after the merge, so we
	// want to copy the screenshots over. -Kyz
	if(!IsMachine())
	{
		// The old screenshot count is stored so we know where to start in the
		// list when copying the screenshot images.
		size_t old_count= m_vScreenshots.size();
		m_vScreenshots.insert(m_vScreenshots.end(),
			other->m_vScreenshots.begin(), other->m_vScreenshots.end());
		for(size_t sid= old_count; sid < m_vScreenshots.size(); ++sid)
		{
			RString old_path= from_dir + "Screenshots/" + m_vScreenshots[sid].sFileName;
			RString new_path= to_dir + "Screenshots/" + m_vScreenshots[sid].sFileName;
			// Only move the old screenshot over if it exists and won't stomp an
			// existing screenshot.
			if(FILEMAN->DoesFileExist(old_path) && (!FILEMAN->DoesFileExist(new_path)))
			{
				FILEMAN->Move(old_path, new_path);
			}
		}
		// The screenshots are kept sorted by date for ease of use, and
		// duplicates are removed because they come from the user mistakenly
		// merging a second time. -Kyz
		std::sort(m_vScreenshots.begin(), m_vScreenshots.end());
		vector<Screenshot>::iterator unique_end=
			std::unique(m_vScreenshots.begin(), m_vScreenshots.end());
		m_vScreenshots.erase(unique_end, m_vScreenshots.end());
	}
}

void Profile::swap(Profile& other)
{
	// Type is skipped because this is meant to be used only on matching types,
	// to move profiles after the priorities have been assigned. -Kyz
	// A bit of a misnomer, since it actually works on any type that has its
	// own swap function, which includes the standard containers.
#define SWAP_STR_MEMBER(member_name) member_name.swap(other.member_name)
#define SWAP_GENERAL(member_name) std::swap(member_name, other.member_name)
#define SWAP_ARRAY(member_name, size) \
	for(int i= 0; i < size; ++i) { \
		std::swap(member_name[i], other.member_name[i]); } \
	SWAP_GENERAL(m_ListPriority);
	SWAP_STR_MEMBER(m_sDisplayName);
	SWAP_STR_MEMBER(m_sCharacterID);
	SWAP_STR_MEMBER(m_sLastUsedHighScoreName);
	SWAP_GENERAL(m_iWeightPounds);
	SWAP_GENERAL(m_Voomax);
	SWAP_GENERAL(m_BirthYear);
	SWAP_GENERAL(m_IgnoreStepCountCalories);
	SWAP_GENERAL(m_IsMale);
	SWAP_STR_MEMBER(m_sGuid);
	SWAP_GENERAL(m_iCurrentCombo);
	SWAP_GENERAL(m_iTotalSessions);
	SWAP_GENERAL(m_iTotalSessionSeconds);
	SWAP_GENERAL(m_iTotalGameplaySeconds);
	SWAP_GENERAL(m_fTotalCaloriesBurned);
	SWAP_GENERAL(m_GoalType);
	SWAP_GENERAL(m_iGoalCalories);
	SWAP_GENERAL(m_iGoalSeconds);
	SWAP_GENERAL(m_iTotalDancePoints);
	SWAP_GENERAL(m_iNumExtraStagesPassed);
	SWAP_GENERAL(m_iNumExtraStagesFailed);
	SWAP_GENERAL(m_iNumToasties);
	SWAP_GENERAL(m_iTotalTapsAndHolds);
	SWAP_GENERAL(m_iTotalJumps);
	SWAP_GENERAL(m_iTotalHolds);
	SWAP_GENERAL(m_iTotalRolls);
	SWAP_GENERAL(m_iTotalMines);
	SWAP_GENERAL(m_iTotalHands);
	SWAP_GENERAL(m_iTotalLifts);
	SWAP_GENERAL(m_bNewProfile);
	SWAP_STR_MEMBER(m_UnlockedEntryIDs);
	SWAP_STR_MEMBER(m_sLastPlayedMachineGuid);
	SWAP_GENERAL(m_LastPlayedDate);
	SWAP_ARRAY(m_iNumSongsPlayedByPlayMode, NUM_PlayMode);
	SWAP_STR_MEMBER(m_iNumSongsPlayedByStyle);
	SWAP_ARRAY(m_iNumSongsPlayedByDifficulty, NUM_Difficulty);
	SWAP_ARRAY(m_iNumSongsPlayedByMeter, MAX_METER+1);
	SWAP_GENERAL(m_iNumTotalSongsPlayed);
	SWAP_ARRAY(m_iNumStagesPassedByPlayMode, NUM_PlayMode);
	SWAP_ARRAY(m_iNumStagesPassedByGrade, NUM_Grade);
	SWAP_GENERAL(m_UserTable);
	SWAP_STR_MEMBER(m_SongHighScores);
	SWAP_STR_MEMBER(m_CourseHighScores);
	for(int st= 0; st < NUM_StepsType; ++st)
	{
		SWAP_ARRAY(m_CategoryHighScores[st], NUM_RankingCategory);
	}
	SWAP_STR_MEMBER(m_vScreenshots);
	SWAP_STR_MEMBER(m_mapDayToCaloriesBurned);
#undef SWAP_STR_MEMBER
#undef SWAP_GENERAL
#undef SWAP_ARRAY
}

// Category high scores
void Profile::AddCategoryHighScore( StepsType st, RankingCategory rc, HighScore hs, int &iIndexOut )
{
	m_CategoryHighScores[st][rc].AddHighScore( hs, iIndexOut, IsMachine() );
}

const HighScoreList& Profile::GetCategoryHighScoreList( StepsType st, RankingCategory rc ) const
{
	return ((Profile *)this)->m_CategoryHighScores[st][rc];
}

HighScoreList& Profile::GetCategoryHighScoreList( StepsType st, RankingCategory rc )
{
	return m_CategoryHighScores[st][rc];
}

int Profile::GetCategoryNumTimesPlayed( StepsType st ) const
{
	int iNumTimesPlayed = 0;
	FOREACH_ENUM( RankingCategory,rc )
		iNumTimesPlayed += m_CategoryHighScores[st][rc].GetNumTimesPlayed();
	return iNumTimesPlayed;
}

void Profile::IncrementCategoryPlayCount( StepsType st, RankingCategory rc )
{
	DateTime now = DateTime::GetNowDate();
	m_CategoryHighScores[st][rc].IncrementPlayCount( now );
}


// Loading and saving
#define WARN_PARSER	ShowWarningOrTrace( __FILE__, __LINE__, "Error parsing file.", true )
#define WARN_AND_RETURN { WARN_PARSER; return; }
#define WARN_AND_CONTINUE { WARN_PARSER; continue; }
#define WARN_AND_BREAK { WARN_PARSER; break; }
#define WARN_M(m)	ShowWarningOrTrace( __FILE__, __LINE__, RString("Error parsing file: ")+(m), true )
#define WARN_AND_RETURN_M(m) { WARN_M(m); return; }
#define WARN_AND_CONTINUE_M(m) { WARN_M(m); continue; }
#define WARN_AND_BREAK_M(m) { WARN_M(m); break; }
#define LOAD_NODE(X)	{ \
	const XNode* X = xml->GetChild(#X); \
	if( X==NULL ) LOG->Warn("Failed to read section " #X); \
	else Load##X##FromNode(X); }

void Profile::LoadCustomFunction( const RString &sDir )
{
	/* Get the theme's custom load function:
	 *   [Profile]
	 *   CustomLoadFunction=function(profile, profileDir) ... end
	 */
	Lua *L = LUA->Get();
	LuaReference customLoadFunc = THEME->GetMetricR("Profile", "CustomLoadFunction");
	customLoadFunc.PushSelf(L);
	ASSERT_M(!lua_isnil(L, -1), "CustomLoadFunction not defined");

	// Pass profile and profile directory as arguments
	this->PushSelf(L);
	LuaHelpers::Push(L, sDir);

	// Run it
	RString Error= "Error running CustomLoadFunction: ";
	LuaHelpers::RunScriptOnStack(L, Error, 2, 0, true);

	LUA->Release(L);
}

void Profile::HandleStatsPrefixChange(RString dir, bool require_signature)
{
	// Temp variables to preserve stuff across the reload.
	// Some stuff intentionally left out because the original reason for the
	// stats prefix was to allow scores from different game types to coexist.
	RString display_name= m_sDisplayName;
	RString character_id= m_sCharacterID;
	RString last_high_score_name= m_sLastUsedHighScoreName;
	int weight= m_iWeightPounds;
	float voomax= m_Voomax;
	int birth_year= m_BirthYear;
	bool ignore_step_cal= m_IgnoreStepCountCalories;
	bool male= m_IsMale;
	ProfileType type= m_Type;
	int priority= m_ListPriority;
	RString guid= m_sGuid;
	map<RString, RString> default_mods= m_sDefaultModifiers;
	SortOrder sort_order= m_SortOrder;
	Difficulty last_diff= m_LastDifficulty;
	CourseDifficulty last_course_diff= m_LastCourseDifficulty;
	StepsType last_stepstype= m_LastStepsType;
	SongID last_song= m_lastSong;
	CourseID last_course= m_lastCourse;
	int total_sessions= m_iTotalSessions;
	int total_session_seconds= m_iTotalSessionSeconds;
	int total_gameplay_seconds= m_iTotalGameplaySeconds;
	float total_calories_burned= m_fTotalCaloriesBurned;
	LuaTable user_table= m_UserTable;
	bool need_to_create_file= false;
	if(IsAFile(dir + PROFILEMAN->GetStatsPrefix() + STATS_XML))
	{
		LoadAllFromDir(dir, require_signature);
	}
	else
	{
		ClearStats();
		need_to_create_file= true;
	}
	m_sDisplayName= display_name;
	m_sCharacterID= character_id;
	m_sLastUsedHighScoreName= last_high_score_name;
	m_iWeightPounds= weight;
	m_Voomax= voomax;
	m_BirthYear= birth_year;
	m_IgnoreStepCountCalories= ignore_step_cal;
	m_IsMale= male;
	m_Type= type;
	m_ListPriority= priority;
	m_sGuid= guid;
	m_sDefaultModifiers= default_mods;
	m_SortOrder= sort_order;
	m_LastDifficulty= last_diff;
	m_LastCourseDifficulty= last_course_diff;
	m_LastStepsType= last_stepstype;
	m_lastSong= last_song;
	m_iTotalSessions= total_sessions;
	m_iTotalSessionSeconds= total_session_seconds;
	m_iTotalGameplaySeconds= total_gameplay_seconds;
	m_fTotalCaloriesBurned= total_calories_burned;
	m_UserTable= user_table;
	if(need_to_create_file)
	{
		SaveAllToDir(dir, require_signature);
	}
}

ProfileLoadResult Profile::LoadAllFromDir( const RString &sDir, bool bRequireSignature )
{
	FILEMAN->CreateDir(sDir + REPLAY_SUBDIR);
	LOG->Trace( "Profile::LoadAllFromDir( %s )", sDir.c_str() );

	ASSERT( sDir.Right(1) == "/" );

	InitAll();

	LoadTypeFromDir(sDir);
	// Not critical if this fails
	LoadEditableDataFromDir( sDir );

	ProfileLoadResult ret= LoadEttFromDir(sDir);
	if (ret != ProfileLoadResult_Success)
		ret = LoadStatsFromDir(sDir, bRequireSignature);
	if (ret != ProfileLoadResult_Success)
		return ret;

	LoadCustomFunction( sDir );

	return ProfileLoadResult_Success;
}

ProfileLoadResult Profile::LoadStatsFromDir(RString dir, bool require_signature)
{
	dir += PROFILEMAN->GetStatsPrefix();
	profiledir = dir;
	// Check for the existance of stats.xml
	RString fn = dir + STATS_XML;
	bool compressed = false;
	if(!IsAFile(fn))
	{
		// Check for the existance of stats.xml.gz
		fn = dir + STATS_XML_GZ;
		compressed = true;
		if(!IsAFile(fn))
		{
			return ProfileLoadResult_FailedNoProfile;
		}
	}

	int iError;
	unique_ptr<RageFileBasic> pFile( FILEMAN->Open(fn, RageFile::READ, iError) );
	if(pFile.get() == NULL)
	{
		LOG->Trace("Error opening %s: %s", fn.c_str(), strerror(iError));
		return ProfileLoadResult_FailedTampered;
	}

	if(compressed)
	{
		RString sError;
		uint32_t iCRC32;
		RageFileObjInflate *pInflate = GunzipFile(pFile.release(), sError, &iCRC32);
		if(pInflate == NULL)
		{
			LOG->Trace("Error opening %s: %s", fn.c_str(), sError.c_str());
			return ProfileLoadResult_FailedTampered;
		}

		pFile.reset(pInflate);
	}

	// Don't load unreasonably large stats.xml files.
	if(!IsMachine())	// only check stats coming from the player
	{
		int iBytes = pFile->GetFileSize();
		if(iBytes > 5 * MAX_PLAYER_STATS_XML_SIZE_BYTES)
		{
			LuaHelpers::ReportScriptErrorFmt("The file '%s' is unreasonably large.  It won't be loaded.", fn.c_str());
			return ProfileLoadResult_FailedTampered;
		}
	}

	if(require_signature)
	{ 
		RString sStatsXmlSigFile = fn+SIGNATURE_APPEND;
		RString sDontShareFile = dir + DONT_SHARE_SIG;

		LOG->Trace("Verifying don't share signature \"%s\" against \"%s\"", sDontShareFile.c_str(), sStatsXmlSigFile.c_str());
		// verify the stats.xml signature with the "don't share" file
		if(!CryptManager::VerifyFileWithFile(sStatsXmlSigFile, sDontShareFile))
		{
			LuaHelpers::ReportScriptErrorFmt("The don't share check for '%s' failed.  Data will be ignored.", sStatsXmlSigFile.c_str());
			return ProfileLoadResult_FailedTampered;
		}
		LOG->Trace("Done.");

		// verify stats.xml
		LOG->Trace("Verifying stats.xml signature");
		if(!CryptManager::VerifyFileWithFile(fn, sStatsXmlSigFile))
		{
			LuaHelpers::ReportScriptErrorFmt("The signature check for '%s' failed.  Data will be ignored.", fn.c_str());
			return ProfileLoadResult_FailedTampered;
		}
		LOG->Trace("Done.");
	}

	LOG->Trace("Loading %s", fn.c_str());
	XNode xml;
	if(!XmlFileUtil::LoadFromFileShowErrors(xml, *pFile.get()))
		return ProfileLoadResult_FailedTampered;
	LOG->Trace("Done.");

	return LoadStatsXmlFromNode(&xml);
}


ProfileLoadResult Profile::LoadEttFromDir(RString dir) {
	dir += PROFILEMAN->GetStatsPrefix();
	profiledir = dir;
	RString fn = dir + ETT_XML;
	bool compressed = false;
	if (!IsAFile(fn)) {
		fn = dir + STATS_XML_GZ;
		compressed = true;
		if (!IsAFile(fn))
			return ProfileLoadResult_FailedNoProfile;
	}

	int iError;
	unique_ptr<RageFileBasic> pFile(FILEMAN->Open(fn, RageFile::READ, iError));
	if (pFile.get() == NULL) {
		LOG->Trace("Error opening %s: %s", fn.c_str(), strerror(iError));
		return ProfileLoadResult_FailedTampered;
	}

	if (compressed) {
		RString sError;
		uint32_t iCRC32;
		RageFileObjInflate *pInflate = GunzipFile(pFile.release(), sError, &iCRC32);
		if (pInflate == NULL) {
			LOG->Trace("Error opening %s: %s", fn.c_str(), sError.c_str());
			return ProfileLoadResult_FailedTampered;
		}
		pFile.reset(pInflate);
	}

	LOG->Trace("Loading %s", fn.c_str());
	XNode xml;
	if (!XmlFileUtil::LoadFromFileShowErrors(xml, *pFile.get()))
		return ProfileLoadResult_FailedTampered;
	LOG->Trace("Done.");

	return LoadEttXmlFromNode(&xml);
}


void Profile::LoadTypeFromDir(const RString &dir)
{
	m_Type= ProfileType_Normal;
	m_ListPriority= 0;
	RString fn= dir + TYPE_INI;
	if(FILEMAN->DoesFileExist(fn))
	{
		IniFile ini;
		if(ini.ReadFile(fn))
		{
			XNode const* data= ini.GetChild("ListPosition");
			if(data != NULL)
			{
				RString type_str;
				if(data->GetAttrValue("Type", type_str))
				{
					m_Type= StringToProfileType(type_str);
					if(m_Type >= NUM_ProfileType)
					{
						m_Type= ProfileType_Normal;
					}
				}
				data->GetAttrValue("Priority", m_ListPriority);
			}
		}
	}
}

ProfileLoadResult Profile::LoadStatsXmlFromNode( const XNode *xml, bool bIgnoreEditable )
{
	/* The placeholder stats.xml file has an <html> tag. Don't load it,
	 * but don't warn about it. */
	if( xml->GetName() == "html" )
		return ProfileLoadResult_FailedNoProfile;

	if( xml->GetName() != "Stats" )
	{
		WARN_M( xml->GetName() );
		return ProfileLoadResult_FailedTampered;
	}

	// These are loaded from Editable, so we usually want to ignore them here.
	RString sName = m_sDisplayName;
	RString sCharacterID = m_sCharacterID;
	RString sLastUsedHighScoreName = m_sLastUsedHighScoreName;
	int iWeightPounds = m_iWeightPounds;
	float Voomax= m_Voomax;
	int BirthYear= m_BirthYear;
	bool IgnoreStepCountCalories= m_IgnoreStepCountCalories;
	bool IsMale= m_IsMale;

	LOAD_NODE( GeneralData );
	LOAD_NODE( SongScores );
	LOAD_NODE( CourseScores );
	LOAD_NODE( CategoryScores );
	LOAD_NODE( ScreenshotData );
	LOAD_NODE( CalorieData );

	if( bIgnoreEditable )
	{
		m_sDisplayName = sName;
		m_sCharacterID = sCharacterID;
		m_sLastUsedHighScoreName = sLastUsedHighScoreName;
		m_iWeightPounds = iWeightPounds;
		m_Voomax= Voomax;
		m_BirthYear= BirthYear;
		m_IgnoreStepCountCalories= IgnoreStepCountCalories;
		m_IsMale= IsMale;
	}

	return ProfileLoadResult_Success;
}

ProfileLoadResult Profile::LoadEttXmlFromNode(const XNode *xml) {
	/* The placeholder stats.xml file has an <html> tag. Don't load it,
	* but don't warn about it. */
	if (xml->GetName() == "html")
		return ProfileLoadResult_FailedNoProfile;

	if (xml->GetName() != "Stats")
	{
		WARN_M(xml->GetName());
		return ProfileLoadResult_FailedTampered;
	}


	LOAD_NODE(GeneralData);

	const XNode* scores = xml->GetChild("PlayerScores");
	LoadEttScoresFromNode(scores);

	return ProfileLoadResult_Success;
}

bool Profile::SaveAllToDir( const RString &sDir, bool bSignData ) const
{
	m_sLastPlayedMachineGuid = PROFILEMAN->GetMachineProfile()->m_sGuid;
	m_LastPlayedDate = DateTime::GetNowDate();

	SaveTypeToDir(sDir);
	// Save editable.ini
	SaveEditableDataToDir( sDir );

	bool bSaved = SaveEttXmlToDir(sDir);

	SaveStatsWebPageToDir( sDir );

	// Empty directories if none exist.
	if( ProfileManager::m_bProfileStepEdits )
		FILEMAN->CreateDir( sDir + EDIT_STEPS_SUBDIR );
	if( ProfileManager::m_bProfileCourseEdits )
		FILEMAN->CreateDir( sDir + EDIT_COURSES_SUBDIR );
	FILEMAN->CreateDir( sDir + SCREENSHOTS_SUBDIR );
	FILEMAN->CreateDir( sDir + RIVAL_SUBDIR );
	FILEMAN->CreateDir( sDir + REPLAY_SUBDIR);

	/* Get the theme's custom save function:
	 *   [Profile]
	 *   CustomSaveFunction=function(profile, profileDir) ... end
	 */
	Lua *L = LUA->Get();
	LuaReference customSaveFunc = THEME->GetMetricR("Profile", "CustomSaveFunction");
	customSaveFunc.PushSelf(L);
	ASSERT_M(!lua_isnil(L, -1), "CustomSaveFunction not defined");

	// Pass profile and profile directory as arguments
	const_cast<Profile *>(this)->PushSelf(L);
	LuaHelpers::Push(L, sDir);

	// Run it
	RString Error= "Error running CustomSaveFunction: ";
	LuaHelpers::RunScriptOnStack(L, Error, 2, 0, true);

	LUA->Release(L);

	return bSaved;
}

XNode *Profile::SaveStatsXmlCreateNode() const
{
	XNode *xml = new XNode( "Stats" );

	xml->AppendChild( SaveGeneralDataCreateNode() );
	xml->AppendChild( SaveSongScoresCreateNode() );
	xml->AppendChild( SaveCourseScoresCreateNode() );
	xml->AppendChild( SaveCategoryScoresCreateNode() );
	xml->AppendChild( SaveScreenshotDataCreateNode() );
	xml->AppendChild( SaveCalorieDataCreateNode() );
	if( SHOW_COIN_DATA.GetValue() && IsMachine() )
		xml->AppendChild( SaveCoinDataCreateNode() );

	return xml;
}

XNode *Profile::SaveEttXmlCreateNode() const
{
	XNode *xml = new XNode("Stats");

	xml->AppendChild(SaveGeneralDataCreateNode());
	xml->AppendChild(SaveEttScoresCreateNode());

	return xml;
}

bool Profile::SaveStatsXmlToDir( RString sDir, bool bSignData ) const
{
	LOG->Trace( "SaveStatsXmlToDir: %s", sDir.c_str() );
	unique_ptr<XNode> xml( SaveStatsXmlCreateNode() );

	sDir += PROFILEMAN->GetStatsPrefix();
	// Save stats.xml
	RString fn = sDir + (g_bProfileDataCompress? STATS_XML_GZ:STATS_XML);

	{
		RString sError;
		RageFile f;
		if( !f.Open(fn, RageFile::WRITE) )
		{
			LuaHelpers::ReportScriptErrorFmt( "Couldn't open %s for writing: %s", fn.c_str(), f.GetError().c_str() );
			return false;
		}

		if( g_bProfileDataCompress )
		{
			RageFileObjGzip gzip( &f );
			gzip.Start();
			if( !XmlFileUtil::SaveToFile( xml.get(), gzip, "", false ) )
				return false;

			if( gzip.Finish() == -1 )
				return false;

			/* After successfully saving STATS_XML_GZ, remove any stray STATS_XML. */
			if( FILEMAN->IsAFile(sDir + STATS_XML) )
				FILEMAN->Remove( sDir + STATS_XML );
		}
		else
		{
			if( !XmlFileUtil::SaveToFile( xml.get(), f, "", false ) )
				return false;

			/* After successfully saving STATS_XML, remove any stray STATS_XML_GZ. */
			if( FILEMAN->IsAFile(sDir + STATS_XML_GZ) )
				FILEMAN->Remove( sDir + STATS_XML_GZ );
		}
	}

	if( bSignData )
	{
		RString sStatsXmlSigFile = fn+SIGNATURE_APPEND;
		CryptManager::SignFileToFile(fn, sStatsXmlSigFile);

		// Save the "don't share" file
		RString sDontShareFile = sDir + DONT_SHARE_SIG;
		CryptManager::SignFileToFile(sStatsXmlSigFile, sDontShareFile);
	}

	return true;
}

bool Profile::SaveEttXmlToDir(RString sDir) const {
	LOG->Trace("SaveStatsXmlToDir: %s", sDir.c_str());
	unique_ptr<XNode> xml(SaveEttXmlCreateNode());

	sDir += PROFILEMAN->GetStatsPrefix();
	// Save Etterna.xml
	RString fn = sDir + ETT_XML;

	{
		RString sError;
		RageFile f;
		if (!f.Open(fn, RageFile::WRITE))
		{
			LuaHelpers::ReportScriptErrorFmt("Couldn't open %s for writing: %s", fn.c_str(), f.GetError().c_str());
			return false;
		}

		if (g_bProfileDataCompress)
		{
			RageFileObjGzip gzip(&f);
			gzip.Start();
			if (!XmlFileUtil::SaveToFile(xml.get(), gzip, "", false))
				return false;

			if (gzip.Finish() == -1)
				return false;

			/* After successfully saving STATS_XML_GZ, remove any stray STATS_XML. */
			if (FILEMAN->IsAFile(sDir + STATS_XML))
				FILEMAN->Remove(sDir + STATS_XML);
		}
		else
		{
			if (!XmlFileUtil::SaveToFile(xml.get(), f, "", false))
				return false;

			/* After successfully saving STATS_XML, remove any stray STATS_XML_GZ. */
			if (FILEMAN->IsAFile(sDir + STATS_XML_GZ))
				FILEMAN->Remove(sDir + STATS_XML_GZ);
		}
	}

	return true;
}

void Profile::SaveTypeToDir(const RString &dir) const
{
	IniFile ini;
	ini.SetValue("ListPosition", "Type", ProfileTypeToString(m_Type));
	ini.SetValue("ListPosition", "Priority", m_ListPriority);
	ini.WriteFile(dir + TYPE_INI);
}

void Profile::SaveEditableDataToDir( const RString &sDir ) const
{
	IniFile ini;

	ini.SetValue( "Editable", "DisplayName",			m_sDisplayName );
	ini.SetValue( "Editable", "CharacterID",			m_sCharacterID );
	ini.SetValue( "Editable", "LastUsedHighScoreName",		m_sLastUsedHighScoreName );
	ini.SetValue( "Editable", "WeightPounds",			m_iWeightPounds );
	ini.SetValue( "Editable", "Voomax", m_Voomax );
	ini.SetValue( "Editable", "BirthYear", m_BirthYear );
	ini.SetValue( "Editable", "IgnoreStepCountCalories", m_IgnoreStepCountCalories );
	ini.SetValue( "Editable", "IsMale", m_IsMale );

	ini.WriteFile( sDir + EDITABLE_INI );
}

XNode* Profile::SaveGeneralDataCreateNode() const
{
	XNode* pGeneralDataNode = new XNode( "GeneralData" );

	// TRICKY: These are write-only elements that are normally never read again.
	// This data is required by other apps (like internet ranking), but is 
	// redundant to the game app.
	pGeneralDataNode->AppendChild( "DisplayName",			GetDisplayNameOrHighScoreName() );
	pGeneralDataNode->AppendChild( "CharacterID",			m_sCharacterID );
	pGeneralDataNode->AppendChild( "LastUsedHighScoreName",		m_sLastUsedHighScoreName );
	pGeneralDataNode->AppendChild( "WeightPounds",			m_iWeightPounds );
	pGeneralDataNode->AppendChild( "Voomax", m_Voomax );
	pGeneralDataNode->AppendChild( "BirthYear", m_BirthYear );
	pGeneralDataNode->AppendChild( "IgnoreStepCountCalories", m_IgnoreStepCountCalories );
	pGeneralDataNode->AppendChild( "IsMale", m_IsMale );

	pGeneralDataNode->AppendChild( "IsMachine",			IsMachine() );

	pGeneralDataNode->AppendChild( "Guid",				m_sGuid );
	pGeneralDataNode->AppendChild( "SortOrder",			SortOrderToString(m_SortOrder) );
	pGeneralDataNode->AppendChild( "LastDifficulty",		DifficultyToString(m_LastDifficulty) );
	pGeneralDataNode->AppendChild( "LastCourseDifficulty",		DifficultyToString(m_LastCourseDifficulty) );
	if( m_LastStepsType != StepsType_Invalid )
		pGeneralDataNode->AppendChild( "LastStepsType",			GAMEMAN->GetStepsTypeInfo(m_LastStepsType).szName );
	pGeneralDataNode->AppendChild( m_lastSong.CreateNode() );
	pGeneralDataNode->AppendChild( m_lastCourse.CreateNode() );
	pGeneralDataNode->AppendChild( "CurrentCombo", m_iCurrentCombo );
	pGeneralDataNode->AppendChild( "TotalSessions",			m_iTotalSessions );
	pGeneralDataNode->AppendChild( "TotalSessionSeconds",		m_iTotalSessionSeconds );
	pGeneralDataNode->AppendChild( "TotalGameplaySeconds",		m_iTotalGameplaySeconds );
	pGeneralDataNode->AppendChild( "TotalCaloriesBurned",		m_fTotalCaloriesBurned );
	pGeneralDataNode->AppendChild( "GoalType",			m_GoalType );
	pGeneralDataNode->AppendChild( "GoalCalories",			m_iGoalCalories );
	pGeneralDataNode->AppendChild( "GoalSeconds",			m_iGoalSeconds );
	pGeneralDataNode->AppendChild( "LastPlayedMachineGuid",		m_sLastPlayedMachineGuid );
	pGeneralDataNode->AppendChild( "LastPlayedDate",		m_LastPlayedDate.GetString() );
	pGeneralDataNode->AppendChild( "TotalDancePoints",		m_iTotalDancePoints );
	pGeneralDataNode->AppendChild( "NumExtraStagesPassed",		m_iNumExtraStagesPassed );
	pGeneralDataNode->AppendChild( "NumExtraStagesFailed",		m_iNumExtraStagesFailed );
	pGeneralDataNode->AppendChild( "NumToasties",			m_iNumToasties );
	pGeneralDataNode->AppendChild( "TotalTapsAndHolds",		m_iTotalTapsAndHolds );
	pGeneralDataNode->AppendChild( "TotalJumps",			m_iTotalJumps );
	pGeneralDataNode->AppendChild( "TotalHolds",			m_iTotalHolds );
	pGeneralDataNode->AppendChild( "TotalRolls",			m_iTotalRolls );
	pGeneralDataNode->AppendChild( "TotalMines",			m_iTotalMines );
	pGeneralDataNode->AppendChild( "TotalHands",			m_iTotalHands );
	pGeneralDataNode->AppendChild( "TotalLifts",			m_iTotalLifts );
	pGeneralDataNode->AppendChild( "PlayerRating",			m_fPlayerRating);

	// Keep declared variables in a very local scope so they aren't 
	// accidentally used where they're not intended.  There's a lot of
	// copying and pasting in this code.

	{
		XNode* pDefaultModifiers = pGeneralDataNode->AppendChild("DefaultModifiers");
		FOREACHM_CONST( RString, RString, m_sDefaultModifiers, it )
			pDefaultModifiers->AppendChild( it->first, it->second );
	}

	{
		XNode* pFavorites = pGeneralDataNode->AppendChild("Favorites");
		FOREACH_CONST(RString, FavoritedCharts, it)
			pFavorites->AppendChild(*it);			
	}

	{
		XNode* pPlayerSkillsets = pGeneralDataNode->AppendChild("PlayerSkillsets");
		FOREACH_ENUM(Skillset, ss)
			pPlayerSkillsets->AppendChild(SkillsetToString(ss), m_fPlayerSkillsets[ss]);
	}

	{
		XNode* pUnlocks = pGeneralDataNode->AppendChild("Unlocks");
		FOREACHS_CONST( RString, m_UnlockedEntryIDs, it )
		{
			XNode *pEntry = pUnlocks->AppendChild("UnlockEntry");
			RString sUnlockEntry = it->c_str();
			pEntry->AppendAttr( "UnlockEntryID", sUnlockEntry );
			if( !UNLOCK_AUTH_STRING.GetValue().empty() )
			{
				RString sUnlockAuth = BinaryToHex( CRYPTMAN->GetMD5ForString(sUnlockEntry + UNLOCK_AUTH_STRING.GetValue()) );
				pEntry->AppendAttr( "Auth", sUnlockAuth );
			}
		}
	}

	{
		XNode* pNumSongsPlayedByPlayMode = pGeneralDataNode->AppendChild("NumSongsPlayedByPlayMode");
		FOREACH_ENUM( PlayMode, pm )
		{
			// Don't save unplayed PlayModes.
			if( !m_iNumSongsPlayedByPlayMode[pm] )
				continue;
			pNumSongsPlayedByPlayMode->AppendChild( PlayModeToString(pm), m_iNumSongsPlayedByPlayMode[pm] );
		}
	}

	{
		XNode* pNumSongsPlayedByStyle = pGeneralDataNode->AppendChild("NumSongsPlayedByStyle");
		FOREACHM_CONST( StyleID, int, m_iNumSongsPlayedByStyle, iter )
		{
			const StyleID &s = iter->first;
			int iNumPlays = iter->second;

			XNode *pStyleNode = s.CreateNode();
			pStyleNode->AppendAttr(XNode::TEXT_ATTRIBUTE, iNumPlays );

			pNumSongsPlayedByStyle->AppendChild( pStyleNode );
		}
	}

	{
		XNode* pNumSongsPlayedByDifficulty = pGeneralDataNode->AppendChild("NumSongsPlayedByDifficulty");
		FOREACH_ENUM( Difficulty, dc )
		{
			if( !m_iNumSongsPlayedByDifficulty[dc] )
				continue;
			pNumSongsPlayedByDifficulty->AppendChild( DifficultyToString(dc), m_iNumSongsPlayedByDifficulty[dc] );
		}
	}

	{
		XNode* pNumSongsPlayedByMeter = pGeneralDataNode->AppendChild("NumSongsPlayedByMeter");
		for( int i=0; i<MAX_METER+1; i++ )
		{
			if( !m_iNumSongsPlayedByMeter[i] )
				continue;
			pNumSongsPlayedByMeter->AppendChild( ssprintf("Meter%d",i), m_iNumSongsPlayedByMeter[i] );
		}
	}

	pGeneralDataNode->AppendChild( "NumTotalSongsPlayed", m_iNumTotalSongsPlayed );

	{
		XNode* pNumStagesPassedByPlayMode = pGeneralDataNode->AppendChild("NumStagesPassedByPlayMode");
		FOREACH_ENUM( PlayMode, pm )
		{
			// Don't save unplayed PlayModes.
			if( !m_iNumStagesPassedByPlayMode[pm] )
				continue;
			pNumStagesPassedByPlayMode->AppendChild( PlayModeToString(pm), m_iNumStagesPassedByPlayMode[pm] );
		}
	}

	{
		XNode* pNumStagesPassedByGrade = pGeneralDataNode->AppendChild("NumStagesPassedByGrade");
		FOREACH_ENUM( Grade, g )
		{
			if( !m_iNumStagesPassedByGrade[g] )
				continue;
			pNumStagesPassedByGrade->AppendChild( GradeToString(g), m_iNumStagesPassedByGrade[g] );
		}
	}

	// Load Lua UserTable from profile
	if( !IsMachine() && m_UserTable.IsSet() )
	{
		Lua *L = LUA->Get();
		m_UserTable.PushSelf( L );
		XNode* pUserTable = XmlFileUtil::XNodeFromTable( L );
		LUA->Release( L );

		// XXX: XNodeFromTable returns a root node with the name "Layer".
		pUserTable->m_sName = "UserTable";
		pGeneralDataNode->AppendChild( pUserTable );
	}

	return pGeneralDataNode;
}

ProfileLoadResult Profile::LoadEditableDataFromDir( const RString &sDir )
{
	RString fn = sDir + EDITABLE_INI;

	// Don't load unreasonably large editable.xml files.
	int iBytes = FILEMAN->GetFileSizeInBytes( fn );
	if( iBytes > MAX_EDITABLE_INI_SIZE_BYTES )
	{
		LuaHelpers::ReportScriptErrorFmt( "The file '%s' is unreasonably large. It won't be loaded.", fn.c_str() );
		return ProfileLoadResult_FailedTampered;
	}

	if( !IsAFile(fn) )
		return ProfileLoadResult_FailedNoProfile;

	IniFile ini;
	ini.ReadFile( fn );

	ini.GetValue( "Editable", "DisplayName",			m_sDisplayName );
	ini.GetValue( "Editable", "CharacterID",			m_sCharacterID );
	ini.GetValue( "Editable", "LastUsedHighScoreName",		m_sLastUsedHighScoreName );
	ini.GetValue( "Editable", "WeightPounds",			m_iWeightPounds );
	ini.GetValue( "Editable", "Voomax", m_Voomax );
	ini.GetValue( "Editable", "BirthYear", m_BirthYear );
	ini.GetValue( "Editable", "IgnoreStepCountCalories", m_IgnoreStepCountCalories );
	ini.GetValue( "Editable", "IsMale", m_IsMale );

	// This is data that the user can change, so we have to validate it.
	wstring wstr = RStringToWstring(m_sDisplayName);
	if( wstr.size() > PROFILE_MAX_DISPLAY_NAME_LENGTH )
		wstr = wstr.substr(0, PROFILE_MAX_DISPLAY_NAME_LENGTH);
	m_sDisplayName = WStringToRString(wstr);
	// TODO: strip invalid chars?
	if( m_iWeightPounds != 0 )
		CLAMP( m_iWeightPounds, 20, 1000 );

	return ProfileLoadResult_Success;
}

void Profile::LoadGeneralDataFromNode( const XNode* pNode )
{
	ASSERT( pNode->GetName() == "GeneralData" );

	RString s;
	const XNode* pTemp;

	pNode->GetChildValue( "DisplayName",				m_sDisplayName );
	pNode->GetChildValue( "CharacterID",				m_sCharacterID );
	pNode->GetChildValue( "LastUsedHighScoreName",			m_sLastUsedHighScoreName );
	pNode->GetChildValue( "WeightPounds",				m_iWeightPounds );
	pNode->GetChildValue( "Voomax", m_Voomax );
	pNode->GetChildValue( "BirthYear", m_BirthYear );
	pNode->GetChildValue( "IgnoreStepCountCalories", m_IgnoreStepCountCalories );
	pNode->GetChildValue( "IsMale", m_IsMale );
	pNode->GetChildValue( "Guid",					m_sGuid );
	pNode->GetChildValue( "SortOrder",				s );	m_SortOrder = StringToSortOrder( s );
	pNode->GetChildValue( "LastDifficulty",				s );	m_LastDifficulty = StringToDifficulty( s );
	pNode->GetChildValue( "LastCourseDifficulty",			s );	m_LastCourseDifficulty = StringToDifficulty( s );
	pNode->GetChildValue( "LastStepsType",				s );	m_LastStepsType = GAMEMAN->StringToStepsType( s );
	pTemp = pNode->GetChild( "Song" );				if( pTemp ) m_lastSong.LoadFromNode( pTemp );
	pTemp = pNode->GetChild( "Course" );				if( pTemp ) m_lastCourse.LoadFromNode( pTemp );
	pNode->GetChildValue( "CurrentCombo", m_iCurrentCombo );
	pNode->GetChildValue( "TotalSessions",				m_iTotalSessions );
	pNode->GetChildValue( "TotalSessionSeconds",			m_iTotalSessionSeconds );
	pNode->GetChildValue( "TotalGameplaySeconds",			m_iTotalGameplaySeconds );
	pNode->GetChildValue( "TotalCaloriesBurned",			m_fTotalCaloriesBurned );
	pNode->GetChildValue( "GoalType",				*ConvertValue<int>(&m_GoalType) );
	pNode->GetChildValue( "GoalCalories",				m_iGoalCalories );
	pNode->GetChildValue( "GoalSeconds",				m_iGoalSeconds );
	pNode->GetChildValue( "LastPlayedMachineGuid",			m_sLastPlayedMachineGuid );
	pNode->GetChildValue( "LastPlayedDate",				s ); m_LastPlayedDate.FromString( s );
	pNode->GetChildValue( "TotalDancePoints",			m_iTotalDancePoints );
	pNode->GetChildValue( "NumExtraStagesPassed",			m_iNumExtraStagesPassed );
	pNode->GetChildValue( "NumExtraStagesFailed",			m_iNumExtraStagesFailed );
	pNode->GetChildValue( "NumToasties",				m_iNumToasties );
	pNode->GetChildValue( "TotalTapsAndHolds",			m_iTotalTapsAndHolds );
	pNode->GetChildValue( "TotalJumps",				m_iTotalJumps );
	pNode->GetChildValue( "TotalHolds",				m_iTotalHolds );
	pNode->GetChildValue( "TotalRolls",				m_iTotalRolls );
	pNode->GetChildValue( "TotalMines",				m_iTotalMines );
	pNode->GetChildValue( "TotalHands",				m_iTotalHands );
	pNode->GetChildValue( "TotalLifts",				m_iTotalLifts );
	pNode->GetChildValue( "PlayerRating",			m_fPlayerRating);

	{
		const XNode* pDefaultModifiers = pNode->GetChild("DefaultModifiers");
		if( pDefaultModifiers )
		{
			FOREACH_CONST_Child( pDefaultModifiers, game_type )
			{
				game_type->GetTextValue( m_sDefaultModifiers[game_type->GetName()] );
			}
		}
	}

	{
		const XNode* pFavorites = pNode->GetChild("Favorites");
		if (pFavorites) {
			FOREACH_CONST_Child(pFavorites, ck) {
				RString tmp = ck->GetName();				// handle duplicated entries caused by an oversight - mina
				bool duplicated = false;
				FOREACH(RString, FavoritedCharts, chartkey)
					if (*chartkey == tmp)
						duplicated = true;
				if (!duplicated)
					FavoritedCharts.emplace_back(tmp);
			}
			SONGMAN->SetFavoritedStatus(FavoritedCharts);
		}
	}

	{
		const XNode* pPlayerSkillsets = pNode->GetChild("PlayerSkillsets");
		if (pPlayerSkillsets) {
			FOREACH_ENUM(Skillset, ss)
				pPlayerSkillsets->GetChildValue(SkillsetToString(ss), m_fPlayerSkillsets[ss]);
		}
	}

	{
		const XNode* pUnlocks = pNode->GetChild("Unlocks");
		if( pUnlocks )
		{
			FOREACH_CONST_Child( pUnlocks, unlock )
			{
				RString sUnlockEntryID;
				if( !unlock->GetAttrValue("UnlockEntryID",sUnlockEntryID) )
					continue;

				if( !UNLOCK_AUTH_STRING.GetValue().empty() )
				{
					RString sUnlockAuth;
					if( !unlock->GetAttrValue("Auth", sUnlockAuth) )
						continue;

					RString sExpectedUnlockAuth = BinaryToHex( CRYPTMAN->GetMD5ForString(sUnlockEntryID + UNLOCK_AUTH_STRING.GetValue()) );
					if( sUnlockAuth != sExpectedUnlockAuth )
						continue;
				}

				m_UnlockedEntryIDs.insert( sUnlockEntryID );
			}
		}
	}

	{
		const XNode* pNumSongsPlayedByPlayMode = pNode->GetChild("NumSongsPlayedByPlayMode");
		if( pNumSongsPlayedByPlayMode )
			FOREACH_ENUM( PlayMode, pm )
				pNumSongsPlayedByPlayMode->GetChildValue( PlayModeToString(pm), m_iNumSongsPlayedByPlayMode[pm] );
	}

	{
		const XNode* pNumSongsPlayedByStyle = pNode->GetChild("NumSongsPlayedByStyle");
		if( pNumSongsPlayedByStyle )
		{
			FOREACH_CONST_Child( pNumSongsPlayedByStyle, style )
			{
				if( style->GetName() != "Style" )
					continue;

				StyleID sID;
				sID.LoadFromNode( style );

				if( !sID.IsValid() )
					WARN_AND_CONTINUE;

				style->GetTextValue( m_iNumSongsPlayedByStyle[sID] );
			}
		}
	}

	{
		const XNode* pNumSongsPlayedByDifficulty = pNode->GetChild("NumSongsPlayedByDifficulty");
		if( pNumSongsPlayedByDifficulty )
			FOREACH_ENUM( Difficulty, dc )
				pNumSongsPlayedByDifficulty->GetChildValue( DifficultyToString(dc), m_iNumSongsPlayedByDifficulty[dc] );
	}

	{
		const XNode* pNumSongsPlayedByMeter = pNode->GetChild("NumSongsPlayedByMeter");
		if( pNumSongsPlayedByMeter )
			for( int i=0; i<MAX_METER+1; i++ )
				pNumSongsPlayedByMeter->GetChildValue( ssprintf("Meter%d",i), m_iNumSongsPlayedByMeter[i] );
	}

	pNode->GetChildValue("NumTotalSongsPlayed", m_iNumTotalSongsPlayed );

	{
		const XNode* pNumStagesPassedByGrade = pNode->GetChild("NumStagesPassedByGrade");
		if( pNumStagesPassedByGrade )
			FOREACH_ENUM( Grade, g )
				pNumStagesPassedByGrade->GetChildValue( GradeToString(g), m_iNumStagesPassedByGrade[g] );
	}

	{
		const XNode* pNumStagesPassedByPlayMode = pNode->GetChild("NumStagesPassedByPlayMode");
		if( pNumStagesPassedByPlayMode )
			FOREACH_ENUM( PlayMode, pm )
				pNumStagesPassedByPlayMode->GetChildValue( PlayModeToString(pm), m_iNumStagesPassedByPlayMode[pm] );

	}

	// Build the custom data table from the existing XNode.
	if( !IsMachine() )
	{
		const XNode *pUserTable = pNode->GetChild( "UserTable" );

		Lua *L = LUA->Get();

		// If we have custom data, load it. Otherwise, make a blank table.
		if( pUserTable )
			LuaHelpers::CreateTableFromXNode( L, pUserTable );
		else
			lua_newtable( L );

		m_UserTable.SetFromStack( L );
		LUA->Release( L );
	}

}

void Profile::AddStepTotals( int iTotalTapsAndHolds, int iTotalJumps, int iTotalHolds, int iTotalRolls, int iTotalMines, int iTotalHands, int iTotalLifts, float fCaloriesBurned )
{
	m_iTotalTapsAndHolds += iTotalTapsAndHolds;
	m_iTotalJumps += iTotalJumps;
	m_iTotalHolds += iTotalHolds;
	m_iTotalRolls += iTotalRolls;
	m_iTotalMines += iTotalMines;
	m_iTotalHands += iTotalHands;
	m_iTotalLifts += iTotalLifts;

	if(!m_IgnoreStepCountCalories)
	{
		m_fTotalCaloriesBurned += fCaloriesBurned;
		DateTime date = DateTime::GetNowDate();
		m_mapDayToCaloriesBurned[date].fCals += fCaloriesBurned;
	}
}

// It's a bit unclean to have this flag for routing around the old step count
// based calorie calculation, but I can't think of a better way to do it.
// AddStepTotals is called (through a couple layers) by CommitStageStats at
// the end of ScreenGameplay, so it can't be moved to somewhere else.  The
// player can't put in their heart rate for calculation until after
// ScreenGameplay
void Profile::AddCaloriesToDailyTotal(float cals)
{
	m_fTotalCaloriesBurned += cals;
	DateTime date = DateTime::GetNowDate();
	m_mapDayToCaloriesBurned[date].fCals += cals;
}

float Profile::CalculateCaloriesFromHeartRate(float HeartRate, float Duration)
{
	// Copied from http://www.shapesense.com/fitness-exercise/calculators/heart-rate-based-calorie-burn-calculator.aspx
	/*
		Male: ((-55.0969 + (0.6309 x HR) + (0.1988 x W) + (0.2017 x A))/4.184) x T
		Female: ((-20.4022 + (0.4472 x HR) - (0.1263 x W) + (0.074 x A))/4.184) x T
		where

		HR = Heart rate (in beats/minute) 
		W = Weight (in kilograms) 
		A = Age (in years) 
		T = Exercise duration time (in minutes)

		Equations for Determination of Calorie Burn if VO2max is Known

		Male: ((-95.7735 + (0.634 x HR) + (0.404 x VO2max) + (0.394 x W) + (0.271 x A))/4.184) x T
		Female: ((-59.3954 + (0.45 x HR) + (0.380 x VO2max) + (0.103 x W) + (0.274 x A))/4.184) x T
		where

		HR = Heart rate (in beats/minute) 
		VO2max = Maximal oxygen consumption (in mL•kg-1•min-1) 
		W = Weight (in kilograms) 
		A = Age (in years) 
		T = Exercise duration time (in minutes)
	*/
	// Duration passed in is in seconds.  Convert it to minutes to make the code
	// match the equations from the website.
	Duration= Duration / 60.f;
	float kilos= GetCalculatedWeightPounds() / 2.205f;
	float age= static_cast<float>(GetAge());

	// Names for the constants in the equations.
	// Assumes male and unknown voomax.
	float gender_factor= -55.0969f;
	float heart_factor= 0.6309f;
	float voo_factor= 0.0f;
	float weight_factor= 0.1988f;
	float age_factor= 0.2017f;
	if(m_Voomax > 0)
	{
		if(m_IsMale)
		{
			gender_factor= -95.7735f;
			heart_factor= 0.634f;
			voo_factor= 0.404f;
			weight_factor= 0.394f;
			age_factor= 0.271f;
		}
		else
		{
			gender_factor= -59.3954f;
			heart_factor= 0.45f;
			voo_factor= 0.380f;
			weight_factor= 0.103f;
			age_factor= 0.274f;
		}
	}
	else if(!m_IsMale)
	{
		gender_factor= -20.4022f;
		heart_factor= 0.6309f;
		weight_factor= 0.1988f;
		age_factor= 0.2017f;
	}
	return ((gender_factor + (heart_factor * HeartRate) +
			(voo_factor * m_Voomax) + (weight_factor * kilos) + (age_factor + age))
		/ 4.184f) * Duration;
}

XNode* Profile::SaveSongScoresCreateNode() const
{
	CHECKPOINT_M("Getting the node to save song scores.");

	const Profile* pProfile = this;
	ASSERT(pProfile != NULL);

	XNode* pNode = new XNode("SongScores");

	FOREACHM_CONST(SongID, HighScoresForASong, m_SongHighScores, i)
	{
		const SongID &songID = i->first;
		const HighScoresForASong &hsSong = i->second;

		// skip songs that have never been played
		if (pProfile->GetSongNumTimesPlayed(songID) == 0)
			continue;

		XNode* pSongNode = pNode->AppendChild(songID.CreateNode());

		int jCheck2 = hsSong.m_StepsHighScores.size();
		int jCheck1 = 0;
		FOREACHM_CONST(StepsID, HighScoresForASteps, hsSong.m_StepsHighScores, j)
		{
			jCheck1++;
			ASSERT(jCheck1 <= jCheck2);
			const StepsID &stepsID = j->first;
			const HighScoresForASteps &hsSteps = j->second;

			const HighScoreList &hsl = hsSteps.hsl;

			// skip steps that have never been played
			if (hsl.GetNumTimesPlayed() == 0)
				continue;

			XNode* pStepsNode = pSongNode->AppendChild(stepsID.CreateNode());

			pStepsNode->AppendChild(hsl.CreateNode());
		}
	}

	return pNode;
}

void Profile::RemoveFromFavorites(RString ck) {
	for (size_t i = 0; i < FavoritedCharts.size(); ++i) {
		if (FavoritedCharts[i] == ck)
			FavoritedCharts.erase(FavoritedCharts.begin() + i);
	}
}

void Profile::LoadSongScoresFromNode(const XNode* pSongScores)
{
	CHECKPOINT_M("Loading the node that contains song scores.");

	ASSERT(pSongScores->GetName() == "SongScores");

	FOREACH_CONST_Child(pSongScores, pSong)
	{
		if (pSong->GetName() != "Song")
			continue;

		SongID songID;
		songID.LoadFromNode(pSong);
		// Allow invalid songs so that scores aren't deleted for people that use
		// AdditionalSongsFolders and change it frequently. -Kyz
		//if( !songID.IsValid() )
		//	continue;

		FOREACH_CONST_Child(pSong, pSteps)
		{
			if (pSteps->GetName() != "Steps")
				continue;

			StepsID stepsID;
			stepsID.LoadFromNode(pSteps);
			if (!stepsID.IsValid())
				WARN_AND_CONTINUE;

			const XNode *pHighScoreListNode = pSteps->GetChild("HighScoreList");
			if (pHighScoreListNode == NULL)
				WARN_AND_CONTINUE;

			HighScoreList &hsl = m_SongHighScores[songID].m_StepsHighScores[stepsID].hsl;
			hsl.LoadFromNode(pHighScoreListNode);
		}
	}
}

void Profile::LoadStatsXmlForConversion() {
	string dir = profiledir;
	RString fn = dir + STATS_XML;
	bool compressed = false;
	if (!IsAFile(fn)) {
		fn = dir + STATS_XML_GZ;
		compressed = true;
		if (!IsAFile(fn)) {
			return ;
		}
	}

	int iError;
	unique_ptr<RageFileBasic> pFile(FILEMAN->Open(fn, RageFile::READ, iError));
	if (pFile.get() == NULL)
		return;

	if (compressed) {
		RString sError;
		uint32_t iCRC32;
		RageFileObjInflate *pInflate = GunzipFile(pFile.release(), sError, &iCRC32);
		if (pInflate == NULL)
			return;

		pFile.reset(pInflate);
	}

	XNode xml;
	if (!XmlFileUtil::LoadFromFileShowErrors(xml, *pFile.get()))
		return;

	XNode* scores = xml.GetChild("SongScores");
	LoadSongScoresFromNode(scores);
}

void Profile::ImportScoresToEtterna() {
	// load stats.xml
	if(IsEtternaProfile)
		LoadStatsXmlForConversion();

	string ck;
	FOREACHM(SongID, HighScoresForASong, m_SongHighScores, i) {
		SongID id = i->first;
		HighScoresForASong& hsfas = i->second;
		FOREACHM(StepsID, HighScoresForASteps, hsfas.m_StepsHighScores, j) {
			StepsID sid = j->first;
			
			if (sid.GetStepsType() != StepsType_dance_single)
				continue;

			if (!id.IsValid()) {
				string sdir = id.ToString();
				string sdir2;

				if (sdir.substr(0, 15) == "AdditionalSongs") {
					sdir2 = id.ToString().substr(10);
				} else if (sdir.substr(0, 5) == "Songs") {
					sdir2 = "Additional" + id.ToString();
				}
				Song* imean = new Song;
				imean->SetSongDir(sdir2);
				id = SongID();
				id.FromSong(imean);
			}

			if (id.IsValid() && sid.IsValid()) {
				vector<HighScore>& hsv = j->second.hsl.vHighScores;

				Song* song = id.ToSong();
				Steps* steps = sid.ToSteps(song, true);

				if (!steps)
					continue;

				ck = steps->GetChartKey();
				for (size_t i = 0; i < hsv.size(); ++i) {
					HighScore hs = hsv[i];
					// ignore historic key and just load from here since the hashing function was changed anyway
					hs.SetChartKey(ck);		
					pscores.AddScore(hs);
				}
			}			
		}
	}
}

XNode* Profile::SaveEttScoresCreateNode() const {
	CHECKPOINT_M("Getting the node to save song scores.");

	const Profile* pProfile = this;
	ASSERT(pProfile != NULL);

	XNode* pNode = pscores.CreateNode();

	return pNode;
}

void Profile::LoadEttScoresFromNode(const XNode* pSongScores) {
	CHECKPOINT_M("Loading the node that contains song scores.");
	pscores.LoadFromNode(pSongScores);
}

// more future goalman stuff
void Profile::CreateGoal(RString ck) {
	ScoreGoal goal;
	goal.timeassigned = DateTime::GetNowDateTime();
	goal.rate = GAMESTATE->m_SongOptions.GetCurrent().m_fMusicRate;
	goalmap[ck].emplace_back(goal);
	LOG->Trace("New goal created %i goals", goalmap[ck].size());
}

XNode* ScoreGoal::CreateNode() const {
	XNode* pNode = new XNode("ScoreGoal");

	pNode->AppendChild("Rate", rate);
	pNode->AppendChild("Percent", percent);
	pNode->AppendChild("Priority", priority);
	pNode->AppendChild("Achieved", achieved);
	pNode->AppendChild("TimeAssigned", timeassigned.GetString());
	if (achieved) {
		pNode->AppendChild("TimeAchieved", timeachieved.GetString());
		pNode->AppendChild("ScoreKey", scorekey);
	}
		
	pNode->AppendChild("Comment", comment);

	return pNode;
}

void ScoreGoal::LoadFromNode(const XNode *pNode) {
	ASSERT(pNode->GetName() == "ScoreGoal");

	RString s;

	pNode->GetChildValue("Rate", rate);
	pNode->GetChildValue("Percent", percent);
	if (percent > 1.f)	// goddamnit why didnt i think this through originally
		percent /= 100.f;
	pNode->GetChildValue("Priority", priority);
	pNode->GetChildValue("Achieved", achieved);
	pNode->GetChildValue("TimeAssigned", s); timeassigned.FromString(s);
	if (achieved) {
		pNode->GetChildValue("TimeAchieved", s); timeachieved.FromString(s);
		pNode->GetChildValue("ScoreKey", s); scorekey;
	}
		
	pNode->GetChildValue("Comment", comment);
}

HighScore* ScoreGoal::GetPBUpTo() {
	auto& scores = PROFILEMAN->GetProfile(PLAYER_1)->pscores;
	return scores.GetChartPBUpTo(chartkey, rate);
}

void ScoreGoal::CheckVacuity() {
	auto& scores = PROFILEMAN->GetProfile(PLAYER_1)->pscores;
	HighScore* pb = scores.GetChartPBAt(chartkey, rate);

	if (pb && pb->GetWifeScore() >= percent)
		vacuous = true;
	else
		vacuous = false;
}

// aaa too lazy to write comparators rn -mina
ScoreGoal& Profile::GetLowestGoalForRate(RString ck, float rate) {
	auto& sgv = goalmap[ck];
	float lowest = 100.f;
	int lowestidx = 0;
	for (size_t i = 0; i < sgv.size(); ++i) {
		ScoreGoal& tmp = sgv[i];
		if (tmp.rate == rate) {
			if (tmp.percent > lowest) {
				lowest = tmp.percent;
				lowestidx = i;
			}
		}
	}
	return sgv[lowestidx];
}

void Profile::SetAnyAchievedGoals(RString ck, float rate, const HighScore& pscore) {
	if (!HasGoal(ck))
		return;

	auto& sgv = goalmap[ck];
	for (size_t i = 0; i < sgv.size(); ++i) {
		ScoreGoal& tmp = sgv[i];
		if (lround(tmp.rate * 10000.f) == lround(rate * 10000.f) && !tmp.achieved &&tmp.percent < pscore.GetWifeScore()) {
			tmp.achieved = true;
			tmp.timeachieved = pscore.GetDateTime();
			tmp.scorekey = pscore.GetScoreKey();
		}
	}
}

void Profile::DeleteGoal(RString ck, DateTime assigned) {
	auto& sgv = goalmap.at(ck);
	for (size_t i = 0; i < sgv.size(); ++i) {
		if (sgv[i].timeassigned == assigned)
			sgv.erase(sgv.begin() + i);
	}
}

// should deal with this misnomer - mina
void Profile::ValidateAllScores() {
	FOREACHM(SongID, HighScoresForASong, m_SongHighScores, i) {
		const SongID& id = i->first;

		HighScoresForASong& hsfas = i->second;
		FOREACHM(StepsID, HighScoresForASteps, hsfas.m_StepsHighScores, j) {
			HighScoresForASteps& zz = j->second;

			// validate scores that match a loaded chartkey - mina
			Steps* pSteps = SONGMAN->GetStepsByChartkey(j->first);

			if (!pSteps)
				continue;

			vector<HighScore>& hsv = zz.hsl.vHighScores;
			for (size_t i = 0; i < hsv.size(); i++)
				hsv[i].SetEtternaValid(true);
		}
	}
}

// should prolly generalize some of the stuff here - mina
void Profile::RecalculateSSRs(bool OnlyOld) {
	FOREACHM(SongID, HighScoresForASong, m_SongHighScores, i) {
		const SongID& id = i->first;
		
		HighScoresForASong& hsfas = i->second;
		FOREACHM(StepsID, HighScoresForASteps, hsfas.m_StepsHighScores, j) {
			HighScoresForASteps& zz = j->second;
			vector<HighScore>& hsv = zz.hsl.vHighScores;

			Steps* pSteps = SONGMAN->GetStepsByChartkey(j->first);

			if (!pSteps) 
				continue;

			if (!pSteps->IsRecalcValid()) {
				for (size_t i = 0; i < hsv.size(); i++) {
					FOREACH_ENUM(Skillset, ss)
						hsv[i].SetSkillsetSSR(ss, 0.f);
				}
				continue;
			}

			vector<float> etaner;
			for (size_t i = 0; i < hsv.size(); i++) {
				float ssrpercent = hsv[i].GetSSRNormPercent();
				float musicrate = hsv[i].GetMusicRate();
				if (ssrpercent <= 0.f || hsv[i].GetGrade() == Grade_Failed)
					FOREACH_ENUM(Skillset, ss)
						hsv[i].SetSkillsetSSR(ss, 0.f);
				else {
					if (OnlyOld && hsv[i].GetSSRCalcVersion() == GetCalcVersion())
						continue;

					// should find away to avoid calling this more than once -
					NoteData& nd = pSteps->GetNoteData();

					// only build etaner once
					if (etaner.empty()) {
						TimingData* td = pSteps->GetTimingData();
						vector<int>& nerv = nd.GetNonEmptyRowVector();
						for (size_t i = 0; i < nerv.size(); i++)
							etaner.emplace_back(td->GetElapsedTimeFromBeatNoOffset(NoteRowToBeat(nerv[i])));
					}

					vector<float> recalcSSR = MinaSDCalc(nd.SerializeNoteData(etaner), nd.GetNumTracks(), musicrate, ssrpercent, 1.f, false);
					FOREACH_ENUM(Skillset, ss)
						hsv[i].SetSkillsetSSR(ss, recalcSSR[ss]);
					hsv[i].SetSSRCalcVersion(GetCalcVersion());
				}
			}
		}
	}
}


float Profile::GetTopSSRValue(unsigned int rank, int ss) {
	if (rank < 0)
		rank = 0;
	HighScore *highScorePtr = GetTopSSRHighScore(rank, ss);
	//Empty HighScore Pointer = NULL then return 0
	if(highScorePtr == NULL)
		return 0.f;

	if (ss >= 0 && ss < NUM_Skillset && rank < pscores.TopSSRs[ss].size())
		return highScorePtr->GetSkillsetSSR(static_cast<Skillset>(ss));

	//Undefined skillset
	return 0.f;
}

HighScore* Profile::GetTopSSRHighScore(unsigned int rank, int ss) {
	if (rank < 0)
		rank = 0;

	if (ss >= 0 && ss < NUM_Skillset && rank < pscores.TopSSRs[ss].size())
		return &(*pscores.TopSSRs[ss][rank]);

	return NULL;
}

XNode* Profile::SaveCourseScoresCreateNode() const
{
	CHECKPOINT_M("Getting the node to save course scores.");

	const Profile* pProfile = this;
	ASSERT( pProfile != NULL );

	XNode* pNode = new XNode( "CourseScores" );

	FOREACHM_CONST( CourseID, HighScoresForACourse, m_CourseHighScores, i )
	{
		const CourseID &courseID = i->first;
		const HighScoresForACourse &hsCourse = i->second;

		// skip courses that have never been played
		if( pProfile->GetCourseNumTimesPlayed(courseID) == 0 )
			continue;

		XNode* pCourseNode = pNode->AppendChild( courseID.CreateNode() );

		FOREACHM_CONST( TrailID, HighScoresForATrail, hsCourse.m_TrailHighScores, j )
		{
			const TrailID &trailID = j->first;
			const HighScoresForATrail &hsTrail = j->second;

			const HighScoreList &hsl = hsTrail.hsl;

			// skip steps that have never been played
			if( hsl.GetNumTimesPlayed() == 0 )
				continue;

			XNode* pTrailNode = pCourseNode->AppendChild( trailID.CreateNode() );

			pTrailNode->AppendChild( hsl.CreateNode() );
		}
	}

	return pNode;
}

void Profile::LoadCourseScoresFromNode( const XNode* pCourseScores )
{
	CHECKPOINT_M("Loading the node that contains course scores.");

	ASSERT( pCourseScores->GetName() == "CourseScores" );

	vector<Course*> vpAllCourses;
	SONGMAN->GetAllCourses( vpAllCourses, true );

	FOREACH_CONST_Child( pCourseScores, pCourse )
	{
		if( pCourse->GetName() != "Course" )
			continue;

		CourseID courseID;
		courseID.LoadFromNode( pCourse );
		// Allow invalid courses so that scores aren't deleted for people that use
		// AdditionalCoursesFolders and change it frequently. -Kyz
		//if( !courseID.IsValid() )
		//	WARN_AND_CONTINUE;


		// Backward compatability hack to fix importing scores of old style 
		// courses that weren't in group folder but have now been moved into
		// a group folder: 
		// If the courseID doesn't resolve, then take the file name part of sPath
		// and search for matches of just the file name.
		{
			Course *pC = courseID.ToCourse();
			if( pC == NULL )
			{
				RString sDir, sFName, sExt;
				splitpath( courseID.GetPath(), sDir, sFName, sExt );
				RString sFullFileName = sFName + sExt;

				FOREACH_CONST( Course*, vpAllCourses, c )
				{
					RString sOther = (*c)->m_sPath.Right(sFullFileName.size());

					if( sFullFileName.CompareNoCase(sOther) == 0 )
					{
						pC = *c;
						courseID.FromCourse( pC );
						break;
					}
				}
			}
		}


		FOREACH_CONST_Child( pCourse, pTrail )
		{
			if( pTrail->GetName() != "Trail" )
				continue;
			
			TrailID trailID;
			trailID.LoadFromNode( pTrail );
			if( !trailID.IsValid() )
				WARN_AND_CONTINUE;

			const XNode *pHighScoreListNode = pTrail->GetChild("HighScoreList");
			if( pHighScoreListNode == NULL )
				WARN_AND_CONTINUE;
			
			HighScoreList &hsl = m_CourseHighScores[courseID].m_TrailHighScores[trailID].hsl;
			hsl.LoadFromNode( pHighScoreListNode );
		}
	}
}

XNode* Profile::SaveCategoryScoresCreateNode() const
{
	CHECKPOINT_M("Getting the node that saves category scores.");

	const Profile* pProfile = this;
	ASSERT( pProfile != NULL );

	XNode* pNode = new XNode( "CategoryScores" );

	FOREACH_ENUM( StepsType,st )
	{
		// skip steps types that have never been played
		if( pProfile->GetCategoryNumTimesPlayed( st ) == 0 )
			continue;

		XNode* pStepsTypeNode = pNode->AppendChild( "StepsType" );
		pStepsTypeNode->AppendAttr( "Type", GAMEMAN->GetStepsTypeInfo(st).szName );

		FOREACH_ENUM( RankingCategory,rc )
		{
			// skip steps types/categories that have never been played
			if( pProfile->GetCategoryHighScoreList(st,rc).GetNumTimesPlayed() == 0 )
				continue;

			XNode* pRankingCategoryNode = pStepsTypeNode->AppendChild( "RankingCategory" );
			pRankingCategoryNode->AppendAttr( "Type", RankingCategoryToString(rc) );

			const HighScoreList &hsl = pProfile->GetCategoryHighScoreList( (StepsType)st, (RankingCategory)rc );

			pRankingCategoryNode->AppendChild( hsl.CreateNode() );
		}
	}

	return pNode;
}

void Profile::LoadCategoryScoresFromNode( const XNode* pCategoryScores )
{
	CHECKPOINT_M("Loading the node that contains category scores.");

	ASSERT( pCategoryScores->GetName() == "CategoryScores" );

	FOREACH_CONST_Child( pCategoryScores, pStepsType )
	{
		if( pStepsType->GetName() != "StepsType" )
			continue;

		RString str;
		if( !pStepsType->GetAttrValue( "Type", str ) )
			WARN_AND_CONTINUE;
		StepsType st = GAMEMAN->StringToStepsType( str );
		if( st == StepsType_Invalid )
			WARN_AND_CONTINUE_M( str );

		FOREACH_CONST_Child( pStepsType, pRadarCategory )
		{
			if( pRadarCategory->GetName() != "RankingCategory" )
				continue;

			if( !pRadarCategory->GetAttrValue( "Type", str ) )
				WARN_AND_CONTINUE;
			RankingCategory rc = StringToRankingCategory( str );
			if( rc == RankingCategory_Invalid )
				WARN_AND_CONTINUE_M( str );

			const XNode *pHighScoreListNode = pRadarCategory->GetChild("HighScoreList");
			if( pHighScoreListNode == NULL )
				WARN_AND_CONTINUE;
			
			HighScoreList &hsl = this->GetCategoryHighScoreList( st, rc );
			hsl.LoadFromNode( pHighScoreListNode );
		}
	}
}

void Profile::SaveStatsWebPageToDir( const RString &sDir) const
{
	ASSERT( PROFILEMAN != NULL );
}

void Profile::SaveMachinePublicKeyToDir( const RString &sDir ) const
{
	if( PREFSMAN->m_bSignProfileData && IsAFile(CRYPTMAN->GetPublicKeyFileName()) )
		FileCopy( CRYPTMAN->GetPublicKeyFileName(), sDir+PUBLIC_KEY_FILE );
}

void Profile::AddScreenshot( const Screenshot &screenshot )
{
	m_vScreenshots.push_back( screenshot );
}

void Profile::LoadScreenshotDataFromNode( const XNode* pScreenshotData )
{
	CHECKPOINT_M("Loading the node containing screenshot data.");

	ASSERT( pScreenshotData->GetName() == "ScreenshotData" );
	FOREACH_CONST_Child( pScreenshotData, pScreenshot )
	{
		if( pScreenshot->GetName() != "Screenshot" )
			WARN_AND_CONTINUE_M( pScreenshot->GetName() );

		Screenshot ss;
		ss.LoadFromNode( pScreenshot );

		m_vScreenshots.push_back( ss );
	}
}

XNode* Profile::SaveScreenshotDataCreateNode() const
{
	CHECKPOINT_M("Getting the node containing screenshot data.");

	const Profile* pProfile = this;
	ASSERT( pProfile != NULL );

	XNode* pNode = new XNode( "ScreenshotData" );

	FOREACH_CONST( Screenshot, m_vScreenshots, ss )
	{
		pNode->AppendChild( ss->CreateNode() );
	}

	return pNode;
}

void Profile::LoadCalorieDataFromNode( const XNode* pCalorieData )
{
	CHECKPOINT_M("Loading the node containing calorie data.");

	ASSERT( pCalorieData->GetName() == "CalorieData" );
	FOREACH_CONST_Child( pCalorieData, pCaloriesBurned )
	{
		if( pCaloriesBurned->GetName() != "CaloriesBurned" )
			WARN_AND_CONTINUE_M( pCaloriesBurned->GetName() );

		RString sDate;
		if( !pCaloriesBurned->GetAttrValue("Date",sDate) )
			WARN_AND_CONTINUE;
		DateTime date;
		if( !date.FromString(sDate) )
			WARN_AND_CONTINUE_M( sDate );

		float fCaloriesBurned = 0;

		pCaloriesBurned->GetTextValue(fCaloriesBurned);

		m_mapDayToCaloriesBurned[date].fCals = fCaloriesBurned;
	}	
}

XNode* Profile::SaveCalorieDataCreateNode() const
{
	CHECKPOINT_M("Getting the node containing calorie data.");

	const Profile* pProfile = this;
	ASSERT( pProfile != NULL );

	XNode* pNode = new XNode( "CalorieData" );

	FOREACHM_CONST( DateTime, Calories, m_mapDayToCaloriesBurned, i )
	{
		XNode* pCaloriesBurned = pNode->AppendChild( "CaloriesBurned", i->second.fCals );

		pCaloriesBurned->AppendAttr( "Date", i->first.GetString() );
	}

	return pNode;
}

float Profile::GetCaloriesBurnedForDay( DateTime day ) const
{
	day.StripTime();
	map<DateTime,Calories>::const_iterator i = m_mapDayToCaloriesBurned.find( day );
	if( i == m_mapDayToCaloriesBurned.end() )
		return 0;
	else
		return i->second.fCals;
}

const Profile::HighScoresForASong *Profile::GetHighScoresForASong( const SongID& songID ) const
{
	map<SongID,HighScoresForASong>::const_iterator it;
	it = m_SongHighScores.find( songID );
	if( it == m_SongHighScores.end() )
		return NULL;
	return &it->second;
}

const Profile::HighScoresForACourse *Profile::GetHighScoresForACourse( const CourseID& courseID ) const
{
	map<CourseID,HighScoresForACourse>::const_iterator it;
	it = m_CourseHighScores.find( courseID );
	if( it == m_CourseHighScores.end() )
		return NULL;
	return &it->second;
}

bool Profile::IsMachine() const
{
	// TODO: Think of a better way to handle this
	return this == PROFILEMAN->GetMachineProfile();
}


XNode* Profile::SaveCoinDataCreateNode() const
{
	CHECKPOINT_M("Getting the node containing coin data.");

	const Profile* pProfile = this;
	ASSERT( pProfile != NULL );

	XNode* pNode = new XNode( "CoinData" );

	{
		int coins[NUM_LAST_DAYS];
		BOOKKEEPER->GetCoinsLastDays( coins );
		XNode* p = pNode->AppendChild( "LastDays" );
		for( int i=0; i<NUM_LAST_DAYS; i++ )
			p->AppendChild( LastDayToString(i), coins[i] );
	}
	{
		int coins[NUM_LAST_WEEKS];
		BOOKKEEPER->GetCoinsLastWeeks( coins );
		XNode* p = pNode->AppendChild( "LastWeeks" );
		for( int i=0; i<NUM_LAST_WEEKS; i++ )
			p->AppendChild( LastWeekToString(i), coins[i] );
	}
	{
		int coins[DAYS_IN_WEEK];
		BOOKKEEPER->GetCoinsByDayOfWeek( coins );
		XNode* p = pNode->AppendChild( "DayOfWeek" );
		for( int i=0; i<DAYS_IN_WEEK; i++ )
			p->AppendChild( DayOfWeekToString(i), coins[i] );
	}
	{
		int coins[HOURS_IN_DAY];
		BOOKKEEPER->GetCoinsByHour( coins );
		XNode* p = pNode->AppendChild( "Hour" );
		for( int i=0; i<HOURS_IN_DAY; i++ )
			p->AppendChild( HourInDayToString(i), coins[i] );
	}

	return pNode;
}

void Profile::MoveBackupToDir( const RString &sFromDir, const RString &sToDir )
{
	if( FILEMAN->IsAFile(sFromDir + STATS_XML) &&
		FILEMAN->IsAFile(sFromDir+STATS_XML+SIGNATURE_APPEND) )
	{
		FILEMAN->Move( sFromDir+STATS_XML,					sToDir+STATS_XML );
		FILEMAN->Move( sFromDir+STATS_XML+SIGNATURE_APPEND,	sToDir+STATS_XML+SIGNATURE_APPEND );
	}
	else if( FILEMAN->IsAFile(sFromDir + STATS_XML_GZ) &&
		FILEMAN->IsAFile(sFromDir+STATS_XML_GZ+SIGNATURE_APPEND) )
	{
		FILEMAN->Move( sFromDir+STATS_XML_GZ,					sToDir+STATS_XML );
		FILEMAN->Move( sFromDir+STATS_XML_GZ+SIGNATURE_APPEND,	sToDir+STATS_XML+SIGNATURE_APPEND );
	}

	if( FILEMAN->IsAFile(sFromDir + EDITABLE_INI) )
		FILEMAN->Move( sFromDir+EDITABLE_INI,				sToDir+EDITABLE_INI );
	if( FILEMAN->IsAFile(sFromDir + DONT_SHARE_SIG) )
		FILEMAN->Move( sFromDir+DONT_SHARE_SIG,				sToDir+DONT_SHARE_SIG );
}

RString Profile::MakeUniqueFileNameNoExtension( const RString &sDir, const RString &sFileNameBeginning )
{
	FILEMAN->FlushDirCache( sDir );
	// Find a file name for the screenshot
	vector<RString> files;
	GetDirListing( sDir + sFileNameBeginning+"*", files, false, false );
	sort( files.begin(), files.end() );

	int iIndex = 0;

	for( int i = files.size()-1; i >= 0; --i )
	{
		static Regex re( "^" + sFileNameBeginning + "([0-9]{5})\\....$" );
		vector<RString> matches;
		if( !re.Compare( files[i], matches ) )
			continue;

		ASSERT( matches.size() == 1 );
		iIndex = StringToInt( matches[0] )+1;
		break;
	}

	return MakeFileNameNoExtension( sFileNameBeginning, iIndex );
}

RString Profile::MakeFileNameNoExtension( const RString &sFileNameBeginning, int iIndex )
{
	return sFileNameBeginning + ssprintf( "%05d", iIndex );
}

// lua start
#include "LuaBinding.h"

/** @brief Allow Lua to have access to the Profile. */ 
class LunaProfile : public Luna<Profile>
{
public:
	static int AddScreenshot(T* p, lua_State *L)
	{
		HighScore* hs = Luna<HighScore>::check(L, 1);
		RString filename = SArg(2);
		Screenshot screenshot;
		screenshot.sFileName = filename;
		screenshot.sMD5 = BinaryToHex(CRYPTMAN->GetMD5ForFile(filename));
		screenshot.highScore = *hs;
		p->AddScreenshot(screenshot);
		COMMON_RETURN_SELF;
	}
	DEFINE_METHOD(GetType, m_Type);
	DEFINE_METHOD(GetPriority, m_ListPriority);

	static int GetDisplayName(T* p, lua_State *L) { lua_pushstring(L, p->m_sDisplayName); return 1; }
	static int SetDisplayName(T* p, lua_State *L)
	{
		p->m_sDisplayName = SArg(1);
		COMMON_RETURN_SELF;
	}
	static int GetLastUsedHighScoreName(T* p, lua_State *L) { lua_pushstring(L, p->m_sLastUsedHighScoreName); return 1; }
	static int SetLastUsedHighScoreName(T* p, lua_State *L)
	{
		p->m_sLastUsedHighScoreName = SArg(1);
		COMMON_RETURN_SELF;
	}
	static int GetHighScoreList(T* p, lua_State *L)
	{
		if (LuaBinding::CheckLuaObjectType(L, 1, "Song"))
		{
			const Song *pSong = Luna<Song>::check(L, 1);
			const Steps *pSteps = Luna<Steps>::check(L, 2);
			HighScoreList &hsl = p->GetStepsHighScoreList(pSong, pSteps);
			hsl.PushSelf(L);
			return 1;
		}
		else if (LuaBinding::CheckLuaObjectType(L, 1, "Course"))
		{
			const Course *pCourse = Luna<Course>::check(L, 1);
			const Trail *pTrail = Luna<Trail>::check(L, 2);
			HighScoreList &hsl = p->GetCourseHighScoreList(pCourse, pTrail);
			hsl.PushSelf(L);
			return 1;
		}

		luaL_typerror(L, 1, "Song or Course");
		COMMON_RETURN_SELF;
	}

	static int GetScoresByKey(T* p, lua_State *L) {
		auto& lol = p->pscores;
		string ck = SArg(1);
		if (lol.pscores.count(ck)) {
			lua_newtable(L);
			auto& doot = lol.pscores[ck];
			vector<int> ratekeys = doot.GetPlayedRateKeys();
			vector<float> rates = doot.GetPlayedRates();
			for (size_t i = 0; i < rates.size(); ++i) {
				// ok this is pretty kms im really tired -mina
				RString tmp = ssprintf("%.2f", rates[i]);
				int j = 1;
				if (tmp.find_last_not_of('0') == tmp.find('.')) 
					j = 2;
				tmp.erase(tmp.find_last_not_of('0') + j, tmp.npos);
				tmp.append("x");
				LuaHelpers::Push(L, tmp);
				doot.ScoresByRate[ratekeys[i]].PushSelf(L);
				lua_rawset(L, -3);
			}

			return 1;
		}

		lua_pushnil(L);
		return 1;
	}

	static int GetCategoryHighScoreList(T* p, lua_State *L)
	{
		StepsType pStepsType = Enum::Check<StepsType>(L, 1);
		RankingCategory pRankCat = Enum::Check<RankingCategory>(L, 2);
		HighScoreList &hsl = p->GetCategoryHighScoreList(pStepsType, pRankCat);
		hsl.PushSelf(L);
		return 1;
	}

	static int GetHighScoreListIfExists(T* p, lua_State *L)
	{
#define GET_IF_EXISTS(arga_type, argb_type) \
		const arga_type *parga = Luna<arga_type>::check(L, 1); \
		const argb_type *pargb = Luna<argb_type>::check(L, 2); \
		arga_type##ID arga_id; \
		arga_id.From##arga_type(parga); \
		argb_type##ID argb_id; \
		argb_id.From##argb_type(pargb); \
		std::map<arga_type##ID, Profile::HighScoresForA##arga_type>::iterator \
			main_scores= p->m_##arga_type##HighScores.find(arga_id); \
		if(main_scores == p->m_##arga_type##HighScores.end()) \
		{ \
			lua_pushnil(L); \
			return 1; \
		} \
		std::map<argb_type##ID, Profile::HighScoresForA##argb_type>::iterator \
			sub_scores= main_scores->second.m_##argb_type##HighScores.find(argb_id); \
		if(sub_scores == main_scores->second.m_##argb_type##HighScores.end()) \
		{ \
			lua_pushnil(L); \
			return 1; \
		} \
		sub_scores->second.hsl.PushSelf(L); \
		return 1;

		if (LuaBinding::CheckLuaObjectType(L, 1, "Song"))
		{
			GET_IF_EXISTS(Song, Steps);
		}
		else if (LuaBinding::CheckLuaObjectType(L, 1, "Course"))
		{
			GET_IF_EXISTS(Course, Trail);
		}
		luaL_typerror(L, 1, "Song or Course");
		return 0;
#undef GET_IF_EXISTS
	}

	static int GetAllUsedHighScoreNames(T* p, lua_State *L)
	{
		std::set<RString> names;
		p->GetAllUsedHighScoreNames(names);
		lua_createtable(L, names.size(), 0);
		int next_name_index = 1;
		for (std::set<RString>::iterator name = names.begin(); name != names.end();
			++name)
		{
			lua_pushstring(L, name->c_str());
			lua_rawseti(L, -2, next_name_index);
			++next_name_index;
		}
		return 1;
	}

	static int GetCharacter(T* p, lua_State *L) { p->GetCharacter()->PushSelf(L); return 1; }
	static int SetCharacter(T* p, lua_State *L) { p->SetCharacter(SArg(1)); COMMON_RETURN_SELF; }
	static int GetWeightPounds(T* p, lua_State *L) { lua_pushnumber(L, p->m_iWeightPounds); return 1; }
	static int SetWeightPounds(T* p, lua_State *L) { p->m_iWeightPounds = IArg(1); COMMON_RETURN_SELF; }
	DEFINE_METHOD(GetVoomax, m_Voomax);
	DEFINE_METHOD(GetAge, GetAge());
	DEFINE_METHOD(GetBirthYear, m_BirthYear);
	DEFINE_METHOD(GetIgnoreStepCountCalories, m_IgnoreStepCountCalories);
	DEFINE_METHOD(GetIsMale, m_IsMale);
	static int SetVoomax(T* p, lua_State *L)
	{
		p->m_Voomax = FArg(1);
		COMMON_RETURN_SELF;
	}
	static int SetBirthYear(T* p, lua_State *L)
	{
		p->m_BirthYear = IArg(1);
		COMMON_RETURN_SELF;
	}
	static int SetIgnoreStepCountCalories(T* p, lua_State *L)
	{
		p->m_IgnoreStepCountCalories = BArg(1);
		COMMON_RETURN_SELF;
	}
	static int SetIsMale(T* p, lua_State *L)
	{
		p->m_IsMale = BArg(1);
		COMMON_RETURN_SELF;
	}
	static int AddCaloriesToDailyTotal(T* p, lua_State *L)
	{
		p->AddCaloriesToDailyTotal(FArg(1));
		COMMON_RETURN_SELF;
	}
	DEFINE_METHOD(CalculateCaloriesFromHeartRate, CalculateCaloriesFromHeartRate(FArg(1), FArg(2)));
	static int GetGoalType(T* p, lua_State *L) { lua_pushnumber(L, p->m_GoalType); return 1; }
	static int SetGoalType(T* p, lua_State *L) { p->m_GoalType = Enum::Check<GoalType>(L, 1); COMMON_RETURN_SELF; }
	static int GetGoalCalories(T* p, lua_State *L) { lua_pushnumber(L, p->m_iGoalCalories); return 1; }
	static int SetGoalCalories(T* p, lua_State *L) { p->m_iGoalCalories = IArg(1); COMMON_RETURN_SELF; }
	static int GetGoalSeconds(T* p, lua_State *L) { lua_pushnumber(L, p->m_iGoalSeconds); return 1; }
	static int SetGoalSeconds(T* p, lua_State *L) { p->m_iGoalSeconds = IArg(1); COMMON_RETURN_SELF; }
	static int GetCaloriesBurnedToday(T* p, lua_State *L) { lua_pushnumber(L, p->GetCaloriesBurnedToday()); return 1; }
	static int GetTotalNumSongsPlayed(T* p, lua_State *L) { lua_pushnumber(L, p->m_iNumTotalSongsPlayed); return 1; }
	static int IsCodeUnlocked(T* p, lua_State *L) { lua_pushboolean(L, p->IsCodeUnlocked(SArg(1))); return 1; }
	static int GetSongsActual(T* p, lua_State *L) { lua_pushnumber(L, p->GetSongsActual(Enum::Check<StepsType>(L, 1), Enum::Check<Difficulty>(L, 2))); return 1; }
	static int GetCoursesActual(T* p, lua_State *L) { lua_pushnumber(L, p->GetCoursesActual(Enum::Check<StepsType>(L, 1), Enum::Check<Difficulty>(L, 2))); return 1; }
	static int GetSongsPossible(T* p, lua_State *L) { lua_pushnumber(L, p->GetSongsPossible(Enum::Check<StepsType>(L, 1), Enum::Check<Difficulty>(L, 2))); return 1; }
	static int GetCoursesPossible(T* p, lua_State *L) { lua_pushnumber(L, p->GetCoursesPossible(Enum::Check<StepsType>(L, 1), Enum::Check<Difficulty>(L, 2))); return 1; }
	static int GetSongsPercentComplete(T* p, lua_State *L) { lua_pushnumber(L, p->GetSongsPercentComplete(Enum::Check<StepsType>(L, 1), Enum::Check<Difficulty>(L, 2))); return 1; }
	static int GetCoursesPercentComplete(T* p, lua_State *L) { lua_pushnumber(L, p->GetCoursesPercentComplete(Enum::Check<StepsType>(L, 1), Enum::Check<Difficulty>(L, 2))); return 1; }
	static int GetTotalStepsWithTopGrade(T* p, lua_State *L) { lua_pushnumber(L, p->GetTotalStepsWithTopGrade(Enum::Check<StepsType>(L, 1), Enum::Check<Difficulty>(L, 2), Enum::Check<Grade>(L, 3))); return 1; }
	static int GetTotalTrailsWithTopGrade(T* p, lua_State *L) { lua_pushnumber(L, p->GetTotalTrailsWithTopGrade(Enum::Check<StepsType>(L, 1), Enum::Check<Difficulty>(L, 2), Enum::Check<Grade>(L, 3))); return 1; }
	static int GetNumTotalSongsPlayed(T* p, lua_State *L) { lua_pushnumber(L, p->m_iNumTotalSongsPlayed); return 1; }
	static int GetTotalSessions(T* p, lua_State *L) { lua_pushnumber(L, p->m_iTotalSessions); return 1; }
	static int GetTotalSessionSeconds(T* p, lua_State *L) { lua_pushnumber(L, p->m_iTotalSessionSeconds); return 1; }
	static int GetTotalGameplaySeconds(T* p, lua_State *L) { lua_pushnumber(L, p->m_iTotalGameplaySeconds); return 1; }
	static int GetSongsAndCoursesPercentCompleteAllDifficulties(T* p, lua_State *L) { lua_pushnumber(L, p->GetSongsAndCoursesPercentCompleteAllDifficulties(Enum::Check<StepsType>(L, 1))); return 1; }
	static int GetTotalCaloriesBurned(T* p, lua_State *L) { lua_pushnumber(L, p->m_fTotalCaloriesBurned); return 1; }
	static int GetDisplayTotalCaloriesBurned(T* p, lua_State *L) { lua_pushstring(L, p->GetDisplayTotalCaloriesBurned()); return 1; }
	static int GetPlayerRating(T* p, lua_State *L) { lua_pushnumber(L, p->m_fPlayerRating); return 1; }
	static int GetMostPopularSong(T* p, lua_State *L)
	{
		Song *p2 = p->GetMostPopularSong();
		if (p2)
			p2->PushSelf(L);
		else
			lua_pushnil(L);
		return 1;
	}
	static int GetMostPopularCourse(T* p, lua_State *L)
	{
		Course *p2 = p->GetMostPopularCourse();
		if (p2)
			p2->PushSelf(L);
		else
			lua_pushnil(L);
		return 1;
	}
	static int GetSongNumTimesPlayed(T* p, lua_State *L)
	{
		ASSERT(!lua_isnil(L, 1));
		Song *pS = Luna<Song>::check(L, 1);
		lua_pushnumber(L, p->GetSongNumTimesPlayed(pS));
		return 1;
	}
	static int HasPassedAnyStepsInSong(T* p, lua_State *L)
	{
		ASSERT(!lua_isnil(L, 1));
		Song *pS = Luna<Song>::check(L, 1);
		lua_pushboolean(L, p->HasPassedAnyStepsInSong(pS));
		return 1;
	}
	static int GetNumToasties(T* p, lua_State *L) { lua_pushnumber(L, p->m_iNumToasties); return 1; }
	static int GetTotalTapsAndHolds(T* p, lua_State *L) { lua_pushnumber(L, p->m_iTotalTapsAndHolds); return 1; }
	static int GetTotalJumps(T* p, lua_State *L) { lua_pushnumber(L, p->m_iTotalJumps); return 1; }
	static int GetTotalHolds(T* p, lua_State *L) { lua_pushnumber(L, p->m_iTotalHolds); return 1; }
	static int GetTotalRolls(T* p, lua_State *L) { lua_pushnumber(L, p->m_iTotalRolls); return 1; }
	static int GetTotalMines(T* p, lua_State *L) { lua_pushnumber(L, p->m_iTotalMines); return 1; }
	static int GetTotalHands(T* p, lua_State *L) { lua_pushnumber(L, p->m_iTotalHands); return 1; }
	static int GetTotalLifts(T* p, lua_State *L) { lua_pushnumber(L, p->m_iTotalLifts); return 1; }
	DEFINE_METHOD(GetTotalDancePoints, m_iTotalDancePoints);
	static int GetUserTable(T* p, lua_State *L) { p->m_UserTable.PushSelf(L); return 1; }
	static int GetNumFaves(T* p, lua_State *L) { lua_pushnumber(L, p->FavoritedCharts.size()); return 1; }
	static int GetLastPlayedSong(T* p, lua_State *L)
	{
		Song *pS = p->m_lastSong.ToSong();
		if (pS)
			pS->PushSelf(L);
		else
			lua_pushnil(L);
		return 1;
	}
	static int GetTopSSRValue(T* p, lua_State *L) {
		lua_pushnumber(L, p->GetTopSSRValue(IArg(1) - 1, IArg(2)));
		return 1;
	}
	static int 	GetTopSSRHighScore(T* p, lua_State *L) {
		HighScore* ths = p->GetTopSSRHighScore(IArg(1) - 1, IArg(2));
		if (ths)
			ths->PushSelf(L);
		else
			lua_pushnil(L);
		return 1;
	}
	static int GetPlayerSkillsetRating(T* p, lua_State *L) {
		Skillset lel = static_cast<Skillset>(IArg(1) - 1);
		lua_pushnumber(L, p->m_fPlayerSkillsets[lel]);
		return 1;
	}

	static int SortAllSSRs(T* p, lua_State *L) {
		for(size_t i = 0; i < NUM_Skillset; ++i)
			p->pscores.SortTopSSRPtrs(static_cast<Skillset>(i));
		return 1;
	}

	DEFINE_METHOD(GetGUID, m_sGuid);
	static int ValidateAllScores(T* p, lua_State *L) {
		p->ValidateAllScores();
		return 1;
	}
	
	static int GetAllGoals(T* p, lua_State *L) {
		lua_newtable(L);
		int idx = 0;
		FOREACHM(RString, vector<ScoreGoal>, p->goalmap, i) {
			const RString &ck = i->first;
			auto &sgv = i->second;
			FOREACH(ScoreGoal, sgv, sg) {
				ScoreGoal &tsg = *sg;
				tsg.chartkey = ck;
				tsg.PushSelf(L);
				lua_rawseti(L, -2, idx + 1);
				idx++;
			}	
		}
		return 1;
	}

	LunaProfile()
	{
		ADD_METHOD( AddScreenshot );
		ADD_METHOD( GetType );
		ADD_METHOD( GetPriority );
		ADD_METHOD( GetDisplayName );
		ADD_METHOD( SetDisplayName );
		ADD_METHOD( GetLastUsedHighScoreName );
		ADD_METHOD( SetLastUsedHighScoreName );
		ADD_METHOD( GetAllUsedHighScoreNames );
		ADD_METHOD( GetHighScoreListIfExists );
		ADD_METHOD( GetHighScoreList );
		ADD_METHOD( GetScoresByKey );
		ADD_METHOD( GetCategoryHighScoreList );
		ADD_METHOD( GetCharacter );
		ADD_METHOD( SetCharacter );
		ADD_METHOD( GetWeightPounds );
		ADD_METHOD( SetWeightPounds );
		ADD_METHOD( GetVoomax );
		ADD_METHOD( SetVoomax );
		ADD_METHOD( GetAge );
		ADD_METHOD( GetBirthYear );
		ADD_METHOD( SetBirthYear );
		ADD_METHOD( GetIgnoreStepCountCalories );
		ADD_METHOD( SetIgnoreStepCountCalories );
		ADD_METHOD( GetIsMale );
		ADD_METHOD( SetIsMale );
		ADD_METHOD( AddCaloriesToDailyTotal );
		ADD_METHOD( CalculateCaloriesFromHeartRate );
		ADD_METHOD( GetGoalType );
		ADD_METHOD( SetGoalType );
		ADD_METHOD( GetGoalCalories );
		ADD_METHOD( SetGoalCalories );
		ADD_METHOD( GetGoalSeconds );
		ADD_METHOD( SetGoalSeconds );
		ADD_METHOD( GetCaloriesBurnedToday );
		ADD_METHOD( GetTotalNumSongsPlayed );
		ADD_METHOD( IsCodeUnlocked );
		ADD_METHOD( GetSongsActual );
		ADD_METHOD( GetCoursesActual );
		ADD_METHOD( GetSongsPossible );
		ADD_METHOD( GetCoursesPossible );
		ADD_METHOD( GetSongsPercentComplete );
		ADD_METHOD( GetCoursesPercentComplete );
		ADD_METHOD( GetTotalStepsWithTopGrade );
		ADD_METHOD( GetTotalTrailsWithTopGrade );
		ADD_METHOD( GetNumTotalSongsPlayed );
		ADD_METHOD( GetTotalSessions );
		ADD_METHOD( GetTotalSessionSeconds );
		ADD_METHOD( GetTotalGameplaySeconds );
		ADD_METHOD( GetSongsAndCoursesPercentCompleteAllDifficulties );
		ADD_METHOD( GetTotalCaloriesBurned );
		ADD_METHOD( GetDisplayTotalCaloriesBurned );
		ADD_METHOD( GetMostPopularSong );
		ADD_METHOD( GetMostPopularCourse );
		ADD_METHOD( GetSongNumTimesPlayed );
		ADD_METHOD( HasPassedAnyStepsInSong );
		ADD_METHOD( GetNumToasties );
		ADD_METHOD( GetTotalTapsAndHolds );
		ADD_METHOD( GetTotalJumps );
		ADD_METHOD( GetTotalHolds );
		ADD_METHOD( GetTotalRolls );
		ADD_METHOD( GetTotalMines );
		ADD_METHOD( GetTotalHands );
		ADD_METHOD( GetTotalLifts );
		ADD_METHOD( GetTotalDancePoints );
		ADD_METHOD( GetUserTable );
		ADD_METHOD( GetLastPlayedSong );
		ADD_METHOD( GetGUID );
		ADD_METHOD( GetPlayerRating );
		ADD_METHOD( GetPlayerSkillsetRating );
		ADD_METHOD( GetNumFaves );
		ADD_METHOD( GetTopSSRValue );
		ADD_METHOD( GetTopSSRHighScore );
		ADD_METHOD( SortAllSSRs );
		ADD_METHOD( ValidateAllScores );
		ADD_METHOD( GetAllGoals );
	}
};

LUA_REGISTER_CLASS( Profile )
class LunaScoreGoal : public Luna<ScoreGoal>
{
public:
	DEFINE_METHOD( GetRate, rate );
	DEFINE_METHOD( GetPercent, percent );
	DEFINE_METHOD( GetPriority, priority );
	DEFINE_METHOD( IsAchieved, achieved );
	DEFINE_METHOD( GetComment, comment );
	DEFINE_METHOD( GetChartKey, chartkey);
	DEFINE_METHOD( WhenAssigned, timeassigned.GetString() );
	
	static int WhenAchieved(T* p, lua_State *L) {
		if (p->achieved)
			lua_pushstring(L, p->timeachieved.GetString());
		else
			lua_pushnil(L);

		return 1;
	}

	static int SetRate(T* p, lua_State *L) { 
		if (!p->achieved) {
			float newrate = FArg(1);
			CLAMP(newrate, 0.7f, 3.0f);
			p->rate = newrate;
			p->CheckVacuity();
		}
		
		return 1; 
	}

	static int SetPercent(T* p, lua_State *L) {
		if (!p->achieved) {
			float newpercent = FArg(1);
			CLAMP(newpercent, .8f, 1.f);
			p->percent = newpercent;
			p->CheckVacuity();
		}
		return 1;
	}

	static int SetPriority(T* p, lua_State *L) {
		if (!p->achieved) {
			int newpriority = IArg(1);
			CLAMP(newpriority, 0, 100);
			p->priority = newpriority;
		}
		return 1;
	}

	static int SetComment(T* p, lua_State *L) { p->comment = SArg(1); return 1; }

	static int Delete(T* p, lua_State *L) {
		PROFILEMAN->GetProfile(PLAYER_1)->DeleteGoal(p->chartkey, p->timeassigned);
		return 1;
	}

	static int GetPBUpTo(T* p, lua_State *L) {
		HighScore* pb = p->GetPBUpTo();
		if (!pb)
			lua_pushnil(L);
		else
			pb->PushSelf(L);
		return 1;
	}

	static int IsVacuous(T* p, lua_State *L) {
		if (p->achieved)
			lua_pushboolean(L, false);
		
		p->CheckVacuity();	// might be redundant
		lua_pushboolean(L, p->vacuous);
		return 1;
	}

	static int AchievedBy(T* p, lua_State *L) {
		if (p->achieved)
			lua_pushstring(L, p->scorekey);
		else
			lua_pushnil(L);
		return 1;
	}

	LunaScoreGoal()
	{
		ADD_METHOD( GetRate );
		ADD_METHOD( GetPercent );
		ADD_METHOD( GetPriority );
		ADD_METHOD( IsAchieved );
		ADD_METHOD( GetComment );
		ADD_METHOD( WhenAssigned );
		ADD_METHOD( WhenAchieved );
		ADD_METHOD( GetChartKey );
		ADD_METHOD( SetRate );
		ADD_METHOD( SetPercent );
		ADD_METHOD( SetPriority );
		ADD_METHOD( SetComment );
		ADD_METHOD( Delete );
		ADD_METHOD( GetPBUpTo );
		ADD_METHOD( IsVacuous );
		ADD_METHOD( AchievedBy );
	}
};
LUA_REGISTER_CLASS(ScoreGoal)
// lua end


/*
 * (c) 2001-2004 Chris Danford
 * All rights reserved.
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, and/or sell copies of the Software, and to permit persons to
 * whom the Software is furnished to do so, provided that the above
 * copyright notice(s) and this permission notice appear in all copies of
 * the Software and that both the above copyright notice(s) and this
 * permission notice appear in supporting documentation.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT OF
 * THIRD PARTY RIGHTS. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR HOLDERS
 * INCLUDED IN THIS NOTICE BE LIABLE FOR ANY CLAIM, OR ANY SPECIAL INDIRECT
 * OR CONSEQUENTIAL DAMAGES, OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 * OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
 * OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */
