
/*
        REX Client Test
        Copyright Reason Studios AB.
*/
#include <assert.h>
#include <errno.h>
#include <memory.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "REX.h"
#include "Wav.h"
#include "argparse.h"

static const char *const usages[] = {
    "basic [options] [[--] args]",
    "basic [options]",
    NULL,
};

#define PERM_READ (1 << 0)
#define PERM_WRITE (1 << 1)
#define PERM_EXEC (1 << 2)

#if REX_DLL_LOADER
#if REX_WINDOWS
#include <shlwapi.h>
#include <tlhelp32.h>
#include <windows.h>
#endif
#if REX_MAC && REX_DLL_LOADER
#include <unistd.h>
#endif
#endif // #if REX_DLL_LOADER

/*
        Constants
*/

#define kFileName "120Stereo.rx2"
#define kOutputSampleRate 44100

#define kREXErrorTextMaxLen 255

/*
        Function prototypes
*/

void GetErrorText(REXError error, char *text, REX_int32_t size);
void PrintREXError(REXError error);
REX_int32_t ScaleSampleLength(REX_int32_t inSampleRate,
                              REX_int32_t sampleLength);

REXError ExtractAllSlices(REXHandle handle, int outputSampleRate,
                          char *outputDir, const char *inputFilePath);
REXError ExtractAsWholeFileWithCues(REXHandle handle, int outputSampleRate,
                                    const char *outputPath);
void WriteCueFile(REXHandle handle, const char *cueFilePath);
REXError PreviewRenderInTempo(REXHandle handle, REX_int32_t tempo,
                              REX_int32_t frameRate, char *outputDir);

REXCallbackResult MyCallback(REX_int32_t percentFinished, void *userData);
int Test(char *outputPath, char *filepath, int outputSampleRate,
         int extractSlicesSeparately, int writeCueFiles);

#if REX_DLL_LOADER

#if REX_WINDOWS

/*
        JE: WinGetLoadingModuleDirAbsPath() is an example of how you can find
   the file system path to the running test app on Windows. Only use this if
   your application / plugin module can't otherwise know where it is installed.
        Depends on tlhelp32.dll
*/

static int WinGetLoadingModuleDirAbsPath(wchar_t *iBuf, size_t iBufSize) {
  int r = 0;
  void *soughtAddress = (void *)&WinGetLoadingModuleDirAbsPath;
  HANDLE hsnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, 0);
  if (hsnap == NULL) {
    return r;
  }

  HMODULE thisModule = NULL;

  MODULEENTRY32W moduleInfo;
  memset(&moduleInfo, 0, sizeof(moduleInfo));
  moduleInfo.dwSize = sizeof(moduleInfo);
  if (Module32First(hsnap, &moduleInfo)) {
    do {
      void *startAddr = moduleInfo.modBaseAddr;
      void *endAddr = ((char *)startAddr) + moduleInfo.modBaseSize;
      if ((startAddr <= soughtAddress) && (soughtAddress < endAddr)) {
        thisModule = moduleInfo.hModule;
        break;
      }
    } while (Module32Next(hsnap, &moduleInfo));
  }
  CloseHandle(hsnap);

  if (thisModule == 0) {
    return r;
  }

  SetLastError(NO_ERROR);
  DWORD len = GetModuleFileNameW(thisModule, iBuf, (DWORD)iBufSize);
  if ((len >= iBufSize) || (GetLastError() != NO_ERROR)) {
    return r;
  }

  if (0 != PathRemoveFileSpecW(iBuf)) {
    r = 1;
  }
  return r;
}

static int GetBaseDirPath(wchar_t *iBuf, size_t iBufSize) {
  return WinGetLoadingModuleDirAbsPath(iBuf, iBufSize);
}

#else // #if REX_WINDOWS

// ??? JE: This is not considered an example - don't use this code in your
// plugin host or plugin. Uses current directory to find REX dylib bundle. Can
// be OK to use in a command line test app like this, which is launched manually
// / always from the same current directory.

static int GetBaseDirPath(char *iBufUTF8, size_t iBufSize) {
  int r = 0;
  char *p = getcwd(iBufUTF8, iBufSize);
  if (p != NULL) {
    r = 1;
  }
  return r;
}

#endif // #else // #if REX_WINDOWS

#endif // #if REX_DLL_LOADER

