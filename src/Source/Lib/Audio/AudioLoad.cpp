/***********************************************************************************************
 *
 * Copyright � DreamWorks Interactive. 1997
 *
 * Contents:
 *		CCAULoad - Abstract base class for loading audio data.
 *
 *		All audio decompressors should be derived from this class and they must implement all
 *		of the pure functions that this class defines.
 *
 * Bugs:
 *
 * To do:
 *
 * Notes:
 *		The code contained within this file should be 100% thread safe, this is because
 *		streamed audio will be loaded from within a thread.
 ***********************************************************************************************
 *
 * $Log:: /JP2_PC/Source/Lib/Audio/AudioLoad.cpp                                               $
 * 
 * 31    10/06/98 2:43p Rwyatt
 * Stricly verify the TPA file when loading them
 * 
 * 30    98/09/17 18:40 Speter
 * Commented out VER_TEST section for now, as it is obviously out of date, and was asserting.
 * 
 * 29    98/09/17 17:49 Speter
 * Fixed some compile errors due to VER_TEST now being recognised.
 * 
 * 28    9/08/98 9:37p Rwyatt
 * Now lists the sample ID of samples that are not found
 * 
 * 27    8/26/98 4:45p Shernd
 * For audio databases in trespasser we don't want GENERIC_WRITE because of the CD
 * 
 * 26    8/23/98 4:31p Rwyatt
 * Initialize handle database properly and correctly log memory
 * 
 * 25    8/22/98 6:51p Pkeet
 * Added a return to avoid a crash in the 'CAudioDatabase' destructor.
 * 
 * 24    8/21/98 12:12a Rwyatt
 * If ADPCM sample is smaller than decompression buffer then we must make the buffer smaller but
 * it still must be a multiple of the sample block length,
 * 
 * 23    8/20/98 3:02p Rwyatt
 * Now calculates the correct size of the deompression buffers
 * 
 * 22    8/05/98 11:15p Rwyatt
 * Added memory logs for all loader memory
 * 
 * 21    8/04/98 4:02p Rwyatt
 * New version, CAU headers are now in the database sample map. This enables headers to be
 * obtained with disk activity.
 * File handles are all created with the database so no open file calls are required.
 * 
 * 20    7/06/98 8:22p Rwyatt
 * Added a rest function to reset the audio databses to their initial state
 * 
 * 19    6/24/98 3:24p Rwyatt
 * Assert for valid sample properties.
 * 
 * 18    6/08/98 12:54p Rwyatt
 * Audio databases can now use a static file handle. This can only be used for databses that do
 * not support multiple loads. Use this flag if possible because it is more efficient as the
 * databse file does not have to be re-opened.
 * 
 * 17    5/26/98 2:53p Rwyatt
 * Audio load is now asserts on any slight problem. If we get through the loader the Audio
 * databases must be valid.
 * 
 * 16    5/22/98 7:09p Rwyatt
 * New element in collision structure for min time delay.
 * New version number - Not backwards compatible.
 * Loader no longer loads old versions.
 * 
 * 15    5/19/98 10:14p Rwyatt
 * Moved FileSetPointer to CPP file.
 * Added support for version 0x121 of TPA file, this includes sample play times in the
 * index/indentifier list.
 * 
 * 14    5/07/98 5:44p Rwyatt
 * Incremented audio database version to 0x120. This is so we can have a identifier count in the
 * header. The identifier loader code has changed to accomodate this but old TPA files will
 * still load.
 * 
 * 13    3/22/98 5:02p Rwyatt
 * New binary audio collisions
 * New binary instance hashing with new instance naming
 * 
 * 12    3/16/98 7:26p Rwyatt
 * Don't check in a test version!
 * 
 * 11    3/16/98 7:25p Rwyatt
 * Fixed stray delete pointer that was causing a crash on exit.
 * 
 * 10    3/16/98 5:41p Rwyatt
 * Keeps the audio database closed while it is not being used.
 * 
 * 9     3/10/98 2:21p Rwyatt
 * Uses a duplicate file handle for the sample loaders when loading from a pack file. This
 * enables two streams to be read from the pack file at the same time without the file pointers
 * getting messed up.
 * 
 * 8     3/10/98 1:59a Rwyatt
 * When loading subtitles the file pointer is now adjusted by the CAU start position. This was
 * causing packed CAU files with subtitles to crash.
 * 
 * 7     3/09/98 10:54p Rwyatt
 * New class to handle sound databases.
 * Modified to read from a database file.
 * 
 * 6     2/06/98 8:19p Rwyatt
 * new member to create a subtitle class
 * 
 * 5     2/03/98 2:28p Rwyatt
 * Temp check in.
 * 
 * 4     12/17/97 2:57p Rwyatt
 * Removed debug new
 * 
 * 3     11/18/97 3:19p Rwyatt
 * If a non CAU file is loaded, the loader will return NULL. It used to Assert in debug mode.
 * 
 * 2     11/14/97 7:19p Rwyatt
 * Added memory logs
 * 
 * 1     11/13/97 11:40p Rwyatt
 * Initial implementation of the audio abstract loader. All loader classes are derived from this
 * class.
 * This class gives a consistent interface to any type of audio compression.
 * 
 ***********************************************************************************************/

// DO NOT INCLUDE COMMON.HPP IN ANY SOUND FILES...
#include "Audio.hpp"
#include "lib/sys/memorylog.hpp"

#include "AudioPCM.hpp"
#include "AudioADPCM.hpp"
#include "AudioVOICE.hpp"

#include <math.h>


//**********************************************************************************************
// setup debug new handler so we get the file and line number of leaks
//
/*#ifdef _DEBUG
void* __cdecl operator new(size_t nSize, const char* lpszFileName, int nLine);
#define DEBUG_NEW new(__FILE__, __LINE__)
#define new DEBUG_NEW
#endif*/
//	
//**************************************


extern bool bIsTrespasser;



//**********************************************************************************************
// A Local min macro
//
#define MIN(p1,p2) (p1)<(p2)?(p1):(p2)
//	
//**************************************



//**********************************************************************************************
// The constructor for a memory CAU file.
//
CCAULoad::CCAULoad
(
	SCAUHeader&	cauheader
)
//	
//**************************************
{
	MEMLOG_ADD_COUNTER(emlSoundLoader,sizeof(CCAULoad));	

	u4CAUFileBase = 0;
	hFile = NULL;
	padatSource = NULL;
	pu1Base = (uint8*)&cauheader;
	pu1Samples = pu1Base + cauheader.u4Offset;
	u4SampleOffset = 0;
	u4SampleBlockLength = cauheader.u4DataSize;
	memcpy(&cauheaderLocal,&cauheader,sizeof(SCAUHeader));

	SelectDecoder();
}



