#ifndef HEADER_LIB_AUDIO_BACKGROUNDMUSIC_HPP
#define HEADER_LIB_AUDIO_BACKGROUNDMUSIC_HPP

#define MAX_PLAYLIST_TRACKS 256

class CBackgroundMusic
{
public:
	CBackgroundMusic();
	~CBackgroundMusic();
	bool Play(const char* pszFilename);
	bool PlayLevelTrack(const char* pszLevelBase);
	void Stop();
	void InnerLoopCall();
	bool IsPlaying() const { return m_bPlaying; }
private:
	bool PlayTrack(const char* pszFile, bool bLoop);
	void PlayPlaylistStart();
	void PlayNextPlaylistTrack();

	bool   m_bPlaying;
	bool   m_bPlaylist;
	char   m_aszPlaylistDir[_MAX_PATH];
	int    m_iPlaylistCount;
	int    m_iPlaylistIndex;
	int    m_iShuffleOrder[MAX_PLAYLIST_TRACKS];
	// Parsed track filenames (playlist mode)
	char   m_aszPlaylistNames[MAX_PLAYLIST_TRACKS][_MAX_PATH];
};

extern CBackgroundMusic g_BackgroundMusic;
#endif