// ### FL: Should add a cpp test file also? But same tests should run for
//	REX_DLL_BUILD on and off, right?
//	Move actual tests to shared file and have different main (c/cpp) for DLL
// and C++?

int Test(char *outputPath, char *filepath, int outputSampleRate,
         int extractSlicesSeparately, int writeCueFiles) {
  REXError result;
  REXHandle handle = 0;
  REXInfo info;
  size_t fileSize = 0;
  char *fileBuffer = 0;

  int returnCode = -1;

#if REX_DLL_LOADER

  /*
          JE: Find the REX module (DLL and dylib bundle) by knowing the path
     where the REX client application / plugin module executable binary is
     installed, then find the REX module relative to that.
  */

#if REX_WINDOWS
  wchar_t pathBuf[1024] = {0};
#else
  char pathBuf[1024] = {0};
#endif
  if (!GetBaseDirPath(&pathBuf[0], 1024)) {
    printf("Error: Failed to get the path to the base directory!\n");
    return -1;
  }

  result = REXInitializeDLL_DirPath(&pathBuf[0]);
  if (result != kREXError_NoError) {
    PrintREXError(result);
    printf("Exiting\n");
    return -1;
  }
#else
  // JE: Initialize REX framework bundle (Mac) or auto-generated link library
  // (Win - test purposes only - see comments in REX.h)
  result = REXInitializeDLL();
#endif // #if REX_DLL_LOADER

  if (result != kREXError_NoError) {
    PrintREXError(result);
    printf("Exiting\n");
    return -1;
  }

  /*
          Load the file into a buffer.
  */
  {
    FILE *inputFile = 0;
    size_t bytesRead = 0;

    inputFile = fopen(filepath, "rb");
    if (inputFile == 0) {
      printf("Unable to open file \"%s\", Exiting\n", filepath);
      REXUninitializeDLL();
      return -1;
    }

    /* Find out file size. */
    if (fseek(inputFile, 0, SEEK_END) != 0) {
      printf("Unable to seek end of file \"%s\", Exiting\n", filepath);
      fclose(inputFile);
      REXUninitializeDLL();
      return -1;
    }
    fileSize = ftell(inputFile);
    rewind(inputFile);

    /* Allocate buffer for file and read file into it. */
    fileBuffer = malloc(fileSize * sizeof(char));
    if (fileBuffer == 0) {
      printf("Out of memory, Exiting\n");
      fclose(inputFile);
      REXUninitializeDLL();
      return -1;
    }

    bytesRead = fread(fileBuffer, sizeof(char), fileSize, inputFile);
    if (bytesRead != fileSize) {
      printf("Unable to read file \"%s\", Exiting\n", filepath);
      free(fileBuffer);
      fclose(inputFile);
      REXUninitializeDLL();
      return -1;
    }

    fclose(inputFile);
  }

  /*
          Create REX object from buffered file.
  */
  result = REXCreate(&handle, fileBuffer, (REX_int32_t)(fileSize), MyCallback,
                     filepath);
  if (result == kREXError_NoError) {

    /*
            Set output sample rate for this REX object.
    */
    result = REXSetOutputSampleRate(handle, outputSampleRate);
    if (result == kREXError_NoError) {

      /*
              Get file info for processing
      */
      result = REXGetInfo(handle, (REX_int32_t)sizeof(REXInfo), &info);
      if (result == kREXError_NoError) {

        /*
                Extract REX file - either as whole file with cues or as separate
           slices
        */
        if (extractSlicesSeparately) {
          result =
              ExtractAllSlices(handle, outputSampleRate, outputPath, filepath);
          if (result == kREXError_NoError) {
            printf(" ✓ %d slices\n", info.fSliceCount);
            returnCode = 0;
          } else {
            PrintREXError(result);
          }
        } else {
          result =
              ExtractAsWholeFileWithCues(handle, outputSampleRate, outputPath);
          if (result == kREXError_NoError) {
            /* Optionally export .cue file with slice data */
            if (writeCueFiles) {
              char cueFilePath[1024];
              const char *dot = strrchr(outputPath, '.');
              if (dot) {
                size_t baseLen = dot - outputPath;
                if (baseLen >= sizeof(cueFilePath) - 5) {
                  baseLen = sizeof(cueFilePath) - 5;
                }
                memcpy(cueFilePath, outputPath, baseLen);
                strcpy(cueFilePath + baseLen, ".cue");
              } else {
                snprintf(cueFilePath, sizeof(cueFilePath), "%s.cue",
                         outputPath);
              }
              WriteCueFile(handle, cueFilePath);
            }
            printf(" ✓ %d slices\n", info.fSliceCount);
            returnCode = 0;
          } else {
            PrintREXError(result);
          }
        }

      } else {
        PrintREXError(result);
      }
    } else {
      PrintREXError(result);
    }

    /*
            Free all resources used by the REX object.
    */
    REXDelete(&handle);

  } else {
    PrintREXError(result);
  }

  free(fileBuffer);
  REXUninitializeDLL();
  return returnCode;
}