//**********************************************************************************************
// This is the file constructor which either uses direct file IO or memory maps the file into
// virtual memory. Once the file is mapped into memory it is identical to the construtor above
// but the disk based loader is radically different as it requires a block of memory that it
// can pre-load into, this intermediate block of memory is only small and although is a small
// performance hit for PCM streams it is essential for compressed streams because it enables
// the decompress functions to be detached from the data source medium.
//
CCAULoad::CCAULoad
(
	CAudioDatabase*	padat,			// database holding the CAU file or NULL for a Win32 file
	HANDLE			h_file,			// file handle
	uint32			u4_fpos,		// offset of the header in the specified file
	SCAUHeader&		cau				// the header at the above offset
)
//	
//**************************************
{
	MEMLOG_ADD_COUNTER(emlSoundLoader,sizeof(CCAULoad));	

	// Whole file based operations do not have an offset.
	u4CAUFileBase = u4_fpos;

	uint32	u4_block_size;

	padatSource = padat;

	cauheaderLocal = cau;

	hFile			= h_file;
	pu1Base			= NULL;

	u4DataChunkRemain = cauheaderLocal.u4DataSize;

	u4_block_size = cauheaderLocal.u4BlockAlignment;

	//
	// an early compression tool set the block alignment to 1 instead of zero for uncompressed
	// samples so this assert is just to catch those..
	//
	Assert(u4_block_size!=1);

	// make an optimal sized load buffer that is smaller than the maximum allowed
	if (u4_block_size == 0)
	{
		u4_block_size = AUDIO_FILE_BUFFER;
	}
	else
	{
		// how many whole blocks can fit into the maximum allowed file buffer ??, allocate
		// enough memory for the maximum buffers to reduce disk loading setup.
		uint32 u4_blocks;
		if (AUDIO_FILE_BUFFER<cauheaderLocal.u4DataSize)
		{
			u4_blocks = AUDIO_FILE_BUFFER / u4_block_size;
		}
		else
		{
			u4_blocks = cauheaderLocal.u4DataSize / u4_block_size;
		}

		// Should this be an assert??
		if (u4_blocks == 0)
			u4_blocks = 1;

		u4_block_size = u4_blocks * u4_block_size;
	}

	// the intermediate buffer MUST be smaller or the same as the sample size so clamp the
	// buffer length to this
	if (u4_block_size>cauheaderLocal.u4DataSize)
	{
		u4_block_size = cauheaderLocal.u4DataSize;
	}

	pu1Samples = new uint8[u4_block_size];

	MEMLOG_ADD_ADRSIZE(emlSoundLoader,pu1Samples);

	u4SampleBlockLength = u4_block_size;
	u4SampleOffset = 0xffffffff;

	// set the file pointer to the start of the data
	SetFilePointer(h_file,u4CAUFileBase + cauheaderLocal.u4Offset,NULL,FILE_BEGIN);
	SelectDecoder();
}



//**********************************************************************************************
// destructor needs to tidy up, close open files, remove possible memory mappings 
// and free any memory used by the loader.
//
CCAULoad::~CCAULoad
(
)
//	
//**************************************
{
	MEMLOG_SUB_COUNTER(emlSoundLoader,sizeof(CCAULoad));	

	// If we are file based we need to delete the loader memory
	if (hFile)
	{
		// delete the intermediate load buffer
		MEMLOG_SUB_ADRSIZE(emlSoundLoader,pu1Samples);	
		delete pu1Samples;

		// If this loader was created with a filehandle from a database then close it with the
		// database from which it came.
		if (padatSource)
		{
			// Call the owing database to close the file, this may close the actual file handle if
			// it is a multiple stream file or it may do nothing.
			padatSource->CloseDatabaseFile(hFile);
		}
		else
		{
			// We have no database so this must be a normal file handle so close it that way.
			CloseHandle(hFile);
		}
	}
}






//**********************************************************************************************
// Decides which of the decoding functions to use and what the silent value is. The silent value
// changes depending on the bit depth of the sample, 8 bit samples are unsigned so 128 is the
// silent value, 16 bit samples are signed so 0 is the silent value.
//
// The decoder function is pointed to by u4DecompressBlock, as the sample format cannot change
// after it is loaded is seems pointless deciding which decoder to use on a per block basis.
//
void CCAULoad::SelectDecoder
(
)
//	
//**************************************
{
	// the sample must be mono or stereo....
	Assert( (cauheaderLocal.u1Channels==1) || (cauheaderLocal.u1Channels==2) );
	// 8 or 16 bit...
	Assert( (cauheaderLocal.u1Bits==8) || (cauheaderLocal.u1Bits==16) );

	if (cauheaderLocal.u1Channels == 1)
	{
		// we are mono in some format
		if (cauheaderLocal.u1Bits == 8)
		{
			// mono 8bit
			u4DecompressBlock = u4DecompressMono8bit;
			u1SilentValue = 128;
		}
		else
		{
			// mono 16bit
			u4DecompressBlock = u4DecompressMono16bit;
			u1SilentValue = 0;
		}
	}
	else
	{
		// we are stereo....
		// we are mono in some format
		if (cauheaderLocal.u1Bits == 8)
		{
			// mono 8bit
			u4DecompressBlock = u4DecompressStereo8bit;
			u1SilentValue = 128;
		}
		else
		{
			// mono 16bit
			u4DecompressBlock = u4DecompressStereo16bit;
			u1SilentValue = 0;
		}
	}
}



//**********************************************************************************************
// This is the main abstracted loading function for audio. The caller requests the number of
// decompressed bytes required and an address to put them at, the rest is handled by this 
// function and the audio compression classes that implement the virtual functions of this
// class.
// It is this function that decides where to get the data from, the current sources of data are:
//		Disk File
//		Memory
//		Memory mapped disk file
//
uint32 CCAULoad::u4LoadSampleData
(
	uint8*		pu1_dst,
	uint32		u4_bytes,
	bool		b_looped
)
//	
//**************************************
{
	uint32	u4_remain;
	uint32	u4_decode;
	uint32	u4_loaded;
	uint32	u4_start = u4_bytes;

	do
	{
		// get the number of bytes in the dcompression buffer
		u4_remain = u4GetRemainingData();
		if (u4_remain == 0)
		{
			// there are zero bytes in the decompression buffer, so we need to pass another in.
			// if we are at the end we either need to go back to the start in the case when we
			// are looped or fill the remaining bytes with zero
			if (u4SampleOffset>=u4SampleBlockLength)
			{
				// are we a memory mapped or real memory CAU file, if we are then we are at the
				// end of the sample, if we are file based we are at the end of the current
				// block
				if (hFile == NULL)
				{
					// are we looped???
					if (b_looped)
					{
						u4SampleOffset = 0;
					}
					else
					{
						//
						// this only gets used by streamed samples when they have finised and are
						// not looped. A stream can only be stopped from within its callback so
						// if the sample data ends between 2 callbacks (which it is very likely to
						// do) then we have to fill with zeros upto the next call back when the
						// stream will be stopped.
						//
						// The actual value used for the silent areas depens on the bit depth of
						// the sample data. If the data is 8bit then data is unsigned and 128 
						// is the silent value (zero power) and if the sample is 16bit then the
						// data is signed and zero is the value to use.
						//
						memset(pu1_dst,u1SilentValue,u4_bytes);
						pu1_dst += u4_bytes;
						u4_bytes = 0;
						continue;
					}
				}
				else
				{
					ReadFile(hFile, pu1Samples, MIN(u4SampleBlockLength,u4DataChunkRemain), (DWORD*)&u4_loaded, NULL);
					u4DataChunkRemain -= u4_loaded;

					// if we did not fill the whole block then we must be at the end of the chunk, so we either have to
					// read from the start and carry on or fill with zeros
					if (u4_loaded<u4SampleBlockLength)
					{
						if (b_looped)
						{
							// set the file pointer back to the start and read the number of bytes again
							SetFilePointer(hFile,u4CAUFileBase+cauheaderLocal.u4Offset,NULL,FILE_BEGIN);
							u4DataChunkRemain = cauheaderLocal.u4DataSize;
							ReadFile(hFile, pu1Samples+u4_loaded, u4SampleBlockLength-u4_loaded, (DWORD*)&u4_loaded, NULL);
							u4DataChunkRemain -= u4_loaded;
						}
						else
						{
							// zero the end of the the intermediate buffer
							memset(pu1Samples+u4_loaded,u1SilentValue,u4SampleBlockLength-u4_loaded);
						}
					}

					u4SampleOffset = 0;
				}
			}

			Assert(u4SampleOffset<u4SampleBlockLength);

			SetSampleSource(pu1Samples + u4SampleOffset, u4SampleBlockLength - u4SampleOffset);
		}

		//
		// this-> on the line below should not be required on the pointer to function line
		// below but VC is so bad that we have to insert it......
		// This only took about an hour to figure out, 10 out of 10 to Microsoft!
		//
		u4_decode = (this->*u4DecompressBlock)(pu1_dst, u4_bytes);

		u4_bytes -= u4_decode;
		pu1_dst+=u4_decode;
		u4SampleOffset += u4_decode;

	} while (u4_bytes>0);

	return u4_start - u4_bytes;
}



