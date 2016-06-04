#ifndef _ERROR_CODES_H_
#define _ERROR_CODES_H_

enum EGlobalError
{
	kDeltaDecodeError = -100,
	kNotDecompressed,
	kInvalidFile,
	kFilePart,
	kUnableToCreateDir,
	kInsufficientMemory,
	kEmptyFile,
	kWriteError = -3,
	kReadError = -2,
	kEndOfFile = -1,
	kSuccess = 1
};

enum EDecodeError
{
	kInvalidSource = -300,
	kInvalidDeltaFile,
	kSourceError
};

enum EDecompressError
{
	kDeflateError = -200,
	kInflateError
};

static const char * ErrorMessage(int iErrorCode)
{
	switch (iErrorCode)
	{
	case kInvalidSource:
		return "Invalid source";

	case kInvalidFile:
		return "Invalid file";

	case kInvalidDeltaFile:
		return "Invalid delta decoded file";

	case kDeltaDecodeError:
		return "Delta decode error";

	case kDeflateError:
		return "Deflate error";

	case kInflateError:
		return "Inflate error";

	case kSourceError:
		return "Source error";

	case kNotDecompressed:
		return "File not decompressed";

	case kUnableToCreateDir:
		return "Could not create directory";

	case kWriteError:
		return "Could not write data to file";

	case kReadError:
		return "Could not read data from file";

	case kEndOfFile:
		return "Reaced end of file";

	case kInsufficientMemory:
		return "Not enough memory";

	case kEmptyFile:
		return "File is empty";

	case kFilePart:
		return "File uncomplete";

	case kSuccess:
		return "No error";
	};

	return "Unknown error";
}

#endif