/*
        MyCallback()

        User-callback to show progress during REXCreate().
*/

REXCallbackResult MyCallback(REX_int32_t percentFinished, void *userData) {
  /* Silent callback - no progress messages */
  (void)percentFinished;
  (void)userData;
  return kREXCallback_Continue;
}

/*
        GetErrorText()

        Gets textual description for a REXError.
*/

void GetErrorText(REXError error, char *text, REX_int32_t size) {
  assert(size >= kREXErrorTextMaxLen);

  switch (error) {
  /*
          Error codes that can be expected at run-time, but are not really
     errors.
  */
  case kREXError_NoError:
    sprintf(text, "%s",
            "No Error. (You shouldn't show an error text in this case!)");
    break;
  case kREXError_OperationAbortedByUser:
    sprintf(text, "%s",
            "Operation was aborted by user. (You shouldn't show an error text "
            "in this case!)");
    break;
  case kREXError_NoCreatorInfoAvailable:
    sprintf(text, "%s",
            "There is no optional creator-supplied information in this file. "
            "(You shouldn't show an error text in this case!)");
    break;

#if REX_DLL_LOADER
  case kREXError_NotEnoughMemoryForDLL:
  case kREXError_UnableToLoadDLL:
  case kREXError_DLLTooOld:
  case kREXError_DLLNotFound:
  case kREXError_APITooOld:
  case kREXError_OSVersionNotSupported:
    strcpy(text, "Loader error");
    break;
#endif
  /*
          Errors that could be expected at run-time.
  */
  case kREXError_OutOfMemory:
    sprintf(text, "%s",
            "The operation could not be completed because there is too little "
            "free memory. Quit some other application, then try again.");
    break;
  case kREXError_FileCorrupt:
    sprintf(text, "%s",
            "The format of this file is unknown or the file is corrupt.");
    break;
  case kREXError_REX2FileTooNew:
    sprintf(text, "%s",
            "This REX2 file was created by a later version of ReCycle and "
            "cannot be loaded with this version of the REX DLL. Please update "
            "the DLL, then try again.");
    break;
  case kREXError_FileHasZeroLoopLength:
    sprintf(text, "%s",
            "This ReCycle file cannot be used because its \"Bars\" and "
            "\"Beats\" settings have not been set.");
    break;

  /*
          Errors that won't happen at run-time, if your code complies to the REX
     API.
  */
  case kREXImplError_DLLNotInitialized:
    sprintf(text, "%s",
            "The REX DLL has not been initialized. You must call "
            "REXInitializeDLL() before any other function in the REX API.");
    break;
  case kREXImplError_DLLAlreadyInitialized:
    sprintf(text, "%s",
            "The REX DLL is already initialized. You may not nest calls to "
            "REXInitializeDLL().");
    break;
  case kREXImplError_InvalidHandle:
    sprintf(text, "%s",
            "Invalid handle. The handle you supplied to the function does not "
            "refer to a REX object currently in memory.");
    break;
  case kREXImplError_InvalidSize:
    sprintf(text, "%s",
            "Invalid size. The fSize field of the REXInfo or REXSliceInfo "
            "structure was not set up correctly. Please set the field to "
            "\"sizeof(REXInfo)\" or \"sizeof(REXSliceInfo)\" prior to the "
            "function call.");
    break;
  case kREXImplError_InvalidArgument:
    sprintf(text, "%s",
            "Invalid argument. One of the parameters you supplied is out of "
            "range. Please check the documentation on this function for more "
            "specific help.");
    break;
  case kREXImplError_InvalidSlice:
    sprintf(text, "%s",
            "Invalid slice. The slice number you have supplied is out of "
            "range. Check the fSliceCount member of the REXInfo structure for "
            "this REX object, to see how many slices this object has.");
    break;
  case kREXImplError_InvalidSampleRate:
    sprintf(text, "%s",
            "Invalid sample rate. The sample rate must be in the range 11025 "
            "Hz - 1000000 Hz, inclusive.");
    break;
  case kREXImplError_BufferTooSmall:
    sprintf(text, "%s",
            "The buffer you have supplied is too small for this slice. Check "
            "the documentation on REXRenderSlice() for an example on how to "
            "calculate the needed buffer size.");
    break;
  case kREXImplError_IsBeingPreviewed:
    sprintf(text, "%s",
            "This REX object is being previewed. You must stop the preview by "
            "calling REXStopPreview() before previewing can be restarted or "
            "before changing the output sample rate.");
    break;
  case kREXImplError_NotBeingPreviewed:
    sprintf(text, "%s",
            "This object is not currently being previewed. You must not call "
            "REXRenderBatch() or REXStopPreview() on a REX object unless "
            "preview has been started.");
    break;
  case kREXImplError_InvalidTempo:
    sprintf(text, "%s",
            "Invalid tempo. The preview tempo must be in the range 20 BPM - "
            "450 BPM, inclusive.");
    break;

  /*
          Errors that indicate problems in the API.
  */
  case kREXError_Undefined:
    sprintf(
        text, "%s",
        "Undefined error. If this happens, you've found a bug in the REX DLL.");
    break;
  default:
    assert(0);
    break;
  }
}