//**********************************************************************************************
// Create a sub title structure for this sample, if this sample does not have subtitle text or
// is an old file version then return NULL
//
CAudioSubtitle* CCAULoad::pasubCreateSubtitle
(
)
//	
//**************************************
{
	CAudioSubtitle*	pasub = NULL;

	SSubtitleHeader	sth;
	uint32			u4_bytes;

	uint32			u4_filepos = SetFilePointer(hFile,0,NULL,FILE_CURRENT);

	// if the cau file is before subtitles then return NULL
	if (cauheaderLocal.u4Version < AUDIO_LOADER_SUBTITLE_VERSION)
		return NULL;

	// if the offset is zero then this file contains no subtitle
	if (cauheaderLocal.u4SubtitleOffset == 0)
		return NULL;

	dprintf("Sample has a subtitle...\n");

	uint8*	pu1_base;
	SetFilePointer(hFile,cauheaderLocal.u4SubtitleOffset+u4CAUFileBase,NULL,FILE_BEGIN);
	
	// read the header...
	if (ReadFile(hFile, &sth, sizeof(SSubtitleHeader), (DWORD*)&u4_bytes, NULL) == false)
		Assert(0);

	// allocate the block for the subtitle
	pu1_base = new uint8[sth.u4SubtitleLen + sizeof(uint32) ];

	// copy the header into the block...
	*(SSubtitleHeader*)pu1_base = sth;

	// read the binary subtitle sections into the block.
	if (ReadFile(hFile, pu1_base+sizeof(SSubtitleHeader), 
					sth.u4SubtitleLen + sizeof(uint32) - sizeof(SSubtitleHeader) , 
					(DWORD*)&u4_bytes, NULL) == false)
	{
		Assert(0);
	}

	// create a sub title from this memory block, the sub title is responsible for deleting the
	// memory, hence the TRUE in the construction.
	pasub = new CAudioSubtitle((SSubtitleHeader*)pu1_base,true);

	// put the file pointer back to where it came from
	SetFilePointer(hFile,u4_filepos,NULL,FILE_BEGIN);

	return pasub;
}



//**********************************************************************************************
// Static helper function to create a loader class from a CAU file in memory
//
CCAULoad* CCAULoad::pcauCreateAudioLoader
(
	SCAUHeader&	cau
)
//	
//**************************************
{
	if (cau.u4Magic != 'ROBW')
	{
		dprintf("Non CAU file (resident) for audio sample\n");
		return NULL;
	}

	Assert (cau.u4Version <= AUDIO_LOADER_VERSION);

	switch (cau.u1Compression)
	{
	case AU_COMPRESS_PCM:			// PCM Audio
		return new CAudioPCM(cau);
		break;

	case AU_COMPRESS_ADPCM:			// ADPCM Audio
		return new CAudioADPCM(cau);
		break;

	case AU_COMPRESS_VOICE:			// VOICE Audio
		Assert(0);					// Not Implemented
		break;

	default:
		Assert(0);					// unknown compression
		break;

	}
	
	return NULL;
}



//**********************************************************************************************
// Static helper function to create a loader class from a CAU file on disk.
// By default the loader class owns the file that it was created with and will close it when
// it is destroyed.
//
CCAULoad* CCAULoad::pcauCreateAudioLoader
(
	char*	str_fname			// filename to open
)
//	
//**************************************
{
	HANDLE		h_file;
	SCAUHeader	cauheader;
	uint32		u4_bytes;

	h_file = CreateFile( str_fname, GENERIC_READ, FILE_SHARE_READ, NULL,
                                OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, 0 );

	// if we failed to open the file return NULL
	if(h_file == (HANDLE)INVALID_HANDLE_VALUE )
	{
		dprintf("Open audio file '%s' failed\n",str_fname);
		return(NULL);
	}

	if (ReadFile(h_file, &cauheader, sizeof(SCAUHeader), (DWORD*)&u4_bytes, NULL) == false)
		Assert(0);

	Assert(u4_bytes == sizeof(SCAUHeader) );

	if (cauheader.u4Magic != 'ROBW')
	{
		dprintf("Non CAU file (loaded) for audio sample\n");
		CloseHandle(h_file);
		return NULL;
	}

	Assert (cauheader.u4Version <= AUDIO_LOADER_VERSION);

	switch (cauheader.u1Compression)
	{
	case AU_COMPRESS_PCM:			// PCM Audio
		return new CAudioPCM(NULL, h_file, 0, cauheader);
		break;

	case AU_COMPRESS_ADPCM:			// ADPCM Audio
		return new CAudioADPCM(NULL, h_file, 0, cauheader);
		break;

	case AU_COMPRESS_VOICE:			// VOICE Audio
		Assert(0);			// Not Implemented
		break;

	default:
		Assert(0);			// unknown compression
		CloseHandle(h_file);
		break;
	}

	CloseHandle(h_file);
	return NULL;
}




