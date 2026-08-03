#ifndef HEADER_LIB_AUDIO_BACKGROUNDMUSIC_HPP
#define HEADER_LIB_AUDIO_BACKGROUNDMUSIC_HPP

class CBackgroundMusic
{
public:
	CBackgroundMusic();
	~CBackgroundMusic();
	bool Play(const char* pszFilename);
	void Stop();
	bool IsPlaying() const { return m_bPlaying; }
private:
	bool m_bPlaying;
};

extern CBackgroundMusic g_BackgroundMusic;
#endif