/*
        PrintREXError()

        Gets the error text for a REX error and prints it.
*/

void PrintREXError(REXError error) {
  char temp[kREXErrorTextMaxLen];
  GetErrorText(error, temp, kREXErrorTextMaxLen);
  printf("%s\n", temp);
}

/*
        ExtractAllSlices,

        Extracts all slices, one by one and write each of them to a file
*/

REXError ExtractAllSlices(REXHandle handle, int outputSampleRate,
                          char *outputDir, const char *inputFilePath) {
  REXError result;
  REXInfo info;
  char baseFileName[256];

  /* Extract base filename from input path */
  const char *lastSlash = strrchr(inputFilePath, '/');
  const char *baseName = lastSlash ? lastSlash + 1 : inputFilePath;

  /* Remove extension */
  const char *dot = strrchr(baseName, '.');
  if (dot) {
    size_t baseLen = dot - baseName;
    if (baseLen >= sizeof(baseFileName)) {
      baseLen = sizeof(baseFileName) - 1;
    }
    memcpy(baseFileName, baseName, baseLen);
    baseFileName[baseLen] = '\0';
  } else {
    snprintf(baseFileName, sizeof(baseFileName), "%s", baseName);
  }
  REXSliceInfo sliceInfo;
  float *renderBuffers[2];
  REX_int32_t sliceIndex = 0;

  /*
          Get info on entire REX object.
  */
  result = REXGetInfo(handle, (REX_int32_t)sizeof(REXInfo), &info);
  if (result != kREXError_NoError) {
    return result;
  }

  /*
          Enumerate all slices in REX object, render and save each one of them
     to a file.
  */
  for (sliceIndex = 0; sliceIndex < info.fSliceCount; sliceIndex++) {
    float *sliceSamples = 0;

    /*
            Get information about this slice, then calculate the length of this
       slice at the output sample rate.
    */
    result = REXGetSliceInfo(handle, sliceIndex,
                             (REX_int32_t)sizeof(REXSliceInfo), &sliceInfo);
    if (result != kREXError_NoError) {
      return result;
    }

    /*
            Allocate one chunk of memory for *all* channels in the REX file,
       then set up the pointers in the renderBuffers array, depending on if the
       file is mono or stereo. Note that "sliceSamples" is the buffer that is
       allocated/deallocated, not "renderBuffers".
    */
    sliceSamples =
        malloc(info.fChannels * sliceInfo.fSampleLength * sizeof(float));
    if (sliceSamples == 0) {
      printf("Malloc failed, Exiting\n");
      return kREXError_OutOfMemory;
    }

    /* First pointer is always set up. */
    renderBuffers[0] = &sliceSamples[0];

    /* Second pointer is only set to something other than zero, if we have a
     * stereo file. */
    if (info.fChannels == 2) {
      renderBuffers[1] = &sliceSamples[sliceInfo.fSampleLength];
    } else {
      assert(info.fChannels == 1);
      renderBuffers[1] = 0;
    }

    /*
            Render this slice into the allocated buffer.
    */
    result = REXRenderSlice(handle, sliceIndex, sliceInfo.fSampleLength,
                            renderBuffers);
    if (result != kREXError_NoError) {
      free(sliceSamples);
      return result;
    }

    /*
            Save the rendered data to a .WAV-file and samples in a plain text
       file.
    */
    {
      FILE *outputFile = 0;
      FILE *outputFileTxt = 0;
      char fileName[255];
      char fileNameTxt[255];

      sprintf(fileName, "%s/%s_slice%03d.wav", outputDir, baseFileName,
              sliceIndex + 1);
      sprintf(fileNameTxt, "%s/%s_slice%03d.txt", outputDir, baseFileName,
              sliceIndex + 1);
      outputFile = fopen(fileName, "wb");
      outputFileTxt = fopen(fileNameTxt, "w");
      if (outputFile != 0 && outputFileTxt != 0) {
        WriteWave(outputFile, sliceInfo.fSampleLength, info.fChannels, 16,
                  outputSampleRate, renderBuffers);
        WriteTxt(outputFileTxt, sliceInfo.fSampleLength, info.fChannels,
                 outputSampleRate, renderBuffers);
        fclose(outputFile);
        fclose(outputFileTxt);
      } else {
        printf("Unable to open file \"%s\"\n", fileName);
      }
    }

    /*
            Free render buffer.
    */
    free(sliceSamples);
    sliceSamples = 0;
  }

  return kREXError_NoError;
}