//**********************************************************************************************
// Load an uncompressed PCM .wav file through the CCAULoad interface. A synthetic CAU header
// is built from the WAV header so the existing PCM decoder can stream the file.
//
CCAULoad* pcauCreateWavAudioLoader
(
	char*	str_fname
)
//**************************************
{
	struct SWavChunk
	{
		uint32	u4Id;
		uint32	u4Size;
	};

	struct SWavFmt
	{
		uint16	u1Format;
		uint16	u1Channels;
		uint32	u4Frequency;
		uint32	u4ByteRate;
		uint16	u1BlockAlign;
		uint16	u1Bits;
	};

	HANDLE	h_file = CreateFile( str_fname, GENERIC_READ, FILE_SHARE_READ, NULL,
                                OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, 0 );

	if (h_file == INVALID_HANDLE_VALUE)
	{
		dprintf("Open audio file '%s' failed\n", str_fname);
		return NULL;
	}

	uint32	u4_bytes;
	uint32	u4_riff;
	uint32	u4_size;
	uint32	u4_wave;

	if (ReadFile(h_file, &u4_riff, 4, (DWORD*)&u4_bytes, NULL) == false || u4_bytes != 4 ||
		ReadFile(h_file, &u4_size, 4, (DWORD*)&u4_bytes, NULL) == false || u4_bytes != 4 ||
		ReadFile(h_file, &u4_wave, 4, (DWORD*)&u4_bytes, NULL) == false || u4_bytes != 4)
	{
		dprintf("WAV header read failed (loaded) for '%s'\n", str_fname);
		CloseHandle(h_file);
		return NULL;
	}

	// Note: the multi-char literal 'RIFF' puts the first character in the high byte
	// in VC6, so little endian file reads must be compared against explicit constants.
	if (u4_riff != 0x46464952 || u4_wave != 0x45564157)
	{
		dprintf("Non RIFF/WAVE file (loaded) for '%s' (riff=%08X size=%08X wave=%08X)\n",
				str_fname, u4_riff, u4_size, u4_wave);
		CloseHandle(h_file);
		return NULL;
	}

	uint16	u1_format		= 0;
	uint16	u1_channels		= 0;
	uint32	u4_freq			= 0;
	uint16	u1_bits			= 0;
	uint16	u1_block_align	= 0;
	uint32	u4_fact_samples	= 0;
	uint32	u4_data_offset	= 0;
	uint32	u4_data_size	= 0;

	while (u4_data_offset == 0)
	{
		SWavChunk	wch;

		if (ReadFile(h_file, &wch, sizeof(wch), (DWORD*)&u4_bytes, NULL) == false || u4_bytes != sizeof(wch))
		{
			dprintf("WAV chunk header read failed (loaded) for '%s'\n", str_fname);
			CloseHandle(h_file);
			return NULL;
		}

		if (wch.u4Id == 0x20746D66)
		{
			SWavFmt	wfmt;

			if (wch.u4Size < sizeof(wfmt) ||
				ReadFile(h_file, &wfmt, sizeof(wfmt), (DWORD*)&u4_bytes, NULL) == false || u4_bytes != sizeof(wfmt))
			{
				dprintf("WAV fmt chunk read failed (loaded) for '%s'\n", str_fname);
				CloseHandle(h_file);
				return NULL;
			}

			u1_format		= wfmt.u1Format;
			u1_channels		= wfmt.u1Channels;
			u4_freq			= wfmt.u4Frequency;
			u1_bits			= wfmt.u1Bits;
			u1_block_align	= wfmt.u1BlockAlign;

			// skip any remaining fmt bytes
			if (wch.u4Size > sizeof(wfmt))
			{
				SetFilePointer(h_file, wch.u4Size - sizeof(wfmt), NULL, FILE_CURRENT);
			}
		}
		else if (wch.u4Id == 0x61746164)
		{
			u4_data_offset	= SetFilePointer(h_file, 0, NULL, FILE_CURRENT);
			u4_data_size	= wch.u4Size;
		}
		else
		{
			if (wch.u4Id == 0x74636166 && wch.u4Size >= 4)
			{
				// sample count per channel for compressed formats
				ReadFile(h_file, &u4_fact_samples, 4, (DWORD*)&u4_bytes, NULL);

				if (wch.u4Size > 4)
				{
					SetFilePointer(h_file, wch.u4Size - 4, NULL, FILE_CURRENT);
				}
			}
			else
			{
				SetFilePointer(h_file, wch.u4Size, NULL, FILE_CURRENT);
			}
		}
	}

	bool	b_pcm	= (u1_format == 1) && (u1_bits == 8 || u1_bits == 16);
	bool	b_adpcm	= (u1_format == 0x11) && (u1_block_align > 0);

	if ((!b_pcm && !b_adpcm) || u4_data_offset == 0 || u4_data_size == 0 || u1_channels == 0)
	{
		dprintf("Unsupported WAV format (loaded) for '%s' (fmt=%i bits=%i ch=%i)\n",
				str_fname, (int)u1_format, (int)u1_bits, (int)u1_channels);
		CloseHandle(h_file);
		return NULL;
	}

	SCAUHeader	cau;
	memset(&cau, 0, sizeof(cau));

	cau.u4Magic			= 'ROBW';
	cau.u4Version		= 100;
	cau.u4Offset		= u4_data_offset;
	cau.u4DataSize		= u4_data_size;
	cau.u4Frequency		= u4_freq;
	cau.u1Channels		= (uint8)u1_channels;

	if (b_adpcm)
	{
		// IMA ADPCM WAV: decoded through the game's existing ADPCM decoder.
		// The output of the decoder is 16 bit.
		cau.u4BlockAlignment	= u1_block_align;
		cau.u1Bits				= 16;
		cau.u1Compression		= AU_COMPRESS_ADPCM;

		// decompressed size: fact chunk if present, otherwise estimate from the
		// number of blocks. Each block has a 4 byte header per channel.
		if (u4_fact_samples != 0)
		{
			cau.u4DecompressedSize = u4_fact_samples * u1_channels * 2;
		}
		else
		{
			uint32	u4_samp_per_block	= ((u1_block_align / u1_channels - 4) * 2) + 1;
			uint32	u4_blocks			= u4_data_size / u1_block_align;

			cau.u4DecompressedSize = u4_blocks * u4_samp_per_block * u1_channels * 2;
		}

		dprintf("WAV loaded OK (ADPCM) '%s' (%i Hz, %i ch, block=%i)\n",
				str_fname, (int)u4_freq, (int)u1_channels, (int)u1_block_align);

		return new CAudioADPCM(NULL, h_file, 0, cau);
	}

	cau.u4BlockAlignment	= 0;					// raw PCM
	cau.u4DecompressedSize	= u4_data_size;
	cau.u1Bits				= (uint8)u1_bits;
	cau.u1Compression		= AU_COMPRESS_PCM;

	dprintf("WAV loaded OK '%s' (%i Hz, %i ch, %i bit)\n",
			str_fname, (int)u4_freq, (int)u1_channels, (int)u1_bits);

	return new CAudioPCM(NULL, h_file, 0, cau);
}


//**********************************************************************************************
// CAudioADX streams CRI ADX files (16 bit only) through the CCAULoad interface. ADX is the
// Dreamcast/Saturn ADPCM variant used by the modding community; the codec itself was ported
// from the background music player.
//
class CAudioADX : public CCAULoad
{
public:
	CAudioADX
	(
		CAudioDatabase*	padat,
		HANDLE			h_file,
		uint32			u4_fpos,
		SCAUHeader&		cau,
		int32			i4_coeff1,
		int32			i4_coeff2
	) : CCAULoad(padat, h_file, u4_fpos, cau)
	{
		u4SourceDataSize	= 0;
		pu1SourceData		= NULL;
		u4FreshBytes		= 0;
		pu1Next				= NULL;
		u4BlockSize			= 0;

		i4Coeff1 = i4_coeff1;
		i4Coeff2 = i4_coeff2;

		s1[0] = s1[1] = 0;
		s2[0] = s2[1] = 0;

		// one decode unit is a frame group: one 18 byte frame per channel,
		// producing 32 interleaved 16 bit samples per channel.
		u4DecompressedBlockSize = 32 * cauheaderLocal.u1Channels * 2;

		pu1BlockBuffer = new uint8[u4DecompressedBlockSize + 32];
		MEMLOG_ADD_COUNTER(emlSoundLoader, u4DecompressedBlockSize + 32);
	}

	~CAudioADX()
	{
		MEMLOG_SUB_COUNTER(emlSoundLoader, u4DecompressedBlockSize + 32);
		delete pu1BlockBuffer;
	}

	virtual uint32 u4DecompressMono8bit(uint8* pu1_dst, uint32 u4_byte_count)
	{
		Assert(0);
		return 0;
	}

	virtual uint32 u4DecompressStereo8bit(uint8* pu1_dst, uint32 u4_byte_count)
	{
		Assert(0);
		return 0;
	}

