#ifndef HEADER_LIB_AUDIO_BACKGROUNDMUSIC_HPP
#define HEADER_LIB_AUDIO_BACKGROUNDMUSIC_HPP

// Simple looping background music player for OGG/WAV files.
// Uses stb_vorbis for OGG decoding, DirectSound for output.

class CBackgroundMusic
{
public:
	CBackgroundMusic();
	~CBackgroundMusic();

	// Load and start playing a file.  Supports .ogg (via stb_vorbis)
	// and .wav (via Windows MMIO).  Returns true on success.
	bool Play(const char* pszFilename);

	// Stop playback and free resources.
	void Stop();

	// Returns true if currently playing.
	bool IsPlaying() const { return m_bPlaying; }

private:
	bool m_bPlaying;
};

// Global instance (initialized in BackgroundMusic.cpp)
extern CBackgroundMusic g_BackgroundMusic;

#endif