/*
        ExtractAsWholeFileWithCues()
        Renders the entire REX file as one continuous WAV file with cue markers
   at each slice position.
*/
REXError ExtractAsWholeFileWithCues(REXHandle handle, int outputSampleRate,
                                    const char *outputPath) {
  REXError result;
  REXInfo info;
  REXSliceInfo sliceInfo;
  float *renderBuffers[2];
  REX_int32_t sliceIndex = 0;
  uint32_t totalSamples = 0;
  uint32_t currentPosition = 0;
  WaveCuePoint *cuePoints = NULL;
  float *fullAudioLeft = NULL;
  float *fullAudioRight = NULL;
  FILE *outputFile = NULL;

  /* Get info on entire REX object */
  result = REXGetInfo(handle, (REX_int32_t)sizeof(REXInfo), &info);
  if (result != kREXError_NoError) {
    return result;
  }

  /* Calculate total sample count needed */
  for (sliceIndex = 0; sliceIndex < info.fSliceCount; sliceIndex++) {
    result = REXGetSliceInfo(handle, sliceIndex,
                             (REX_int32_t)sizeof(REXSliceInfo), &sliceInfo);
    if (result != kREXError_NoError) {
      return result;
    }
    totalSamples += sliceInfo.fSampleLength;
  }

  /* Allocate memory for the full audio */
  fullAudioLeft = malloc(totalSamples * sizeof(float));
  if (fullAudioLeft == NULL) {
    printf("Malloc failed for audio buffer\n");
    return kREXError_OutOfMemory;
  }

  if (info.fChannels == 2) {
    fullAudioRight = malloc(totalSamples * sizeof(float));
    if (fullAudioRight == NULL) {
      printf("Malloc failed for right channel buffer\n");
      free(fullAudioLeft);
      return kREXError_OutOfMemory;
    }
  }

  /* Allocate memory for cue points */
  cuePoints = malloc(info.fSliceCount * sizeof(WaveCuePoint));
  if (cuePoints == NULL) {
    printf("Malloc failed for cue points\n");
    free(fullAudioLeft);
    if (fullAudioRight)
      free(fullAudioRight);
    return kREXError_OutOfMemory;
  }

  /* Render all slices into the full audio buffer and record cue points */
  currentPosition = 0;
  for (sliceIndex = 0; sliceIndex < info.fSliceCount; sliceIndex++) {
    float *sliceSamples = NULL;

    /* Get slice info */
    result = REXGetSliceInfo(handle, sliceIndex,
                             (REX_int32_t)sizeof(REXSliceInfo), &sliceInfo);
    if (result != kREXError_NoError) {
      free(fullAudioLeft);
      if (fullAudioRight)
        free(fullAudioRight);
      free(cuePoints);
      return result;
    }

    /* Record cue point at the start of this slice */
    cuePoints[sliceIndex].position = currentPosition;
    snprintf(cuePoints[sliceIndex].label, sizeof(cuePoints[sliceIndex].label),
             "Slice %03d", sliceIndex + 1);

    /* Allocate temporary buffer for this slice */
    sliceSamples =
        malloc(info.fChannels * sliceInfo.fSampleLength * sizeof(float));
    if (sliceSamples == NULL) {
      printf("Malloc failed for slice buffer\n");
      free(fullAudioLeft);
      if (fullAudioRight)
        free(fullAudioRight);
      free(cuePoints);
      return kREXError_OutOfMemory;
    }

    /* Set up render buffers for this slice */
    renderBuffers[0] = &sliceSamples[0];
    if (info.fChannels == 2) {
      renderBuffers[1] = &sliceSamples[sliceInfo.fSampleLength];
    } else {
      renderBuffers[1] = NULL;
    }

    /* Render this slice */
    result = REXRenderSlice(handle, sliceIndex, sliceInfo.fSampleLength,
                            renderBuffers);
    if (result != kREXError_NoError) {
      free(sliceSamples);
      free(fullAudioLeft);
      if (fullAudioRight)
        free(fullAudioRight);
      free(cuePoints);
      return result;
    }

    /* Copy this slice into the full audio buffer */
    memcpy(&fullAudioLeft[currentPosition], renderBuffers[0],
           sliceInfo.fSampleLength * sizeof(float));
    if (info.fChannels == 2) {
      memcpy(&fullAudioRight[currentPosition], renderBuffers[1],
             sliceInfo.fSampleLength * sizeof(float));
    }

    currentPosition += sliceInfo.fSampleLength;
    free(sliceSamples);
  }

  /* Write the complete WAV file with cue markers */
  outputFile = fopen(outputPath, "wb");
  if (outputFile != NULL) {
    float *finalBuffers[2];
    finalBuffers[0] = fullAudioLeft;
    finalBuffers[1] = fullAudioRight;

    WriteWaveWithCues(outputFile, totalSamples, info.fChannels, 16,
                      outputSampleRate, finalBuffers, cuePoints,
                      info.fSliceCount);
    fclose(outputFile);
  } else {
    printf("  ✗ Unable to open file \"%s\" for writing\n", outputPath);
    result = kREXError_OutOfMemory; /* Using this as a generic error */
  }

  /* Clean up */
  free(fullAudioLeft);
  if (fullAudioRight)
    free(fullAudioRight);
  free(cuePoints);

  return kREXError_NoError;
}