	virtual uint32 u4DecompressMono16bit(uint8* pu1_dst, uint32 u4_byte_count)
	{
		return u4Decompress16bit(pu1_dst, u4_byte_count, 1);
	}

	virtual uint32 u4DecompressStereo16bit(uint8* pu1_dst, uint32 u4_byte_count)
	{
		return u4Decompress16bit(pu1_dst, u4_byte_count, 2);
	}

	virtual void SetSampleSource(uint8* pu1_src, uint32 u4_src_len)
	{
		pu1SourceData		= pu1_src;
		u4SourceDataSize	= u4_src_len;
		u4BlockSize			= min(u4_src_len, cauheaderLocal.u4BlockAlignment);

		// a zero length source is used to reset the loader to the start of the sample
		if (u4_src_len == 0)
		{
			s1[0] = s1[1] = 0;
			s2[0] = s2[1] = 0;
		}
	}

	virtual uint32 u4GetRemainingData()
	{
		return u4SourceDataSize;
	}

protected:
	//******************************************************************************************
	// Decode one frame (18 bytes) of one channel into 32 samples.
	uint32 u4DecodeFrame(uint8* pu1_src, uint32 u4_channel, short* ps_dst)
	{
		int32	i4_scale = ((pu1_src[0] << 8) | pu1_src[1]) & 0x1FFF;
		int32	i4_s1 = s1[u4_channel];
		int32	i4_s2 = s2[u4_channel];
		uint32	u4;

		for (u4 = 2; u4 < 18; u4++)
		{
			int32	i4_b = pu1_src[u4];
			int32	i4_d;
			int32	i4_s0;

			// high nibble first
			i4_d = i4_b >> 4;
			if (i4_d & 8) i4_d -= 16;
			i4_s0 = i4_d * i4_scale + ((i4Coeff1 * i4_s1 + i4Coeff2 * i4_s2) >> 14);
			if (i4_s0 > 32767) i4_s0 = 32767;
			else if (i4_s0 < -32768) i4_s0 = -32768;
			*ps_dst++ = (short)i4_s0;
			i4_s2 = i4_s1;
			i4_s1 = i4_s0;

			// low nibble
			i4_d = i4_b & 15;
			if (i4_d & 8) i4_d -= 16;
			i4_s0 = i4_d * i4_scale + ((i4Coeff1 * i4_s1 + i4Coeff2 * i4_s2) >> 14);
			if (i4_s0 > 32767) i4_s0 = 32767;
			else if (i4_s0 < -32768) i4_s0 = -32768;
			*ps_dst++ = (short)i4_s0;
			i4_s2 = i4_s1;
			i4_s1 = i4_s0;
		}

		s1[u4_channel] = i4_s1;
		s2[u4_channel] = i4_s2;

		return 32;
	}

	//******************************************************************************************
	uint32 u4Decompress16bit(uint8* pu1_dst, uint32 u4_byte_count, uint32 u4_channels)
	{
		uint32	u4_count;
		uint32	u4_total = 0;

		while ((u4_byte_count) && (u4SourceDataSize > 0))
		{
			if (u4FreshBytes == 0)
			{
				// decode one frame group (one frame per channel) into the block buffer
				if (u4BlockSize >= 18 * u4_channels)
				{
					short*	ps_dst = (short*)pu1BlockBuffer;
					uint32	u4;

					for (u4 = 0; u4 < u4_channels; u4++)
					{
						short	as_tmp[32];

						u4DecodeFrame(pu1SourceData + u4 * 18, u4, as_tmp);

						uint32	u4_smp;

						for (u4_smp = 0; u4_smp < 32; u4_smp++)
						{
							ps_dst[u4_smp * u4_channels + u4] = as_tmp[u4_smp];
						}
					}

					u4FreshBytes = 32 * u4_channels * 2;
				}
				else
				{
					// partial frame group at the very end of the sample, ignore it
					u4FreshBytes = 0;
				}

				u4SourceDataSize -= u4BlockSize;
				pu1SourceData += u4BlockSize;

				pu1Next = pu1BlockBuffer;
			}

			if (u4FreshBytes == 0)
			{
				break;
			}

			u4_count = min(u4_byte_count, u4FreshBytes);

			memcpy(pu1_dst, pu1Next, u4_count);
			pu1_dst += u4_count;
			pu1Next += u4_count;

			u4FreshBytes -= u4_count;
			u4_byte_count -= u4_count;

			u4_total += u4_count;
		}

		return u4_total;
	}

	uint32		u4SourceDataSize;
	uint8*		pu1SourceData;
	uint32		u4BlockSize;
	uint32		u4FreshBytes;
	uint8*		pu1Next;
	uint8*		pu1BlockBuffer;
	uint32		u4DecompressedBlockSize;

	int32		i4Coeff1;
	int32		i4Coeff2;
	int32		s1[2];
	int32		s2[2];
};


//**********************************************************************************************
// Load a CRI ADX file through the CCAULoad interface. Only 16 bit ADX is supported which is
// the format used for Trespasser mods.
//
CCAULoad* pcauCreateAdxAudioLoader
(
	char*	str_fname
)
//**************************************
{
	HANDLE	h_file = CreateFile( str_fname, GENERIC_READ, FILE_SHARE_READ, NULL,
                                OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, 0 );

	if (h_file == INVALID_HANDLE_VALUE)
	{
		dprintf("Open audio file '%s' failed\n", str_fname);
		return NULL;
	}

	uint8	au1_header[0x14];
	DWORD	u4_bytes;

	if (ReadFile(h_file, au1_header, 0x14, &u4_bytes, NULL) == false || u4_bytes != 0x14)
	{
		CloseHandle(h_file);
		return NULL;
	}

	// ADX header: 0x80 0x00 at the start, big endian fields
	if (au1_header[0] != 0x80)
	{
		dprintf("Non ADX file (loaded) for audio sample\n");
		CloseHandle(h_file);
		return NULL;
	}

	uint32	u4_copy_off	= (au1_header[2] << 8) | au1_header[3];
	uint8	u1_enc_type	= au1_header[4];
	uint8	u1_blk_size	= au1_header[5];
	uint8	u1_channels	= au1_header[7];
	uint32	u4_freq		= (au1_header[8] << 24) | (au1_header[9] << 16) | (au1_header[10] << 8) | au1_header[11];
	uint32	u4_samples	= (au1_header[12] << 24) | (au1_header[13] << 16) | (au1_header[14] << 8) | au1_header[15];
	int32	i4_highpass	= (au1_header[0x10] << 8) | au1_header[0x11];

	if (u1_channels == 0 || u1_channels > 2 || u1_blk_size != 18)
	{
		dprintf("Unsupported ADX format (loaded) for audio sample\n");
		CloseHandle(h_file);
		return NULL;
	}

	// validate the copyright string, it starts 2 bytes before the copy offset
	if (u4_copy_off < 6 || u4_copy_off > 0x100)
	{
		CloseHandle(h_file);
		return NULL;
	}

	SetFilePointer(h_file, u4_copy_off - 2, NULL, FILE_BEGIN);

	uint8	au1_copy[8];

	if (ReadFile(h_file, au1_copy, 8, &u4_bytes, NULL) == false || u4_bytes != 8 ||
		memcmp(au1_copy, "(c)CRI", 6) != 0)
	{
		dprintf("ADX copyright missing (loaded) for audio sample\n");
		CloseHandle(h_file);
		return NULL;
	}

	// audio data starts after the copyright block
	uint32	u4_data_offset	= u4_copy_off + 4;
	uint32	u4_file_size	= SetFilePointer(h_file, 0, NULL, FILE_END);
	uint32	u4_block_align	= 18 * u1_channels;

	if (u4_data_offset >= u4_file_size)
	{
		CloseHandle(h_file);
		return NULL;
	}

	uint32	u4_data_size = ((u4_file_size - u4_data_offset) / u4_block_align) * u4_block_align;

	// prediction coefficients
	int32	i4_c1;
	int32	i4_c2;

	if (u1_enc_type == 3)
	{
		// standard ADX: compute from the highpass frequency
		double	d_a		= 1.41421356237 - cos(6.28318530718 * (double)i4_highpass / 44100.0);
		double	d_b		= 0.41421356237;
		double	d_cc	= (d_a - sqrt((d_a + d_b) * (d_a - d_b))) / d_b;

		i4_c1 = (int32)(d_cc * 2.0 * 16384.0);
		i4_c2 = (int32)(-(d_cc * d_cc) * 16384.0);
	}
	else
	{
		// type 2: no prediction
		i4_c1 = 0;
		i4_c2 = 0;
	}

	SCAUHeader	cau;
	memset(&cau, 0, sizeof(cau));

	// some encoders write an inflated total sample count, clamp it to the actual
	// number of frames in the file
	uint32	u4_frame_samples = (u4_data_size / u4_block_align) * 32;
	uint32	u4_dec_samples  = (u4_samples != 0 && u4_samples < u4_frame_samples) ? u4_samples : u4_frame_samples;

	cau.u4Magic				= 'ROBW';
	cau.u4Version			= 100;
	cau.u4Offset			= u4_data_offset;
	cau.u4BlockAlignment	= u4_block_align;
	cau.u4DataSize			= u4_data_size;
	cau.u4DecompressedSize	= u4_dec_samples * u1_channels * 2;
	cau.u4Frequency			= u4_freq;
	cau.u1Bits				= 16;
	cau.u1Channels			= u1_channels;
	cau.u1Compression		= AU_COMPRESS_ADPCM;

	dprintf("ADX loaded OK '%s' (%i Hz, %i ch)\n",
			str_fname, (int)u4_freq, (int)u1_channels);

	return new CAudioADX(NULL, h_file, 0, cau, i4_c1, i4_c2);
}