/*
        WriteCueFile()
        Writes slice information to a CDRWIN .cue sheet file
*/
void WriteCueFile(REXHandle handle, const char *cueFilePath) {
  REXError result;
  REXInfo info;
  REXSliceInfo sliceInfo;
  FILE *cueFile = NULL;
  uint32_t currentPosition = 0;

  /* Get info on entire REX object */
  result = REXGetInfo(handle, (REX_int32_t)sizeof(REXInfo), &info);
  if (result != kREXError_NoError) {
    printf("Error getting REX info for cue file\n");
    return;
  }

  /* Open cue file for writing */
  cueFile = fopen(cueFilePath, "w");
  if (cueFile == NULL) {
    printf("Unable to create cue file: %s\n", cueFilePath);
    return;
  }

  /* Get the WAV filename from the cue file path */
  const char *lastSlash = strrchr(cueFilePath, '/');
  const char *baseFileName = lastSlash ? lastSlash + 1 : cueFilePath;
  char wavFileName[512];

  /* Replace .cue with .wav */
  const char *dot = strrchr(baseFileName, '.');
  if (dot) {
    size_t baseLen = dot - baseFileName;
    if (baseLen >= sizeof(wavFileName) - 5) {
      baseLen = sizeof(wavFileName) - 5;
    }
    memcpy(wavFileName, baseFileName, baseLen);
    strcpy(wavFileName + baseLen, ".wav");
  } else {
    snprintf(wavFileName, sizeof(wavFileName), "%s.wav", baseFileName);
  }

  /* Write CDRWIN cue sheet header */
  fprintf(cueFile, "REM Generated by rex2wav\n");
  fprintf(cueFile, "REM Original Tempo: %.2f BPM\n",
          (float)info.fTempo / 1000.0f);
  fprintf(cueFile, "REM Time Signature: %d/%d\n", info.fTimeSignNom,
          info.fTimeSignDenom);
  fprintf(cueFile, "FILE \"%s\" WAVE\n", wavFileName);

  /* Write track for each slice */
  for (REX_int32_t sliceIndex = 0; sliceIndex < info.fSliceCount;
       sliceIndex++) {
    result = REXGetSliceInfo(handle, sliceIndex,
                             (REX_int32_t)sizeof(REXSliceInfo), &sliceInfo);
    if (result != kREXError_NoError) {
      continue;
    }

    /* Calculate time position in MM:SS:FF format (frames = 75 per second) */
    uint32_t startSample = currentPosition;
    float startTimeSec = (float)startSample / (float)info.fSampleRate;

    int minutes = (int)(startTimeSec / 60.0f);
    int seconds = (int)startTimeSec % 60;
    int frames =
        (int)((startTimeSec - (float)(minutes * 60 + seconds)) * 75.0f);

    fprintf(cueFile, "  TRACK %02d AUDIO\n", sliceIndex + 1);
    fprintf(cueFile, "    TITLE \"Slice %02d\"\n", sliceIndex + 1);
    fprintf(cueFile, "    INDEX 01 %02d:%02d:%02d\n", minutes, seconds, frames);

    currentPosition += sliceInfo.fSampleLength;
  }

  fclose(cueFile);
}

REXError PreviewRenderInTempo(REXHandle handle, REX_int32_t tempo,
                              REX_int32_t frameRate, char *outputDir) {
  REXError result;
  REXInfo info;
  float *renderSamples = NULL;
  float *renderBuffers[2]; /* = {NULL,NULL};*/
  REX_int32_t lengthFrames = 0;
  REX_int32_t framesRendered = 0;

  result = REXGetInfo(handle, (REX_int32_t)sizeof(REXInfo), &info);
  if (result != kREXError_NoError) {
    return result;
  }

  /* Calculate length in frames of preview rendered loop */

  {
    double tmp = frameRate;
    tmp *= 1000.0;
    tmp *= info.fPPQLength;
    tmp /= (tempo * 256);
    lengthFrames = (REX_int32_t)tmp;
  }

  /*
          Allocate one chunk of memory for *all* channels in the REX file, then
     set up the pointers in the renderBuffers array, depending on if the file is
     mono or stereo. Note that "sliceSamples" is the buffer that is
     allocated/deallocated, not "renderBuffers".
  */
  renderSamples = malloc(info.fChannels * lengthFrames * sizeof(float));
  if (renderSamples == 0) {
    printf("Malloc failed, Exiting\n");
    return kREXError_OutOfMemory;
  }

  /* First pointer is always set up. */
  renderBuffers[0] = &renderSamples[0];

  /* Second pointer is only set to something other than zero, if we have a
   * stereo file. */
  if (info.fChannels == 2) {
    renderBuffers[1] = &renderSamples[lengthFrames];
  } else {
    assert(info.fChannels == 1);
    renderBuffers[1] = 0;
  }

  result = REXSetPreviewTempo(handle, tempo);
  if (result != kREXError_NoError) {
    printf("REXSetPreviewTempo failed, exiting\n");
    goto done;
  }

  result = REXStartPreview(handle);
  if (result != kREXError_NoError) {
    printf("REXStartPreview failed, exiting\n");
    goto done;
  }

  while (framesRendered != lengthFrames) {
    REX_int32_t remaining = lengthFrames - framesRendered;
    REX_int32_t todo = remaining;
    float *tmpRenderBuffers[2] = {NULL, NULL};

    if (todo > 64) {
      todo = 64;
    }

    tmpRenderBuffers[0] = renderBuffers[0] + framesRendered;
    if (renderBuffers[1] != NULL) {
      tmpRenderBuffers[1] = renderBuffers[1] + framesRendered;
    }

    result = REXRenderPreviewBatch(handle, todo, tmpRenderBuffers);
    if (result != kREXError_NoError) {
      printf("REXRenderPreviewBatch failed, exiting\n");
      goto done;
    }

    framesRendered += todo;
  }

  result = REXStopPreview(handle);
  if (result != kREXError_NoError) {
    printf("REXStopPreview failed, exiting\n");
    goto done;
  }
  {
    float left[64];
    float right[64];
    float *tmpRenderBuffers[2];
    tmpRenderBuffers[0] = &left[0];
    tmpRenderBuffers[1] = &right[0];
    result = REXRenderPreviewBatch(handle, 64, tmpRenderBuffers);
  }
  if (result != kREXError_NoError) {
    printf("REXRenderPreviewBatch failed, exiting\n");
    goto done;
  }

  /*
          Save the rendered data to a .WAV-file and samples in a plain text
     file.
  */
  {
    FILE *outputFile = 0;
    FILE *outputFileTxt = 0;
    char fileName[255];
    char fileNameTxt[255];

    sprintf(fileName, "%s/PreviewRender_Tempo%06d.wav", outputDir, tempo);
    sprintf(fileNameTxt, "%s/PreviewRender_Tempo%06d.txt", outputDir, tempo);
    outputFile = fopen(fileName, "wb");
    outputFileTxt = fopen(fileNameTxt, "w");
    if (outputFile != 0 && outputFileTxt != 0) {
      WriteWave(outputFile, lengthFrames, info.fChannels, 16, frameRate,
                renderBuffers);
      WriteTxt(outputFileTxt, lengthFrames, info.fChannels, frameRate,
               renderBuffers);
      fclose(outputFile);
      fclose(outputFileTxt);
    } else {
      printf("Unable to open file \"%s\"\n", fileName);
      result = kREXError_Undefined;
    }
  }

  result = kREXError_NoError;

done:
  free(renderSamples);
  renderSamples = NULL;

  return result;
}