//**********************************************************************************************
// Static helper function to create a loader class from a CAU file within in a packed file.
// By default loader classes own the file that they are created with and will close it when the
// loader is destroyed. For packed file loads we must clear the file owenership otherwise we 
// will close the database.
//
CCAULoad* CCAULoad::pcauCreateAudioLoader
(
	CAudioDatabase*		padat,
	TSoundHandle		sndhnd
)
//
//**************************************
{
	MEMLOG_ADD_COUNTER(emlSoundLoader,sizeof(CCAULoad));

	// there is no audio loader..
	if (padat == NULL)
		return NULL;

	//
	// Override directory support: if a loose file exists in override\<pack name>\ for
	// this sample hash, load it instead of the packed sample. Supported formats:
	// .cau, .wav (uncompressed PCM) and .adx (CRI ADPCM).
	//
	const char*	pstr_override = padat->pstrFindOverride(sndhnd);

	if (pstr_override)
	{
		dprintf("Override hit '%s' for sample %08X\n", pstr_override, (uint32)sndhnd);

		char*		pstr_path = (char*)pstr_override;
		char*		pstr_ext  = strrchr(pstr_path, '.');

		if (pstr_ext)
		{
			if (stricmp(pstr_ext, ".cau") == 0)
			{
				return pcauCreateAudioLoader(pstr_path);
			}
			else if (stricmp(pstr_ext, ".wav") == 0)
			{
				return pcauCreateWavAudioLoader(pstr_path);
			}
			else if (stricmp(pstr_ext, ".adx") == 0)
			{
				return pcauCreateAdxAudioLoader(pstr_path);
			}
		}
	}

	SSampleFile*	psf_sample = padat->psfFindSample(sndhnd);

	if ( psf_sample == NULL)
	{
		// we have not found the requested sample so complain
		dprintf("%d Identifier not found in pack file\n", (uint32)sndhnd);

		// we have not found the sample in this database, play the annoying missing sample
		psf_sample = padat->psfFindSample(padat->sndhndGetMissingID());
		if (psf_sample == NULL)
		{
			return NULL;
		}
	}

	// if we fail to duplicate the database handle, return NULL
	HANDLE h_local = padat->hDuplicateDatabaseFileHandle();
	if (h_local == NULL)
		return NULL;

	CCAULoad* pcau = NULL;

	switch (psf_sample->cauheaderIndex.u1Compression)
	{
	case AU_COMPRESS_PCM:			// PCM Audio
		pcau = new CAudioPCM(padat, h_local, psf_sample->u4CAUStart, psf_sample->cauheaderIndex);
		break;

	case AU_COMPRESS_ADPCM:			// ADPCM Audio
		pcau = new CAudioADPCM(padat, h_local, psf_sample->u4CAUStart, psf_sample->cauheaderIndex);
		break;

	case AU_COMPRESS_VOICE:			// VOICE Audio
		Assert(0);			// Not Implemented
		break;

	default:
		Assert(0);			// unknown compression
		break;
	}

	return pcau;
}


//**********************************************************************************************
void CCAULoad::ResetToStartPosition
(
)
//**************************************
{
	if (hFile)
	{
		SetFilePointer(hFile,u4CAUFileBase+cauheaderLocal.u4Offset,NULL,FILE_BEGIN);
	}
	SetSampleSource(pu1Samples,0);
	u4SampleOffset = 0;
}











//**********************************************************************************************
//
CAudioDatabase::CAudioDatabase
(
	const char*		str_filename,
	uint32			u4_handle_count		// 0 for a single shared handle
)
//**************************************
{
	uint32	u4_bytes;
	uint32	u4;
	HANDLE	h_file;
	uint32	u4_sample_bytes;
	uint32	u4_tpa_len;
    uint32  u4_access;

	Assert(str_filename);
	Assert(strlen(str_filename)<u4DATABASE_FILE_LEN);

	psfIdentifiers = NULL;
	pchCollisions = NULL;
	acolCollisions = NULL;
	SFileCollision * pfcol = NULL;
	hDatabase = NULL;		// there is no static database handle
	aahHandles = NULL;

    u4_access = GENERIC_READ;
    if (!bIsTrespasser)
    {
        u4_access |= GENERIC_WRITE;
    }

	h_file = CreateFile( str_filename, u4_access, FILE_SHARE_READ|FILE_SHARE_WRITE, NULL,
                                OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, 0 );

	if (h_file==INVALID_HANDLE_VALUE)
	{
		dprintf("Failed to open packed audio file: '%s' 'Error = %i'\n",str_filename, GetLastError());
		return;
	}

	// get the file size of the TPA file and assert that it is valid
	u4_tpa_len = GetFileSize(h_file,NULL);
	Assert(u4_tpa_len>0);

	//
	// Read the header
	//
	if (ReadFile(h_file,&pahHeader,sizeof(SPackedAudioHeader),(DWORD*)&u4_bytes, NULL) == false)
	{
		goto error;
	}

	if (u4_bytes!=sizeof(SPackedAudioHeader))
		goto error;

	// If the disk file version is greater than the version this build knows about the do not
	// attempt to load it. We can still load old version though.
	// We have to do this because we have no idea as to what has been modified
	if (pahHeader.u4Version>u4PACKED_AUDIO_VERSION)
	{
		dprintf("TPA Version is greater than the current loader (%s)\n", str_filename);
		goto error;
	}


	// Check if this file is before the minimum version we can load
	if (pahHeader.u4Version<u4MINLOAD_AUDIO_VERSION)
	{
		dprintf("TPA Version is too old for the current loader (%s)\n", str_filename);
		goto error;
	}

	//
	// Add all the sample identifiers to the database so we can find them very quickly.
	//
	// In TPA files beofre version 0x120 there was a bug. The number of actual sample
	// identifiers was not saved and the number of samples was used ofr the count. This
	// is not the correct thing to do because samples can be instanced and in theory 
	// you could have a single sample and 20 identifiers pointing to it. In this case
	// only the first identifier would have been loaded.
	//
	uint32	u4_id_count;

	u4_id_count = pahHeader.u4Identifiers;

	//
	//Allocate memory and read in the identifiers
	//
	psfIdentifiers = new SSampleFile[u4_id_count];
	memset(psfIdentifiers,0,u4_id_count*sizeof(SSampleFile));

	MEMLOG_ADD_COUNTER(emlSoundControl,u4_id_count*sizeof(SSampleFile));

	u4_sample_bytes = u4_id_count*sizeof(SSampleFile);
	SetFilePointer(h_file, pahHeader.u4SampleFileOffset, NULL, FILE_BEGIN);

	// All files pointers must be before the end of the file
	Assert(pahHeader.u4SampleFileOffset<u4_tpa_len);

	if (ReadFile(h_file,psfIdentifiers,u4_sample_bytes,
			(DWORD*)&u4_bytes, NULL) == false)
	{
		goto error;
	}

	if (u4_bytes != u4_sample_bytes)
		goto error;

	for (u4=0;u4<u4_id_count;u4++)
	{
		// Validate that the sample identifiers are loading proprtly
		Assert(psfFindSample(psfIdentifiers[u4].u4Hash) == NULL);	// this identifier must not already exist
		Assert(psfIdentifiers[u4].u4CAUStart<u4_tpa_len);			// sample must start before the end of the file
		Assert(psfIdentifiers[u4].fMasterVolume<=0.0f);				// Master volume is in dBs
		Assert(psfIdentifiers[u4].fAttenuation>=0.0f);				// attenuation is +ve..This is wrong but it is the way it is.
		Assert(psfIdentifiers[u4].cauheaderIndex.u4Magic == 'ROBW');// make sure the CAU header is valid

		AddSample(&psfIdentifiers[u4]);

		Assert(psfFindSample(psfIdentifiers[u4].u4Hash));			// the identifer must now exist

		/*if ((psfIdentifiers[u4].cauheaderIndex.u4DataSize < 1028) && (psfIdentifiers[u4].cauheaderIndex.u1Compression!=0))
		{
			dprintf("Sample ID = %x needs to be uncompressed.\n", psfIdentifiers[u4].u4Hash);
		}*/
	}

	//
	// Null out the collision pointers in case the file has no collision as we do not
	// want any stray pointers.
	//
	pchCollisions = NULL;
	acolCollisions = NULL;

	if (pahHeader.u4Collisions)
	{
		pchCollisions = new TCollisionHash;
		acolCollisions = new  SAudioCollision[pahHeader.u4Collisions];

		MEMLOG_ADD_COUNTER(emlSoundControl,sizeof(TCollisionHash));
		MEMLOG_ADD_COUNTER(emlSoundControl,sizeof(SAudioCollision)*pahHeader.u4Collisions);

		// This file has binary collisions
		pfcol = new SFileCollision[pahHeader.u4Collisions];

		//
		// Read the collisions in a single operation
		//
		SetFilePointer(h_file, pahHeader.u4CollisionFileOffset, NULL, FILE_BEGIN);
		// All files pointers must be before the end of the file
		Assert(pahHeader.u4CollisionFileOffset<u4_tpa_len);

		if (ReadFile(h_file,pfcol,pahHeader.u4Collisions*sizeof(SFileCollision),
				(DWORD*)&u4_bytes, NULL) == false)
		{
			goto error;
		}

		if (u4_bytes != pahHeader.u4Collisions*sizeof(SFileCollision))
			goto error;

		//
		// Now copy the file collisions into the collison array and add them
		// to the map.
		//
		for (u4 = 0; u4<pahHeader.u4Collisions; u4++)
		{
			// copy the structure from the file collision to the memory collision
			acolCollisions[u4] = pfcol[u4].colCollision;

#if VER_TEST
			Assert( memcmp(&acolCollisions[u4], &pfcol[u4].colCollision, sizeof(SAudioCollision)) == 0);
			Assert( pcolFindCollision(pfcol[u4].u8Key) == NULL);		// Collision does not already exist

			Assert( pfcol[u4].colCollision.u4SampleFlags != 0 );	// Sample flags cannot be zero

			// do we have collision 1??
			if (pfcol[u4].colCollision.u4CollisionSamples() > 0)
			{
				// Sample 0 must have an identifer
				Assert(pfcol[u4].colCollision.sndhndSamples[0]!=0);
				// Sample 0 identifier must exist in the sample index
				Assert(psfFindSample( pfcol[u4].colCollision.sndhndSamples[0] ));
			}

			// do we have collision 2, in this case we must have collision 1??
			if (pfcol[u4].colCollision.u4CollisionSamples() > 1)
			{
				// Sample 0 identifier must exist in the sample index
				Assert(psfFindSample( pfcol[u4].colCollision.sndhndSamples[0] ));

				// Sample 1 must have an identifer
				Assert(pfcol[u4].colCollision.sndhndSamples[1]!=0);
				// Sample 1 identifier must exist in the sample index
				Assert(psfFindSample( pfcol[u4].colCollision.sndhndSamples[1] ));
			}

			if (pfcol[u4].colCollision.bSlide())
			{
				// Slide sample must have an identifer
				Assert(pfcol[u4].colCollision.sndhndSamples[2]!=0);
				// Slide sample identifier must exist in the sample index
				Assert(psfFindSample( pfcol[u4].colCollision.sndhndSamples[2] ));
			}

			Assert(pfcol[u4].colCollision.fMinTimeDelay>=0.0f);
			Assert(pfcol[u4].colCollision.fTimeLastUsed == 0.0f);
#endif // VER_TEST

			AddCollision(pfcol[u4].u8Key, &acolCollisions[u4]);

			Assert( pcolFindCollision(pfcol[u4].u8Key) );				// Collision should now exist
		}
		
		// Delete the block of memory that holds the file collisions.
		if (pfcol)
			delete pfcol;
	}

	//
	// Set the identifier that will be used for missing samples
	//
	sndhndMissing = sndhndHashIdentifier("MISSING");

	if (u4_handle_count == 0)
	{
		// Keep the current file handle and use it for all accesses
		hDatabase = h_file;
	}
	else
	{
		hDatabase = NULL;		// there is no static database handle
		aahHandles = new SAudioHandle[u4_handle_count];
		MEMLOG_ADD_COUNTER(emlSoundControl,sizeof(SAudioHandle)*u4_handle_count);
		u4HandleCount = u4_handle_count;
		memset(aahHandles, 0, sizeof(SAudioHandle)*u4_handle_count);

		aahHandles[0].hFile = h_file;
		aahHandles[0].bInUse = false;

		for (uint32 u4=1; u4<u4_handle_count; u4++)
		{
			aahHandles[u4].hFile = CreateFile( str_filename, GENERIC_READ|GENERIC_WRITE, FILE_SHARE_READ|FILE_SHARE_WRITE, NULL,
                                OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, 0 );
		}
	}

	//
	// Store the packed file's base name (without path or extension) so that we can
	// look for loose override files in the override directory.
	//
	StoreBaseName(str_filename);

	ScanOverrideDirectory();

	return;

error:
	CloseHandle(h_file);
	hDatabase = NULL;
	aahHandles = NULL;
	if (psfIdentifiers)
	{
		delete[] psfIdentifiers;
		psfIdentifiers = NULL;
	}

	if (pfcol)
		delete pfcol;

	if (pchCollisions)
	{
		delete	pchCollisions;
		pchCollisions = NULL;
	}

	if (acolCollisions)
	{
		delete[] acolCollisions;
		acolCollisions = NULL;
	}
}