int main(int argc, char *argv[]) {
  // Init args
  char *list = NULL;
  char *input_folder = NULL;
  char *input_file = NULL;
  bool recursive = false;
  int slice_limit = 0;
  int bit_rate = 24;
  int sample_rate = 44100;
  char *output_file = NULL;
  char *output_folder = NULL;

  struct argparse_option options[] = {
      OPT_HELP(),
      OPT_GROUP("Input Source."),
      OPT_STRING('l', "list", &list, "Plaintext list of files to process.",
                 NULL, 0, 0),
      OPT_STRING('d', "input_folder", &input_folder,
                 "Input folder of files to process", NULL, 0, 0),
      OPT_BOOLEAN(
          'r', "recursive", &recursive,
          "If input folder is set, recursively search for Recycle files.", NULL,
          0, 0),
      OPT_STRING('i', "input_file", &input_file, "Input file.", NULL, 0, 0),
      OPT_GROUP("Output options."),
      OPT_STRING('o', "output_file", &output_file, "Output File.", NULL, 0, 0),
      OPT_STRING('f', "output_folder", &output_folder,
                 "Output directory for converted files.", NULL, 0, 0),
      OPT_INTEGER('s', "Output Sample Rate", &sample_rate,
                  "Output sample rate. From 11.025kHz to 1mHz.", NULL, 0, 0),
      OPT_INTEGER(
          'b', "bit_rate", &bit_rate,
          "Sets output sample bitrate. Only values of 8, 16, and 24 accepted.",
          NULL, 0, 0),
      OPT_INTEGER(
          'm', "max_slice_limit", &slice_limit,
          "Maximum number of slices per output file.  Will cut in half at "
          "nearest slice marker until all sections are under the limit.\n"
          "Set to 0 to render single file complete with all slice points.\n"
          "Set to 1 to split all slices into individual files.",
          NULL, 0, 0),
      OPT_END(),
  };

  struct argparse argparse;
  argparse_init(&argparse, options, usages, 0);
  argparse_describe(
      &argparse, "\n Convert REX2 files into WAV with cue markers for slices.",
      "\n More hardware formats coming soon...");
  argc = argparse_parse(&argparse, argc, argv);

  // Argument sanity checks
  if (!list && !input_folder && !input_file) {
    printf("\nAt least one input source must be declared via --list, "
           "--input_folder, or --input_file.\n");
    exit(1);
  }
  if ((list || input_folder) && (!output_folder)) {
    printf(
        "\nAn --output_folder must be used with --list or --input_folder.\n");
    exit(1);
  }
  if ((list || input_folder) && input_file) {
    printf("\nCannot use --input_file along with either --list or "
           "--input_folder\n");
    exit(1);
  }
  if ((list || input_folder) && output_file) {
    printf("\nCannot specify output file if using --list or --input_folder as "
           "input sources.\n");
    exit(1);
  }
  if (slice_limit < 0) {
    printf("\n--max_slices must be integer 0 or greater.");
    exit(1);
  }
  if (bit_rate != 8 || bit_rate != 16 || bit_rate != 24) {
    printf("\n--bit_rate must be either 8, 16, or 24.");
    exit(1);
  }
  if (recursive && input_file) {
    printf("\n--recursive cannot be used when using an --input_file.\n");
    exit(1);
  }
}