//**********************************************************************************************
//
CAudioDatabase::~CAudioDatabase
(
)
//**************************************
{
	MEMLOG_SUB_ADRSIZE(emlSoundControl,aahHandles);
	MEMLOG_SUB_ADRSIZE(emlSoundControl,psfIdentifiers);
	MEMLOG_SUB_ADRSIZE(emlSoundControl,pchCollisions);
	MEMLOG_SUB_ADRSIZE(emlSoundControl,acolCollisions);

	// Delete the identifiers for this database
	if (psfIdentifiers)
	{
		delete[] psfIdentifiers;
		psfIdentifiers = NULL;
	}

	// Delete the collision map
	if (pchCollisions)
	{
		delete	pchCollisions;
		pchCollisions = NULL;
	}

	// Delete the collision data
	if (acolCollisions)
	{
		delete[] acolCollisions;
		acolCollisions = NULL;
	}

	// If we have a database file then close it
	if (hDatabase)
	{
		CloseHandle(hDatabase);
		hDatabase = NULL;
	}
	else
	{
		if (aahHandles)
		{
			// No datbase handle so we must have an array of pre-allocated handles...
			for (uint32 u4=0; u4<u4HandleCount; u4++)
			{
				// no audio file should be in use when we get to here..
				Assert(aahHandles[u4].bInUse == false);

				CloseHandle(aahHandles[u4].hFile);
			}
			delete[] aahHandles;
		}
		aahHandles = NULL;
	}

	// Free the override directory paths.
	TOverrideHash::iterator it;

	for (it = ohOverrides.begin(); it != ohOverrides.end(); ++it)
	{
		delete[] (*it).second;
	}

	for (it = ohSrtOverrides.begin(); it != ohSrtOverrides.end(); ++it)
	{
		delete[] (*it).second;
	}
}



//**********************************************************************************************
//
HANDLE CAudioDatabase::hDuplicateDatabaseFileHandle
(
)
//**************************************
{
	// If the database handle is set then we use this handle for all access.
	if (hDatabase)
	{
		return hDatabase;
	}

	if (aahHandles == NULL)
		return NULL;

	// Go through our pre-allocated array of file handles looking for an unused handle
	for (uint32 u4 = 0; u4< u4HandleCount; u4++)
	{
		if (aahHandles[u4].bInUse == false)
		{
			aahHandles[u4].bInUse = true;
			return aahHandles[u4].hFile;
		}
	}

	return NULL;
}


//**********************************************************************************************
//
void CAudioDatabase::CloseDatabaseFile
(
	HANDLE h_file
)
//**************************************
{
	Assert(h_file);

	// if have a handle for the database file we are using just this file so do not close it.
	if (hDatabase)
		return;

	// Go through our pre-allocated array of file handles looking for an unused handle
	for (uint32 u4 = 0; u4< u4HandleCount; u4++)
	{
		if (aahHandles[u4].hFile == h_file)
		{
			aahHandles[u4].bInUse = false;
			return;
		}
	}

	// Handle does not belong to this database.......
	Assert(0);
}


//**********************************************************************************************
// Store the packed file's base name (without path or extension) in strBaseName so that the
// database can look for loose override files in the override directory.
//
void CAudioDatabase::StoreBaseName
(
	const char*	str_filename
)
//**************************************
{
	strBaseName[0] = 0;

	const char* pstr_base = str_filename + strlen(str_filename);

	while (pstr_base > str_filename && pstr_base[-1] != '\\' && pstr_base[-1] != '/' && pstr_base[-1] != ':')
	{
		pstr_base--;
	}

	uint32 u4_base_len = 0;

	while (pstr_base[u4_base_len] != 0 && pstr_base[u4_base_len] != '.')
	{
		u4_base_len++;
	}

	if (u4_base_len > 0 && u4_base_len < sizeof(strBaseName))
	{
		memcpy(strBaseName, pstr_base, u4_base_len);
		strBaseName[u4_base_len] = 0;
	}
}


//**********************************************************************************************
// Scan the override directory (override\<base name>\) for loose files that replace samples
// from this packed audio file. File names are hashed with the same function the game uses
// for sample names (sndhndHashIdentifier), so "BIRD 07.cau" overrides the packed sample
// whose identifier was hashed from "BIRD 07".
//
void CAudioDatabase::ScanOverrideDirectory
(
)
//**************************************
{
	if (strBaseName[0] == 0)
	{
		return;
	}

	char			asz_pattern[_MAX_PATH];
	WIN32_FIND_DATA	ffd;
	HANDLE			h_find;
	const char*		asz_exts[4] = { "*.cau", "*.wav", "*.adx", "*.srt" };
	uint32			u4;

	for (u4 = 0; u4 < 4; u4++)
	{
		wsprintf(asz_pattern, "override\\%s\\%s", strBaseName, asz_exts[u4]);

		h_find = FindFirstFile(asz_pattern, &ffd);

		if (h_find == INVALID_HANDLE_VALUE)
		{
			continue;
		}

		do
		{
			// skip directories
			if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			{
				continue;
			}

			// strip the extension to get the sample name, hash it (the hash function
			// lowercases the name itself).
			char	asz_name[_MAX_PATH];
			strcpy(asz_name, ffd.cFileName);

			char*	pstr_dot = strrchr(asz_name, '.');

			if (pstr_dot)
			{
				*pstr_dot = 0;
			}

			uint32	u4_hash = sndhndHashIdentifier(asz_name);

			if (u4_hash == 0)
			{
				continue;
			}

			// build the full relative path of the override file
			char*	pstr_path = new char[_MAX_PATH];
			wsprintf(pstr_path, "override\\%s\\%s", strBaseName, ffd.cFileName);

			if (u4 == 3)
			{
				ohSrtOverrides[u4_hash] = pstr_path;
			}
			else
			{
				ohOverrides[u4_hash] = pstr_path;
			}
		}
		while (FindNextFile(h_find, &ffd));

		FindClose(h_find);
	}
}


//**********************************************************************************************
// Reset the audio database, most importantly reset the collision time.
//
void CAudioDatabase::Reset
(
)
//**************************************
{
	// No collision map then do nothing
	if (pchCollisions == NULL)
		return;

	// go through the map and reset all the last time used elememts
	for (TCollisionHash::iterator i = pchCollisions->begin(); i!=pchCollisions->end(); ++i)
	{
		(*i).second->fTimeLastUsed = 0.0f;
	}
}
